"""External, bounded 1S cell workflow. Instrument safeguards remain on each node."""

from __future__ import annotations
import csv
from concurrent.futures import ThreadPoolExecutor
import dataclasses
import datetime
import json
import math
import time
import uuid
from pathlib import Path
from labbridge_generated.models.current_request import CurrentRequest
from labbridge_generated.models.voltage_request import VoltageRequest
from labbridge_generated.models.output_request import OutputRequest
from labbridge_generated.models.dl24m_settings import Dl24mSettings
from labbridge_generated.models.switch_path_request import SwitchPathRequest


@dataclasses.dataclass(frozen=True)
class Profile:
    name: str
    chemistry: str
    series_count: int
    maximum_voltage_v: float
    maximum_current_a: float
    charge_voltage_v: float
    charge_current_a: float
    termination_current_a: float
    termination_hold_s: float
    charge_timeout_s: float
    discharge_current_a: float
    discharge_cutoff_v: float
    discharge_timeout_s: float
    minimum_temperature_c: float
    maximum_temperature_c: float
    rest_s: float
    pulse_current_a: float
    pulse_s: float
    pulse_rest_s: float
    pulse_count: int
    cycles: int
    max_power_w: float
    sample_interval_s: float
    max_sample_gap_s: float

    def validate(self):
        if (
            self.chemistry not in ("li-ion", "lifepo4")
            or type(self.series_count) is not int
            or self.series_count != 1
        ):
            raise ValueError(
                "Only explicit 1S li-ion and lifepo4 profiles are supported"
            )
        for f in dataclasses.fields(self):
            value = getattr(self, f.name)
            if (
                f.type != "str"
                and isinstance(value, (int, float))
                and (not math.isfinite(value) or value < 0)
            ):
                if f.name != "minimum_temperature_c":
                    raise ValueError(f"Invalid {f.name}")
        ceiling = 4.2 if self.chemistry == "li-ion" else 3.65
        floor = 2.5 if self.chemistry == "li-ion" else 2.0
        if (
            not floor
            <= self.discharge_cutoff_v
            < self.charge_voltage_v
            <= self.maximum_voltage_v
            <= ceiling
        ):
            raise ValueError("Voltage limits exceed supported chemistry bounds")
        if (
            not 0 < self.termination_current_a < self.charge_current_a
            or not 0 < self.discharge_current_a
        ):
            raise ValueError("Invalid charge/termination/discharge current")
        if self.maximum_current_a < max(
            self.charge_current_a, self.discharge_current_a, self.pulse_current_a
        ):
            raise ValueError("Safety current must cover all requested currents")
        if (
            self.pulse_s < 1
            or max(self.charge_timeout_s, self.discharge_timeout_s, self.pulse_s)
            + self.max_sample_gap_s
            > 86400
        ):
            raise ValueError(
                "Use pulses of at least 1 second and phase caps below 24 hours"
            )
        if not -20 <= self.minimum_temperature_c < self.maximum_temperature_c <= 60:
            raise ValueError("Invalid cell temperature range")
        if (
            not 0.05 <= self.sample_interval_s <= 1
            or not self.sample_interval_s < self.max_sample_gap_s <= 3
        ):
            raise ValueError("Invalid sample timing")
        if (
            not 1 <= self.cycles <= 100
            or not isinstance(self.cycles, int)
            or not isinstance(self.pulse_count, int)
            or not 0 <= self.pulse_count <= 100
        ):
            raise ValueError("Cycle/pulse counts must be bounded integers")
        if (
            min(
                self.charge_timeout_s,
                self.discharge_timeout_s,
                self.termination_hold_s,
                self.pulse_s,
            )
            <= 0
        ):
            raise ValueError("Durations must be positive")
        if (
            self.charge_voltage_v
            * max(self.charge_current_a, self.discharge_current_a, self.pulse_current_a)
            > self.max_power_w
        ):
            raise ValueError("Profile power limit is too low")
        if (
            abs(self.discharge_cutoff_v * 10 - round(self.discharge_cutoff_v * 10))
            > 1e-6
        ):
            raise ValueError("DL24M cutoff requires 0.1 V steps")


class Integrator:
    def __init__(self, max_gap_s):
        self.max_gap_s = max_gap_s
        self.previous = None
        self.ah = self.wh = 0.0
        self.accepted_s = self.excluded_s = 0.0

    def add(self, t, v, a, valid, sequence, stage):
        if not valid or not all(math.isfinite(x) for x in (t, v, a)) or v < 0 or a < 0:
            self.previous = None
            return
        current = (t, v, a, sequence, stage)
        if self.previous:
            pt, pv, pa, ps, pstage = self.previous
            dt = t - pt
            if 0 < dt <= self.max_gap_s and sequence != ps and stage == pstage:
                self.ah += (pa + a) * dt / 7200
                self.wh += (pv * pa + v * a) * dt / 7200
                self.accepted_s += dt
            elif dt > 0:
                self.excluded_s += dt
        self.previous = current


@dataclasses.dataclass(frozen=True)
class Ref:
    node: str
    device: str


class Runner:
    def __init__(
        self,
        cluster,
        bindings,
        profile,
        output_dir,
        *,
        clock=time.monotonic,
        sleep=time.sleep,
    ):
        profile.validate()
        self.cluster, self.profile = cluster, profile
        self.refs = {
            k: Ref(**v)
            for k, v in bindings.items()
            if k in ("source", "load", "interlock")
        }
        if set(self.refs) != set(("source", "load", "interlock")):
            raise ValueError("Source, load and interlock bindings required")
        if len(set(self.refs.values())) != 3:
            raise ValueError("Roles must use distinct devices")
        self.probe = bindings["cell_probe"]
        self.clock, self.sleep = clock, sleep
        self.directory = Path(output_dir)
        self.directory.mkdir(parents=True, exist_ok=False)
        self.start = clock()
        self.integrators = {}
        self.metadata = {
            "profile": dataclasses.asdict(profile),
            "bindings": bindings,
            "started_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "nodes": {},
            "status": "preflight",
        }
        self.original = {}
        self.session = None
        self.writer = None

    def node(self, role):
        return self.cluster.nodes[self.refs[role].node]

    def device(self, role):
        return self.refs[role].device

    def arguments(self, role):
        return dict(
            device_id=self.device(role),
            x_lab_lease=self.session.token(self.refs[role].node),
            idempotency_key=uuid.uuid4().hex,
        )

    def perform(self, role, method, **kwargs):
        self.session.check()
        node = self.node(role)
        return node.wait(method(**self.arguments(role), **kwargs))

    def temperature(self):
        probe = self.probe
        reading = self.cluster.nodes[probe["node"]].core.get_measurements(
            probe["device"], _request_timeout=2.0
        )
        channel = next(
            (c for c in reading.temperature_channels if c.id == probe["channel"]), None
        )
        if (
            channel is None
            or channel.kind not in ("cell", "external_probe")
            or channel.quality != "valid"
            or channel.age_ms > 3000
            or channel.value_c is None
        ):
            raise RuntimeError("Fresh external cell-temperature channel required")
        value = channel.value_c
        if (
            not math.isfinite(value)
            or not self.profile.minimum_temperature_c
            <= value
            <= self.profile.maximum_temperature_c
        ):
            raise RuntimeError("Cell temperature outside profile")
        return value

    def preflight(self):
        p = self.profile
        capabilities = {
            "source": "source.cc_cv",
            "load": "load.cc",
            "interlock": "switch.feedback",
        }
        required_nodes = {ref.node for ref in self.refs.values()} | {self.probe["node"]}
        for name in sorted(required_nodes):
            node = self.cluster.nodes[name]
            info = node.core.get_node()
            config = node.core.get_configuration()
            self.metadata["nodes"][name] = {
                "info": info.to_dict(),
                "configuration": config.to_dict(),
            }
        identities = set()
        for role, ref in self.refs.items():
            node = self.node(role)
            d = node.core.get_device(ref.device)
            identity = (d.driver, d.identity)
            if identity in identities:
                raise RuntimeError(
                    "Duplicate physical instrument identity across workflow roles"
                )
            identities.add(identity)
            if (
                d.state != "ready"
                or d.output != "off"
                or capabilities[role] not in d.capabilities
            ):
                raise RuntimeError(
                    f"{role}: confirmed OFF and required capabilities needed"
                )
            cfg = self.metadata["nodes"][ref.node]["configuration"]
            configured = next(x for x in cfg["devices"] if x["id"] == ref.device)
            guard = configured.get("cell_temperature")
            if (
                not guard
                or guard["minimum_c"] < p.minimum_temperature_c
                or guard["maximum_c"] > p.maximum_temperature_c
            ):
                raise RuntimeError(
                    f"{role}: configure a local cell-temperature guard within profile limits"
                )
            if role != "interlock":
                if (
                    d.limits.voltage_v > p.maximum_voltage_v + 0.001
                    or d.limits.current_a > p.maximum_current_a + 0.001
                    or d.limits.power_w > p.max_power_w + 0.001
                ):
                    raise RuntimeError(
                        f"{role}: node safety limits must fit the cell profile"
                    )
            if role != "interlock":
                requested_current = (
                    p.charge_current_a
                    if role == "source"
                    else max(p.discharge_current_a, p.pulse_current_a)
                )
                duration = (
                    p.charge_timeout_s
                    if role == "source"
                    else max(p.discharge_timeout_s, p.pulse_s)
                )
                configured_duration = configured["limits"].get("max_duration_s")
                if (
                    d.limits.current_a < requested_current
                    or d.limits.voltage_v < p.charge_voltage_v
                    or (
                        configured_duration
                        and configured_duration < duration + p.max_sample_gap_s
                    )
                ):
                    raise RuntimeError(
                        f"{role}: node limits are below requested targets or phase duration"
                    )
            sample = node.core.get_measurements(ref.device)
            if role == "source":
                if sample.set_voltage_v is None or sample.set_current_a is None:
                    raise RuntimeError("Source setpoint snapshot unavailable")
                self.original["source"] = (sample.set_voltage_v, sample.set_current_a)
            elif role == "load":
                if (
                    sample.quality != "valid"
                    or sample.age_ms > 3000
                    or sample.voltage_v is None
                    or not p.discharge_cutoff_v
                    < sample.voltage_v
                    <= p.charge_voltage_v + 0.01
                ):
                    raise RuntimeError(
                        "Commission the Kelvin cell-voltage sense connection before running"
                    )
                settings = node.dl24m.get_dl24m_settings(ref.device)
                if (
                    sample.set_current_a is None
                    or settings.mode != 0
                    or settings.cutoff_voltage_v is None
                    or settings.time_limit_s is None
                ):
                    raise RuntimeError("CC load/settings snapshot unavailable")
                self.original["load"] = (sample.set_current_a, settings)
        self.temperature()
        source_v, source_a = self.original["source"]
        load_a, settings = self.original["load"]
        self.metadata["original_settings"] = {
            "source": {"voltage_v": source_v, "current_a": source_a},
            "load": {"current_a": load_a, "settings": settings.to_dict()},
        }

    def checkpoint(self):
        path = self.directory / "run.json.tmp"
        path.write_text(json.dumps(self.metadata, indent=2) + "\n")
        path.replace(self.directory / "run.json")

    def sample(self, stage, role):
        self.session.check()
        temperature = self.temperature()
        m = self.node(role).core.get_measurements(
            self.device(role), _request_timeout=2.0
        )
        expected_boot = self.metadata["nodes"][self.refs[role].node]["info"]["boot_id"]
        if m.boot_id != expected_boot:
            raise RuntimeError(
                "Node reboot detected; samples cannot be integrated across boots"
            )
        valid = (
            m.quality == "valid"
            and m.age_ms <= 3000
            and m.voltage_v is not None
            and m.current_a is not None
        )
        t = self.clock() - self.start - m.age_ms / 1000
        self.integrators.setdefault(
            stage, Integrator(self.profile.max_sample_gap_s)
        ).add(t, m.voltage_v or 0, m.current_a or 0, valid, m.sequence, stage)
        self.writer.writerow(
            dict(
                elapsed_s=t,
                node=self.refs[role].node,
                device=self.device(role),
                stage=stage,
                sequence=m.sequence,
                boot_id=m.boot_id,
                voltage_v=m.voltage_v,
                current_a=m.current_a,
                power_w=m.power_w,
                temperature_c=temperature,
                quality=m.quality,
                age_ms=m.age_ms,
            )
        )
        if not valid:
            raise RuntimeError(f"{role}: stale/unavailable measurements")
        if (
            m.voltage_v > self.profile.maximum_voltage_v
            or m.current_a > self.profile.maximum_current_a
            or m.power_w is None
            or m.power_w > self.profile.max_power_w
        ):
            raise RuntimeError("Measured voltage/power exceeds profile")
        if ":pulse:" in stage and m.voltage_v <= self.profile.discharge_cutoff_v:
            raise RuntimeError("Pulse reached cell cutoff")
        return m

    def pause(self, seconds, stage):
        end = self.clock() + seconds
        while self.clock() < end:
            self.sample(stage, "load")
            self.sleep(min(self.profile.sample_interval_s, end - self.clock()))

    def isolate(self):
        errors = []
        with ThreadPoolExecutor(max_workers=3) as executor:
            futures = [
                executor.submit(self.node(role).stop, self.device(role))
                for role in ("interlock", "load", "source")
            ]
            for future in futures:
                try:
                    future.result()
                except Exception as exc:
                    errors.append(exc)
        if errors:
            raise ExceptionGroup("Isolation not confirmed", errors)

    def path(self, path):
        self.isolate()
        self.perform(
            "interlock",
            self.node("interlock").interlock.set_switch_path,
            switch_path_request=SwitchPathRequest(path=path),
        )

    def enable(self, role, duration_s):
        self.perform(
            role,
            self.node(role).core.set_output,
            output_request=OutputRequest(
                enabled=True,
                max_duration_ms=math.ceil(
                    (duration_s + self.profile.max_sample_gap_s) * 1000
                ),
            ),
        )

    def charge(self, cycle):
        p = self.profile
        self.isolate()
        self.perform(
            "source",
            self.node("source").rd60xx.set_source_voltage,
            voltage_request=VoltageRequest(voltage_v=p.charge_voltage_v),
        )
        self.perform(
            "source",
            self.node("source").rd60xx.set_source_current,
            current_request=CurrentRequest(current_a=p.charge_current_a),
        )
        self.enable("source", p.charge_timeout_s)
        self.perform(
            "interlock",
            self.node("interlock").interlock.set_switch_path,
            switch_path_request=SwitchPathRequest(path="charge"),
        )
        deadline = self.clock() + p.charge_timeout_s
        held = None
        while self.clock() < deadline:
            m = self.sample(f"{cycle}:charge", "source")
            if (
                m.voltage_v >= p.charge_voltage_v - 0.03
                and m.current_a <= p.termination_current_a
            ):
                if held is None:
                    held = self.clock()
                if self.clock() - held >= p.termination_hold_s:
                    self.isolate()
                    return
            else:
                held = None
            if (
                self.node("source").core.get_device(self.device("source")).output
                != "on"
            ):
                raise RuntimeError("Source stopped before charge termination")
            self.sleep(p.sample_interval_s)
        raise TimeoutError("Charge termination timeout")

    def discharge(self, cycle):
        p = self.profile
        self.path("discharge")
        self.perform(
            "load",
            self.node("load").dl24m.set_dl24m_settings,
            dl24m_settings=Dl24mSettings(
                cutoff_voltage_v=p.discharge_cutoff_v, time_limit_s=0, mode=0
            ),
        )
        self.perform(
            "load",
            self.node("load").dl24m.set_load_current,
            current_request=CurrentRequest(current_a=p.discharge_current_a),
        )
        self.enable("load", p.discharge_timeout_s)
        deadline = self.clock() + p.discharge_timeout_s
        while self.clock() < deadline:
            m = self.sample(f"{cycle}:discharge", "load")
            if m.voltage_v <= p.discharge_cutoff_v + 0.02:
                self.isolate()
                return
            if self.node("load").core.get_device(self.device("load")).output != "on":
                raise RuntimeError("Load stopped before cutoff confirmation")
            self.sleep(p.sample_interval_s)
        raise TimeoutError("Discharge cutoff timeout")

    def pulses(self, cycle):
        p = self.profile
        for index in range(p.pulse_count):
            self.path("discharge")
            self.perform(
                "load",
                self.node("load").dl24m.set_dl24m_settings,
                dl24m_settings=Dl24mSettings(
                    cutoff_voltage_v=p.discharge_cutoff_v, time_limit_s=0, mode=0
                ),
            )
            self.perform(
                "load",
                self.node("load").dl24m.set_load_current,
                current_request=CurrentRequest(current_a=p.pulse_current_a),
            )
            self.enable("load", p.pulse_s)
            self.pause(p.pulse_s, f"{cycle}:pulse:{index}")
            self.isolate()
            self.pause(p.pulse_rest_s, f"{cycle}:pulse-rest:{index}")

    def restore(self):
        errors = []
        try:
            self.isolate()
        except Exception as exc:
            errors.append(exc)
        for role in self.original:
            try:
                if role == "source":
                    v, a = self.original[role]
                    self.perform(
                        role,
                        self.node(role).rd60xx.set_source_voltage,
                        voltage_request=VoltageRequest(voltage_v=v),
                    )
                    self.perform(
                        role,
                        self.node(role).rd60xx.set_source_current,
                        current_request=CurrentRequest(current_a=a),
                    )
                else:
                    a, settings = self.original[role]
                    self.perform(
                        role,
                        self.node(role).dl24m.set_load_current,
                        current_request=CurrentRequest(current_a=a),
                    )
                    # Restore only fields deliberately changed by this workflow.
                    self.perform(
                        role,
                        self.node(role).dl24m.set_dl24m_settings,
                        dl24m_settings=Dl24mSettings(
                            cutoff_voltage_v=settings.cutoff_voltage_v,
                            time_limit_s=settings.time_limit_s,
                            mode=settings.mode,
                        ),
                    )
            except Exception as exc:
                errors.append(exc)
        self.metadata["restore_pending"] = bool(errors)
        if errors:
            raise ExceptionGroup(
                "Restore incomplete; outputs were requested OFF", errors
            )

    def run(self):
        try:
            self.preflight()
            self.metadata["status"] = "reserving"
            self.checkpoint()
            groups = {}
            for ref in self.refs.values():
                groups.setdefault(ref.node, []).append(ref.device)
            with self.cluster.lease(groups) as session:
                self.session = session
                self.metadata["status"] = "running"
                self.metadata["restore_pending"] = True
                self.checkpoint()
                with (self.directory / "samples.csv").open(
                    "w", newline="", buffering=1
                ) as stream:
                    self.writer = csv.DictWriter(
                        stream,
                        fieldnames="elapsed_s node device stage sequence boot_id voltage_v current_a power_w temperature_c quality age_ms".split(),
                    )
                    self.writer.writeheader()
                    try:
                        for cycle in range(self.profile.cycles):
                            self.charge(cycle)
                            self.pause(
                                self.profile.rest_s, f"{cycle}:rest-after-charge"
                            )
                            self.pulses(cycle)
                            self.discharge(cycle)
                            self.pause(
                                self.profile.rest_s, f"{cycle}:rest-after-discharge"
                            )
                        self.metadata["status"] = "succeeded"
                    finally:
                        self.restore()
        except BaseException as exc:
            self.metadata["status"] = "failed"
            self.metadata["error"] = str(exc)
            raise
        finally:
            self.metadata["integration"] = {
                k: dict(
                    ah=v.ah, wh=v.wh, accepted_s=v.accepted_s, excluded_s=v.excluded_s
                )
                for k, v in self.integrators.items()
            }
            self.metadata["finished_at"] = datetime.datetime.now(
                datetime.timezone.utc
            ).isoformat()
            self.checkpoint()
        return self.metadata

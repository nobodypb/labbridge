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
from labbridge_generated.models.cutoff_request import CutoffRequest
from labbridge_generated.models.duration_request import DurationRequest
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


class Integrator:
    def __init__(self, max_gap_s):
        self.max_gap_s = max_gap_s
        self.previous = None
        self.ah = self.wh = 0.0
        self.accepted_s = self.excluded_s = 0.0

    def add(self, t, v, a, valid, sequence, stage, power_w=None):
        if not valid or not all(math.isfinite(x) for x in (t, v, a)) or v < 0 or a < 0:
            self.previous = None
            return
        power = v * a if power_w is None else power_w
        if not math.isfinite(power) or power < 0:
            self.previous = None
            return
        current = (t, v, a, sequence, stage, power)
        if self.previous:
            pt, pv, pa, ps, pstage, pp = self.previous
            dt = t - pt
            if 0 < dt <= self.max_gap_s and sequence != ps and stage == pstage:
                self.ah += (pa + a) * dt / 7200
                self.wh += (pp + power) * dt / 7200
                self.accepted_s += dt
            elif dt > 0:
                self.excluded_s += dt
        self.previous = current


def binding_nodes(bindings):
    refs = [
        bindings[k]
        for k in ("source", "load", "interlock", "cell_probe")
        if k in bindings
    ]
    refs += list(bindings.get("meters", {}).values()) + list(
        bindings.get("sensors", [])
    )
    return {ref["node"] for ref in refs}


@dataclasses.dataclass(frozen=True)
class Ref:
    node: str
    device: str
    component: str = ""


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
            k: Ref(
                **{
                    **v,
                    "component": v.get(
                        "component",
                        "sink" if k == "load" else "source" if k == "source" else "",
                    ),
                }
            )
            for k, v in bindings.items()
            if k in ("source", "load", "interlock")
        }
        if not {"source", "load"}.issubset(self.refs):
            raise ValueError("Source and load bindings required; interlock is optional")
        if len(set(self.refs.values())) != len(self.refs):
            raise ValueError("Roles must use distinct components")
        self.probe = bindings["cell_probe"]
        self.meters = bindings.get("meters", {})
        if set(self.meters) - {"charge", "discharge", "rest"}:
            raise ValueError("Meter phases are charge, discharge and rest")
        for meter in self.meters.values():
            if meter.get("current_sign", 1) not in (-1, 1) or meter.get(
                "power_sign", 1
            ) not in (-1, 1):
                raise ValueError("Meter current_sign/power_sign must be +1 or -1")
        self.extra_sensors = bindings.get("sensors", [])
        self.sensor_epochs = {}
        self.actor_actions = {}
        self.sensor_writer = None
        self.probe_component = None
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
            "routing": (
                "feedback-interlock" if "interlock" in self.refs else "direct-outputs"
            ),
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
            **(
                {"component_id": self.refs[role].component}
                if role in ("source", "load")
                else {}
            ),
            device_id=self.device(role),
            x_lab_lease=self.session.token(self.refs[role].node),
            idempotency_key=uuid.uuid4().hex,
        )

    def perform(self, role, method, **kwargs):
        self.session.check()
        node = self.node(role)
        return node.wait(method(**self.arguments(role), **kwargs))

    def read_sensor(self, ref, component):
        node = self.cluster.nodes[ref["node"]]
        sample = node.components.get_component_reading(
            ref["device"], component, _request_timeout=2.0
        )
        self.check_sensor_boot(ref["node"], sample.boot_id)
        key = (ref["node"], ref["device"], component)
        epoch = getattr(sample, "epoch", None)
        if key in self.sensor_epochs and self.sensor_epochs[key] != epoch:
            raise RuntimeError("Sensor restarted; measurement continuity lost")
        self.sensor_epochs[key] = epoch
        if (
            sample.quality != "valid"
            or sample.age_ms > 3000
            or sample.value is None
            or not math.isfinite(sample.value)
        ):
            raise RuntimeError("Fresh sensor reading required")
        return sample

    def check_sensor_boot(self, name, boot_id):
        expected = self.metadata["nodes"][name]["info"]["boot_id"]
        if boot_id != expected:
            raise RuntimeError(
                "Node reboot detected; samples cannot be integrated across boots"
            )

    def temperature(self):
        probe = self.probe
        component = probe.get(
            "component", "temperature-" + probe.get("channel", "cell")
        )
        if self.probe_component is None:
            descriptors = (
                self.cluster.nodes[probe["node"]]
                .components.list_components(probe["device"])
                .components
            )
            channel = next((c for c in descriptors if c.id == component), None)
            if (
                channel is None
                or channel.quantity != "temperature"
                or channel.temperature_kind not in ("cell", "external_probe")
            ):
                raise RuntimeError("External cell-temperature component required")
            self.probe_component = component
        sample = self.read_sensor(probe, component)
        if (
            sample.unit != "degC"
            or not self.profile.minimum_temperature_c
            <= sample.value
            <= self.profile.maximum_temperature_c
        ):
            raise RuntimeError("Cell temperature outside profile")
        return sample.value

    def preflight(self):
        p = self.profile
        capabilities = {
            "source": "source.cc_cv",
            "load": "load.cc",
            "interlock": "switch.feedback",
        }
        required_nodes = binding_nodes(self.metadata["bindings"])
        for name in sorted(required_nodes):
            node = self.cluster.nodes[name]
            info = node.core.get_node()
            config = node.core.get_configuration()
            self.metadata["nodes"][name] = {
                "info": info.to_dict(),
                "configuration": config.to_dict(),
            }
        identities = set()
        owners = {}
        for role, ref in self.refs.items():
            node = self.node(role)
            d = node.core.get_device(ref.device)
            physical = (d.driver, d.identity)
            if physical in owners and owners[physical] != (ref.node, ref.device):
                raise RuntimeError("A physical actuator device requires one owner node")
            owners[physical] = (ref.node, ref.device)
            identity = (d.driver, d.identity, ref.component)
            if identity in identities:
                raise RuntimeError(
                    "Duplicate physical component identity across workflow roles"
                )
            identities.add(identity)
            if (
                d.state != "ready"
                or d.output != "off"
                or (role == "interlock" and capabilities[role] not in d.capabilities)
            ):
                raise RuntimeError(
                    f"{role}: confirmed OFF and required capabilities needed"
                )
            actor_state = None
            if role in ("source", "load"):
                descriptors = node.components.list_components(ref.device).components
                component = next(
                    (c for c in descriptors if c.id == ref.component), None
                )
                if component is None or component.kind != (
                    "source" if role == "source" else "sink"
                ):
                    raise RuntimeError("Configured actor component unavailable")
                self.actor_actions[role] = set(component.actions)
                actor_state = node.components.get_actuator_state(
                    ref.device, ref.component
                )
                if (
                    actor_state.output != "off"
                    or actor_state.state != "ready"
                    or d.limits is None
                ):
                    raise RuntimeError(
                        "Ready actor with confirmed OFF and local limits required"
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
                if (
                    actor_state.settings.voltage_v is None
                    or actor_state.settings.current_a is None
                ):
                    raise RuntimeError("Source setpoint snapshot unavailable")
                self.original["source"] = (
                    actor_state.settings.voltage_v,
                    actor_state.settings.current_a,
                )
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
                settings = actor_state.settings
                if (
                    settings.current_a is None
                    or settings.regulation_mode != "cc"
                    or settings.cutoff_voltage_v is None
                    or "cutoff" not in self.actor_actions[role]
                ):
                    raise RuntimeError("CC load/settings snapshot unavailable")
                self.original["load"] = (settings.current_a, settings)
        self.temperature()
        for meter in self.meters.values():
            self.meter_sample(meter)
        for extra in self.extra_sensors:
            self.extra_sensor_sample(extra)
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

    def remember_epoch(self, node, device, reading):
        self.check_sensor_boot(node, reading.boot_id)
        key = (node, device, reading.component_id)
        epoch = getattr(reading, "epoch", None)
        if key in self.sensor_epochs and self.sensor_epochs[key] != epoch:
            raise RuntimeError("Sensor restarted; measurement continuity lost")
        self.sensor_epochs[key] = epoch

    def meter_sample(self, ref):
        result = self.cluster.nodes[ref["node"]].components.get_sensor_readings(
            ref["device"], _request_timeout=2.0
        )
        self.check_sensor_boot(ref["node"], result.boot_id)
        by_id = {r.component_id: r for r in result.readings}
        values = {}
        selected = []
        for quantity, unit in [("voltage", "V"), ("current", "A"), ("power", "W")]:
            key = ref.get(quantity + "_component", quantity)
            r = by_id.get(key)
            if (
                r is None
                or r.quantity != quantity
                or r.unit != unit
                or r.quality != "valid"
                or r.age_ms > 3000
                or r.value is None
                or not math.isfinite(r.value)
            ):
                raise RuntimeError("Fresh DC meter components in SI units required")
            self.remember_epoch(ref["node"], ref["device"], r)
            values[quantity] = r.value
            selected.append(r)
        if len({r.sequence for r in selected}) != 1:
            raise RuntimeError("Meter readings are not from one sample")
        values.update(
            node=ref["node"],
            device=ref["device"],
            sequence=selected[0].sequence,
            boot_id=result.boot_id,
            age_ms=max(r.age_ms for r in selected),
            quality="valid",
        )
        return values

    def extra_sensor_sample(self, ref):
        sample = self.read_sensor(ref, ref["component"])
        if (
            "minimum" in ref
            and sample.value < ref["minimum"]
            or "maximum" in ref
            and sample.value > ref["maximum"]
        ):
            raise RuntimeError("Additional sensor outside configured range")
        return sample

    def sample(self, stage, role):
        from types import SimpleNamespace

        self.session.check()
        temperature = self.temperature()
        actor = self.node(role).core.get_measurements(
            self.device(role), _request_timeout=2.0
        )
        self.check_sensor_boot(self.refs[role].node, actor.boot_id)
        if (
            actor.quality != "valid"
            or actor.age_ms > 3000
            or any(
                v is None or not math.isfinite(v)
                for v in (actor.voltage_v, actor.current_a, actor.power_w)
            )
        ):
            raise RuntimeError(f"{role}: stale/unavailable measurements")
        if (
            actor.voltage_v > self.profile.maximum_voltage_v
            or abs(actor.current_a) > self.profile.maximum_current_a
            or abs(actor.power_w) > self.profile.max_power_w
        ):
            raise RuntimeError("Actor measurement exceeds profile")
        phase = (
            "charge"
            if stage.endswith(":charge")
            else (
                "discharge"
                if stage.endswith(":discharge") or ":pulse:" in stage
                else "rest"
            )
        )
        meter = self.meters.get(phase)
        if meter:
            selected = self.meter_sample(meter)
            raw_current, raw_power = selected["current"], selected["power"]
            v, a, p = (
                selected["voltage"],
                raw_current * meter.get("current_sign", 1),
                raw_power * meter.get("power_sign", 1),
            )
        else:
            v, a, p = actor.voltage_v, actor.current_a, actor.power_w
            raw_current, raw_power = a, p
            selected = dict(
                node=self.refs[role].node,
                device=self.device(role),
                sequence=actor.sequence,
                boot_id=actor.boot_id,
                age_ms=actor.age_ms,
                quality=actor.quality,
            )
        if phase != "rest" and (a < 0 or p < 0):
            raise RuntimeError(
                "Meter polarity disagrees with the active phase; configure explicit signs"
            )
        if (
            v < 0
            or v > self.profile.maximum_voltage_v
            or abs(a) > self.profile.maximum_current_a
            or abs(p) > self.profile.max_power_w
        ):
            raise RuntimeError("Selected measurements exceed profile")
        t = self.clock() - self.start - selected["age_ms"] / 1000
        self.integrators.setdefault(
            stage, Integrator(self.profile.max_sample_gap_s)
        ).add(t, v, a, True, selected["sequence"], stage, power_w=p)
        self.writer.writerow(
            dict(
                elapsed_s=t,
                node=selected["node"],
                device=selected["device"],
                actor_node=self.refs[role].node,
                actor_device=self.device(role),
                actor_component=self.refs[role].component,
                stage=stage,
                sequence=selected["sequence"],
                boot_id=selected["boot_id"],
                voltage_v=v,
                current_a=a,
                power_w=p,
                current_raw_a=raw_current,
                power_raw_w=raw_power,
                temperature_c=temperature,
                quality=selected["quality"],
                age_ms=selected["age_ms"],
            )
        )
        for extra in self.extra_sensors:
            reading = self.extra_sensor_sample(extra)
            if self.sensor_writer is not None:
                self.sensor_writer.writerow(
                    dict(
                        elapsed_s=self.clock() - self.start - reading.age_ms / 1000,
                        label=extra.get("label", extra["component"]),
                        node=extra["node"],
                        device=extra["device"],
                        component=extra["component"],
                        boot_id=reading.boot_id,
                        sequence=reading.sequence,
                        quantity=reading.quantity,
                        unit=reading.unit,
                        value=reading.value,
                        quality=reading.quality,
                        age_ms=reading.age_ms,
                    )
                )
        if ":pulse:" in stage and v <= self.profile.discharge_cutoff_v:
            raise RuntimeError("Pulse reached cell cutoff")
        return SimpleNamespace(voltage_v=v, current_a=a, power_w=p)

    def pause(self, seconds, stage):
        end = self.clock() + seconds
        while self.clock() < end:
            self.sample(stage, "load")
            self.sleep(min(self.profile.sample_interval_s, end - self.clock()))

    def isolate(self):
        errors = []
        targets = {(ref.node, ref.device) for ref in self.refs.values()}
        with ThreadPoolExecutor(max_workers=len(targets)) as executor:
            futures = [
                executor.submit(self.cluster.nodes[name].stop, identifier)
                for name, identifier in targets
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
        if "interlock" in self.refs:
            self.perform(
                "interlock",
                self.node("interlock").interlock.set_switch_path,
                switch_path_request=SwitchPathRequest(path=path),
            )

    def enable(self, role, duration_s):
        self.perform(
            role,
            self.node(role).components.set_component_output,
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
        # Absolute writes are checked against the other, still-current setpoint.
        self.perform(
            "source",
            self.node("source").components.set_component_current,
            current_request=CurrentRequest(current_a=0.0),
        )
        self.perform(
            "source",
            self.node("source").components.set_component_voltage,
            voltage_request=VoltageRequest(voltage_v=p.charge_voltage_v),
        )
        self.perform(
            "source",
            self.node("source").components.set_component_current,
            current_request=CurrentRequest(current_a=p.charge_current_a),
        )
        self.enable("source", p.charge_timeout_s)
        if "interlock" in self.refs:
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
            self.node("load").components.set_component_current,
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
                self.node("load").components.set_component_current,
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
                        self.node(role).components.set_component_current,
                        current_request=CurrentRequest(current_a=0.0),
                    )
                    self.perform(
                        role,
                        self.node(role).components.set_component_voltage,
                        voltage_request=VoltageRequest(voltage_v=v),
                    )
                    self.perform(
                        role,
                        self.node(role).components.set_component_current,
                        current_request=CurrentRequest(current_a=a),
                    )
                else:
                    a, settings = self.original[role]
                    self.perform(
                        role,
                        self.node(role).components.set_component_current,
                        current_request=CurrentRequest(current_a=a),
                    )
                    self.perform(
                        role,
                        self.node(role).components.set_component_cutoff,
                        cutoff_request=CutoffRequest(
                            cutoff_voltage_v=settings.cutoff_voltage_v
                        ),
                    )
                    if (
                        settings.time_limit_s is not None
                        and "duration" in self.actor_actions[role]
                    ):
                        self.perform(
                            role,
                            self.node(role).components.set_component_duration,
                            duration_request=DurationRequest(
                                time_limit_s=settings.time_limit_s
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
                ids = groups.setdefault(ref.node, [])
                if ref.device not in ids:
                    ids.append(ref.device)
            with self.cluster.lease(groups) as session:
                self.session = session
                self.metadata["status"] = "running"
                self.metadata["restore_pending"] = True
                self.checkpoint()
                with (self.directory / "samples.csv").open(
                    "w", newline="", buffering=1
                ) as stream, (self.directory / "sensors.csv").open(
                    "w", newline="", buffering=1
                ) as sensor_stream:
                    self.writer = csv.DictWriter(
                        stream,
                        fieldnames="elapsed_s node device actor_node actor_device actor_component stage sequence boot_id voltage_v current_a power_w current_raw_a power_raw_w temperature_c quality age_ms".split(),
                    )
                    self.writer.writeheader()
                    self.sensor_writer = csv.DictWriter(
                        sensor_stream,
                        fieldnames="elapsed_s label node device component boot_id sequence quantity unit value quality age_ms".split(),
                    )
                    self.sensor_writer.writeheader()
                    try:
                        self.perform(
                            "load",
                            self.node("load").components.set_component_cutoff,
                            cutoff_request=CutoffRequest(
                                cutoff_voltage_v=self.profile.discharge_cutoff_v
                            ),
                        )
                        if "duration" in self.actor_actions["load"]:
                            self.perform(
                                "load",
                                self.node("load").components.set_component_duration,
                                duration_request=DurationRequest(time_limit_s=0),
                            )
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

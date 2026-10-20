"""Deterministic API simulator for workflow development; not a cell physics model."""

import json
from contextlib import contextmanager
from types import SimpleNamespace
from urllib.parse import urlsplit
from labbridge_generated.rest import RESTResponse
from .sdk import Node, Cluster


class Clock:
    def __init__(self):
        self.now = 0.0
        self.listeners = []

    def __call__(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds
        for fn in self.listeners:
            fn(seconds)


class Cell:
    def __init__(self, clock, profile):
        self.clock, self.profile = clock, profile
        self.voltage = (profile.charge_voltage_v + profile.discharge_cutoff_v) / 2
        self.devices = []
        self.path = "isolated"
        self.temperature = 25.0
        clock.listeners.append(self.advance)

    def advance(self, dt):
        for d in self.devices:
            if (
                d["output"]
                and d["role"] == "source"
                and self.path in ("charge", "direct")
            ):
                self.voltage = min(d["voltage"], self.voltage + dt * 0.2)
            if (
                d["output"]
                and d["role"] == "load"
                and self.path in ("discharge", "direct")
            ):
                self.voltage = max(
                    self.profile.discharge_cutoff_v, self.voltage - dt * 0.15
                )
                if self.voltage <= self.profile.discharge_cutoff_v:
                    d["output"] = False


class Simulator:
    def __init__(self, name, roles, clock, cell, profile):
        self.name, self.clock, self.cell, self.profile = name, clock, cell, profile
        self.devices = {}
        self.leases = {}
        self.operations = {}
        self.keys = {}
        self.sequence = 0
        self.boot_id = "sim-" + name + "-boot"
        self.stale = False
        self.fail_next = False
        for role, identifier in roles.items():
            d = dict(
                role=role,
                output=False,
                current=0.1,
                voltage=profile.charge_voltage_v,
                settings={
                    "cutoff_voltage_v": profile.discharge_cutoff_v,
                    "time_limit_s": 0,
                    "mode": 0,
                },
            )
            self.devices[identifier] = d
            cell.devices.append(d)
        if "probe" not in self.devices:
            self.devices["probe"] = dict(role="probe", output=False)

    def reply(self, data, status=200):
        return RESTResponse(
            SimpleNamespace(
                status=status,
                reason="",
                headers={"Content-Type": "application/json"},
                data=json.dumps(self.clean(data)).encode(),
            )
        )

    @staticmethod
    def clean(value):
        if isinstance(value, dict):
            return {k: Simulator.clean(v) for k, v in value.items() if v is not None}
        if isinstance(value, list):
            return [Simulator.clean(v) for v in value]
        return value

    def limits(self, role):
        p = self.profile
        return dict(
            voltage_v=p.maximum_voltage_v,
            current_a=p.maximum_current_a,
            power_w=p.max_power_w,
            max_age_ms=3000,
        )

    def describe(self, identifier):
        d = self.devices[identifier]
        role = d["role"]
        caps = {
            "source": ["source.cc_cv", "output"],
            "load": ["load.cc", "output", "dl24m.settings"],
            "interlock": ["switch.path", "switch.feedback", "output"],
            "probe": ["temperature.external"],
            "meter": [
                "components",
                "voltage.measure",
                "current.measure",
                "power.measure",
            ],
        }[role]
        return dict(
            id=identifier,
            driver={
                "source": "rd60xx",
                "load": "dl24m",
                "interlock": "gpio_interlock",
                "probe": "ds18b20",
                "meter": "ina228",
            }[role],
            identity=f"sim-{self.name}-{identifier}",
            transport="sim",
            capabilities=caps,
            state="ready",
            output="on" if d["output"] else "off",
            limits=self.limits(role),
        )

    def config(self):
        devices = []
        for identifier in self.devices:
            d = self.describe(identifier)
            entry = {k: d[k] for k in ("id", "driver", "transport", "limits")}
            entry["endpoint"] = d["identity"]
            entry["cell_temperature"] = dict(
                sensor_device_id="probe",
                channel_id="cell",
                minimum_c=self.profile.minimum_temperature_c,
                maximum_c=self.profile.maximum_temperature_c,
                max_age_ms=3000,
            )
            devices.append(entry)
        return dict(revision=1, node_id=self.name, board="sim", devices=devices)

    def component_list(self, identifier):
        role = self.devices[identifier]["role"]
        result = []
        if role in ("source", "load"):
            result.append(
                dict(
                    id="source" if role == "source" else "sink",
                    kind="source" if role == "source" else "sink",
                    actions=["current", "output"]
                    + (["voltage"] if role == "source" else ["cutoff", "duration"]),
                )
            )
        if role in ("source", "load", "meter"):
            for q, u in [("voltage", "V"), ("current", "A"), ("power", "W")]:
                result.append(dict(id=q, kind="sensor", quantity=q, unit=u, actions=[]))
        result.append(
            dict(
                id="temperature-die" if role == "meter" else "temperature-cell",
                kind="sensor",
                quantity="temperature",
                unit="degC",
                actions=[],
                temperature_kind="internal" if role == "meter" else "cell",
            )
        )
        return result

    def measure(self, identifier, now):
        self.sequence += 1
        d = self.devices[identifier]
        current = 0.0
        if d["output"]:
            current = (
                d["current"]
                if d["role"] == "load"
                else 0.01 if self.cell.voltage >= d["voltage"] - 0.001 else d["current"]
            )
        if d["role"] == "meter":
            for device in self.cell.devices:
                if (
                    device.get("output")
                    and device["role"] == "source"
                    and self.cell.path in ("charge", "direct")
                ):
                    current = (
                        0.01
                        if self.cell.voltage >= device["voltage"] - 0.001
                        else device["current"]
                    )
                if (
                    device.get("output")
                    and device["role"] == "load"
                    and self.cell.path in ("discharge", "direct")
                ):
                    current = -device["current"]
        return dict(
            device_id=identifier,
            boot_id=self.boot_id,
            sequence=self.sequence,
            received_ms=now,
            age_ms=4000 if self.stale else 0,
            quality="stale" if self.stale else "valid",
            voltage_v=self.cell.voltage,
            current_a=current,
            power_w=self.cell.voltage * abs(current),
            set_voltage_v=d.get("voltage"),
            set_current_a=d.get("current"),
            regulation_mode="cc",
            temperature_channels=[
                dict(
                    id="cell",
                    kind="cell",
                    quality="valid",
                    value_c=self.cell.temperature,
                    received_ms=now,
                    age_ms=0,
                )
            ],
        )

    def component_reading(self, identifier, component, now, frame=None):
        frame = frame or self.measure(identifier, now)
        c = next(c for c in self.component_list(identifier) if c["id"] == component)
        value = (
            self.cell.temperature
            if c["quantity"] == "temperature"
            else frame[
                {"voltage": "voltage_v", "current": "current_a", "power": "power_w"}[
                    component
                ]
            ]
        )
        return dict(
            device_id=identifier,
            component_id=component,
            boot_id=self.boot_id,
            epoch="1",
            quantity=c["quantity"],
            unit=c["unit"],
            value=value,
            sequence=frame["sequence"],
            received_ms=now,
            age_ms=frame["age_ms"],
            quality=frame["quality"],
        )

    def request(self, method, url, headers=None, body=None, **kwargs):
        path = urlsplit(url).path
        headers = headers or {}
        body = body or {}
        parts = path.strip("/").split("/")[2:]
        now = int(self.clock() * 1000)
        for lease in self.leases.values():
            if lease["state"] == "active" and now >= lease["expires_at_ms"]:
                lease["state"] = "stopped"
                for identifier in lease["device_ids"]:
                    self.stop(identifier)
        if parts == ["node"]:
            return self.reply(
                dict(
                    id=self.name,
                    boot_id=self.boot_id,
                    firmware_version="sim",
                    board="sim",
                    api_version="0.1.0",
                )
            )
        if parts == ["configuration"]:
            return self.reply(self.config())
        if parts == ["devices"]:
            return self.reply({"devices": [self.describe(i) for i in self.devices]})
        if parts == ["leases"] and method == "POST":
            identifier = f"l{len(self.leases)+1}"
            self.leases[identifier] = dict(
                id=identifier,
                device_ids=body["device_ids"],
                expires_at_ms=now + body.get("ttl_ms", 10000),
                state="active",
            )
            return self.reply(self.leases[identifier])
        if parts[0] == "leases":
            l = self.leases[parts[1]]
            if len(parts) > 2 and parts[2] == "heartbeat" and l["state"] == "active":
                l["expires_at_ms"] = now + 10000
            if len(parts) > 2 and parts[2] == "stop":
                l["state"] = "stopped"
                for identifier in l["device_ids"]:
                    self.stop(identifier)
            return self.reply(l)
        if parts[0] == "operations":
            return self.reply(self.operations[parts[1]])
        if parts[0] != "devices" or parts[1] not in self.devices:
            return self.reply(
                dict(
                    type="about:blank",
                    title="Not found",
                    status=404,
                    detail="Unknown route/device",
                ),
                404,
            )
        identifier = parts[1]
        d = self.devices[identifier]
        tail = parts[2:]
        if method == "GET":
            if not tail:
                return self.reply(self.describe(identifier))
            if tail == ["dl24m", "settings"]:
                return self.reply(d["settings"])
            if tail == ["measurements"]:
                return self.reply(self.measure(identifier, now))
            if tail == ["components"]:
                return self.reply(dict(components=self.component_list(identifier)))
            if tail == ["readings"]:
                frame = self.measure(identifier, now)
                return self.reply(
                    dict(
                        device_id=identifier,
                        boot_id=self.boot_id,
                        readings=[
                            self.component_reading(identifier, c["id"], now, frame)
                            for c in self.component_list(identifier)
                            if c["kind"] == "sensor"
                        ],
                    )
                )
            if len(tail) == 3 and tail[0] == "components":
                component = tail[1]
                if tail[2] == "measurement":
                    return self.reply(
                        self.component_reading(identifier, component, now)
                    )
                if tail[2] == "state":
                    settings = dict(current_a=d["current"], regulation_mode="cc")
                    if d["role"] == "source":
                        settings["voltage_v"] = d["voltage"]
                    if d["role"] == "load":
                        settings.update(
                            {k: v for k, v in d["settings"].items() if k != "mode"}
                        )
                    return self.reply(
                        dict(
                            device_id=identifier,
                            component_id=component,
                            state="ready",
                            output="on" if d["output"] else "off",
                            settings=settings,
                        )
                    )
        if len(tail) == 3 and tail[0] == "components":
            actor = next(
                (c for c in self.component_list(identifier) if c["id"] == tail[1]), None
            )
            if (
                actor is None
                or actor["kind"] == "sensor"
                or tail[2] not in actor["actions"]
            ):
                return self.reply(
                    dict(
                        type="about:blank",
                        title="Unavailable",
                        status=422,
                        detail="Component action unavailable",
                    ),
                    422,
                )
            action = tail[2]
            tail = (
                ["output"]
                if action == "output"
                else (
                    ["source" if actor["kind"] == "source" else "load", action]
                    if action in ("current", "voltage")
                    else ["dl24m", "settings"]
                )
            )
        key = headers.get("Idempotency-Key", "")
        signature = json.dumps([method, path, body], sort_keys=True)
        if key in self.keys:
            previous, operation = self.keys[key]
            if signature != previous:
                return self.reply(
                    dict(
                        type="about:blank",
                        title="Conflict",
                        status=409,
                        detail="Key reuse",
                    ),
                    409,
                )
            return self.reply(operation, 202)
        stop = tail == ["stop"] or tail == ["output"] and not body.get("enabled")
        lease = headers.get("X-Lab-Lease", "")
        if not stop and (
            lease not in self.leases
            or self.leases[lease]["state"] != "active"
            or identifier not in self.leases[lease]["device_ids"]
        ):
            return self.reply(
                dict(
                    type="about:blank",
                    title="Lease required",
                    status=423,
                    detail="Lease required",
                ),
                423,
            )
        if stop:
            self.stop(identifier)
        elif tail == ["output"]:
            d["output"] = body["enabled"]
        elif tail in (["load", "current"], ["source", "current"]):
            d["current"] = body["current_a"]
        elif tail == ["source", "voltage"]:
            d["voltage"] = body["voltage_v"]
        elif tail == ["switch", "path"]:
            self.cell.path = body["path"]
            d["output"] = body["path"] != "isolated"
        elif tail == ["dl24m", "settings"]:
            d["settings"].update(body)
        operation = dict(
            id=f"o{len(self.operations)+1}",
            device_id=identifier,
            status="failed" if self.fail_next else "succeeded",
            stage="injected_failure" if self.fail_next else "confirmed",
            read_attempts=1,
            write_attempts=1,
            reconnects=0,
            duration_ms=1,
            final_output="on" if d["output"] else "off",
            restore_pending=False,
        )
        self.fail_next = False
        self.operations[operation["id"]] = operation
        self.keys[key] = (signature, operation)
        return self.reply(operation, 202)

    def stop(self, identifier):
        d = self.devices[identifier]
        d["output"] = False
        if d["role"] == "interlock":
            self.cell.path = "isolated"


class SimulatedCluster(Cluster):
    def __init__(self, nodes, clock):
        super().__init__(nodes)
        self.clock = clock

    @contextmanager
    def lease(self, devices):
        with super().lease(devices) as session:
            callback = lambda dt: session.heartbeat()
            self.clock.listeners.append(callback)
            try:
                yield session
            finally:
                self.clock.listeners.remove(callback)


def simulated_cluster(bindings, profile):
    clock = Clock()
    cell = Cell(clock, profile)
    roles = {}
    for role, ref in bindings.items():
        if role not in ("source", "load", "interlock"):
            continue
        roles.setdefault(ref["node"], {})[role] = ref["device"]
    roles.setdefault(bindings["cell_probe"]["node"], {})["probe"] = bindings[
        "cell_probe"
    ]["device"]
    if "interlock" not in bindings:
        cell.path = "direct"
    for ref in list(bindings.get("meters", {}).values()) + bindings.get("sensors", []):
        mapping = roles.setdefault(ref["node"], {})
        if ref["device"] not in mapping.values():
            mapping["meter"] = ref["device"]
    transports = {
        name: Simulator(name, devices, clock, cell, profile)
        for name, devices in roles.items()
    }
    return (
        SimulatedCluster(
            {
                name: Node("http://" + name, rest=rest)
                for name, rest in transports.items()
            },
            clock,
        ),
        clock,
        transports,
    )

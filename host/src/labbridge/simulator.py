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
            if d["output"] and d["role"] == "source" and self.path == "charge":
                self.voltage = min(d["voltage"], self.voltage + dt * 0.2)
            if d["output"] and d["role"] == "load" and self.path == "discharge":
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
        }[role]
        return dict(
            id=identifier,
            driver={
                "source": "rd60xx",
                "load": "dl24m",
                "interlock": "gpio_interlock",
                "probe": "ds18b20",
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
                self.sequence += 1
                current = 0.0
                if d["output"]:
                    if d["role"] == "load":
                        current = d["current"]
                    elif d["role"] == "source":
                        current = (
                            0.01
                            if self.cell.voltage >= d["voltage"] - 0.001
                            else d["current"]
                        )
                return self.reply(
                    dict(
                        device_id=identifier,
                        boot_id=self.boot_id,
                        sequence=self.sequence,
                        received_ms=now,
                        age_ms=4000 if self.stale else 0,
                        quality="stale" if self.stale else "valid",
                        voltage_v=self.cell.voltage,
                        current_a=current,
                        power_w=self.cell.voltage * current,
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
        if role == "cell_probe":
            continue
        roles.setdefault(ref["node"], {})[role] = ref["device"]
    roles.setdefault(bindings["cell_probe"]["node"], {})["probe"] = bindings[
        "cell_probe"
    ]["device"]
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

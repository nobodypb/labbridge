"""Deterministic API simulator; never opens a device or network connection."""
from __future__ import annotations
import copy
import math
import secrets
import threading
import time
from .client import BridgeError, Session


class Simulator:
    def __init__(self, clock=time.monotonic, sample_period=1.0, source="psu1"):
        self.clock, self.sample_period = clock, sample_period
        self.source = source
        self.timeout = 1
        self.lock = threading.RLock()
        self.started = clock()
        self.token = ""
        self.deadline = 0
        self.owned: list[str] = []
        self.last_stop = ""
        self.stop_outputs = [None]*3
        self.commands: list[tuple] = []
        self.disconnected: set[str] = set()
        self.frozen: set[str] = set()
        self.wifi = True
        self.data = {}
        for name in ("load", "psu1", "psu2"):
            self.data[name] = dict(id=name, model="DL24M-A" if name == "load" else "RD6024",
                identity="SIM-"+name, transport="simulation", connected=True, fresh=True, writable=True,
                capabilities=["measure", "output", "current_a"] + ([] if name == "load" else ["voltage_v"]),
                limits=dict(voltage_v=30., current_a=2., power_w=30.), output=False,
                voltage_v=0., current_a=0., power_w=0., set_voltage_v=0., set_current_a=0.,
                temperature_c=25., protection=0, sequence=1, received_ms=0, age_ms=0)

    def _stop(self, reason):
        self.last_stop = reason
        self.stop_outputs = [None]*3
        for name in ("psu1", "psu2", "load"):
            if name in self.owned:
                i = ("load", "psu1", "psu2").index(name)
                if name not in self.disconnected:
                    self.data[name]["output"] = False
                    self.stop_outputs[i] = False
                    self.commands.append((name, "output", False))
        self.token = ""
        self.owned = []

    def _tick(self):
        now = self.clock()
        ms = int((now-self.started)*1000)
        for name, d in self.data.items():
            d["connected"] = name not in self.disconnected
            if d["connected"] and name not in self.frozen and ms-d["received_ms"] >= self.sample_period*1000:
                d["sequence"] += 1
                d["received_ms"] = ms
                source = self.data[self.source] if self.source else None
                voltage = (source["set_voltage_v"] if source["output"] else 0.) if source else 12.
                current = self.data["load"]["set_current_a"] if self.data["load"]["output"] and voltage else 0.
                if name != "load" and name != self.source:
                    voltage = d["set_voltage_v"] if d["output"] else 0.
                    current = 0.
                d.update(voltage_v=voltage, current_a=current, power_w=voltage*current)
            d["age_ms"] = ms-d["received_ms"]
            d["fresh"] = d["connected"] and d["age_ms"] <= 3000
        if self.token:
            if now >= self.deadline:
                self._stop("lease_expired")
            elif not self.wifi:
                self._stop("wifi_disconnected")
            elif any(not self.data[n]["fresh"] for n in self.owned):
                self._stop("device_unavailable")

    def request(self, method, path, body=None, lease=""):
        with self.lock:
            self._tick()
            return copy.deepcopy(self._request(method, path, body or {}, lease))

    def _request(self, method, path, body, lease):
        if method == "POST" and path == "/devices/load/emergency-stop":
            if "load" in self.owned:
                self._stop("emergency_stop")
            confirmed = "load" not in self.disconnected
            self.data["load"]["writable"] = False
            if confirmed:
                self.data["load"]["output"] = False
                self.commands.append(("load", "output", False))
                self.stop_outputs[0] = False
            return dict(status="confirmed" if confirmed else "pending",
                        output_off_confirmed=confirmed, zero_current_confirmed=confirmed,
                        load_stop_pending=not confirmed)
        if method == "GET":
            if path == "/devices":
                return {"devices": list(self.data.values())}
            if path.startswith("/devices/") and path.split("/")[-1] in self.data:
                return self.data[path.split("/")[-1]]
            if path == "/session":
                return dict(active=bool(self.token), last_stop_reason=self.last_stop, stop_outputs=self.stop_outputs)
        if method == "POST" and path == "/session":
            if self.token:
                raise BridgeError("session_busy", 409)
            devices = body.get("devices", [])
            if not devices or len(set(devices)) != len(devices) or any(n not in self.data for n in devices):
                raise BridgeError("invalid_device_selection", 400)
            if any(not self.data[n]["fresh"] or self.data[n]["output"] is not False or not self.data[n]["writable"] for n in devices):
                raise BridgeError("device_not_ready_or_output_not_off", 409)
            self.owned = list(devices)
            self.token = secrets.token_hex(16)
            self.deadline = self.clock()+10
            return dict(status="confirmed", lease=self.token, expires_in_ms=10000)
        if method == "POST":
            if not self.token or lease != self.token:
                raise BridgeError("invalid_or_expired_lease", 409)
            if path == "/session/heartbeat":
                self.deadline = self.clock()+10
                return dict(status="confirmed")
            if path == "/session/stop":
                self._stop("client_stop")
                return dict(active=False, last_stop_reason=self.last_stop, stop_outputs=self.stop_outputs)
            parts = path.split("/")
            if len(parts) == 4 and parts[1] == "devices" and parts[3] == "command":
                name = parts[2]
                if name not in self.owned:
                    raise BridgeError("device_not_owned", 409)
                d = self.data[name]
                if len(body) != 1:
                    raise BridgeError("one_command_per_request", 400)
                key, value = next(iter(body.items()))
                if key not in d["capabilities"]:
                    raise BridgeError("unsupported", 422)
                if key == "output":
                    if type(value) is not bool:
                        raise BridgeError("boolean_required", 400)
                elif type(value) not in (int, float) or not math.isfinite(value) or value < 0:
                    raise BridgeError("finite_nonnegative_number_required", 422)
                v = value if key == "voltage_v" else d["set_voltage_v"] if name != "load" else d["voltage_v"]
                i = value if key == "current_a" else d["set_current_a"]
                lim = d["limits"]
                if not (key == "output" and value is False) and (v>lim["voltage_v"] or i>lim["current_a"] or v*i>lim["power_w"]):
                    raise BridgeError("limit_exceeded", 422)
                self.deadline = self.clock()+10
                d[key if key == "output" else "set_"+key] = value
                self.commands.append((name, key, value))
                return dict(status="confirmed")
        raise BridgeError("not_found", 404)

    def devices(self):
        return self.request("GET", "/devices")["devices"]

    def device(self, name):
        return self.request("GET", "/devices/"+name)

    def session(self, devices):
        return Session(self, devices)

"""Read-only native USB provider using the official optional Yoctopuce SDK.

It implements the same generated discovery/measurement operations as a node.
The SDK owns USB framing and calibration; it is never treated as a serial port.
"""

import json
import math
import re
import threading
import time
from types import SimpleNamespace
from urllib.parse import urlsplit
import uuid
from labbridge_generated.rest import RESTResponse

CHANNELS = {
    "voltage": ("voltage", "V"),
    "current": ("current", "A"),
    "power": ("power", "W"),
    "voltage-ac": ("voltage_ac_rms", "V"),
    "current-ac": ("current_ac_rms", "A"),
    "power-factor": ("power_factor", "1"),
}


class OfficialUsbBackend:
    _lock = threading.RLock()
    _users = 0

    def __init__(self, serial):
        try:
            from yoctopuce.yocto_api import YAPI, YRefParam, YModule
            from yoctopuce.yocto_voltage import YVoltage
            from yoctopuce.yocto_current import YCurrent
            from yoctopuce.yocto_power import YPower
        except ImportError as exc:
            raise RuntimeError(
                "Install the optional USB backend: pip install -e 'host[yocto]'"
            ) from exc
        self.api, self.serial, self.closed = YAPI, serial, False
        with self._lock:
            if not type(self)._users:
                error = YRefParam()
                if YAPI.RegisterHub("usb", error) != YAPI.SUCCESS:
                    raise OSError(str(error.value))
            type(self)._users += 1
            self.module = YModule.FindModule(serial)
            self.functions = {
                "voltage": (YVoltage.FindVoltage(serial + ".voltage1"), "V DC", 1),
                "current": (YCurrent.FindCurrent(serial + ".current1"), "mA DC", 0.001),
                "power": (YPower.FindPower(serial + ".power"), "W", 1),
                "voltage-ac": (YVoltage.FindVoltage(serial + ".voltage2"), "V AC", 1),
                "current-ac": (
                    YCurrent.FindCurrent(serial + ".current2"),
                    "mA AC",
                    0.001,
                ),
            }

    def read(self):
        with self._lock:
            if self.closed or self.module.load(100) != self.api.SUCCESS:
                raise OSError("Yocto-Watt is offline")
            if (
                self.module.get_serialNumber() != self.serial
                or self.module.get_productId() != 228
            ):
                raise OSError("Yocto-Watt identity mismatch")
            values = {}
            for name, (sensor, unit, scale) in self.functions.items():
                try:
                    if (
                        sensor.load(100) != self.api.SUCCESS
                        or sensor.get_sensorState() != 0
                        or sensor.get_unit() not in (unit, unit.split(" ")[0])
                    ):
                        raise OSError("Sensor unavailable or unit mismatch")
                    if hasattr(sensor, "get_enabled") and sensor.get_enabled() != 1:
                        raise OSError("Sensor input disabled")
                    value = sensor.get_currentValue() * scale
                    if not math.isfinite(value) or abs(value) > 1e12:
                        raise OSError("Invalid SDK measurement")
                    values[name] = value
                except Exception:
                    if name in ("voltage", "current", "power"):
                        raise
            power = self.functions["power"][0]
            pf = power.get_powerFactor()
            if math.isfinite(pf) and -1 <= pf <= 1:
                values["power-factor"] = pf
            return {
                "serial": self.serial,
                "uptime_ms": self.module.get_upTime(),
                "values": values,
            }

    def close(self):
        with self._lock:
            if self.closed:
                return
            self.closed = True
            type(self)._users -= 1
            if not type(self)._users:
                self.api.UnregisterHub("usb")


class YoctoUsbTransport:
    def __init__(self, serial, *, backend=None):
        if not re.fullmatch(r"YWATMK2C-[A-Za-z0-9]{1,16}", serial):
            raise ValueError("Use the Yocto-Watt hardware serial, not a logical name")
        self.serial = serial
        self.backend = backend or OfficialUsbBackend(serial)
        self.lock = threading.RLock()
        self.boot = uuid.uuid4().hex
        self.values, self.sequence, self.uptime = {}, 0, None
        self.received, self.online, self.lost = 0.0, False, False

    def close(self):
        self.backend.close()

    @staticmethod
    def reply(data, status=200):
        return RESTResponse(
            SimpleNamespace(
                status=status,
                reason="",
                headers={
                    "Content-Type": (
                        "application/problem+json"
                        if status >= 400
                        else "application/json"
                    )
                },
                data=json.dumps(data).encode(),
            )
        )

    def poll(self):
        try:
            frame = self.backend.read()
            if frame["serial"] != self.serial or not all(
                math.isfinite(v) and abs(v) < 1e12 for v in frame["values"].values()
            ):
                raise OSError("Invalid identity/value")
            if self.uptime is not None and (
                self.lost or frame["uptime_ms"] < self.uptime
            ):
                self.boot = uuid.uuid4().hex
            self.values = frame["values"]
            self.uptime = frame["uptime_ms"]
            self.received = time.monotonic()
            self.sequence += 1
            self.online = True
            self.lost = False
        except Exception:
            self.online = False
            self.lost = True

    def reading(self, identifier):
        quantity, unit = CHANNELS[identifier]
        valid = self.online and identifier in self.values
        result = dict(
            device_id="meter",
            component_id=identifier,
            boot_id=self.boot,
            quantity=quantity,
            unit=unit,
            quality="valid" if valid else "unavailable",
            sequence=self.sequence,
            received_ms=int(self.received * 1000),
            age_ms=max(0, int((time.monotonic() - self.received) * 1000)),
        )
        if valid:
            result["value"] = self.values[identifier]
        return result

    def request(self, method, url, headers=None, body=None, **kwargs):
        with self.lock:
            if method != "GET":
                return self.reply(
                    dict(
                        type="about:blank",
                        title="Read-only USB meter",
                        status=405,
                        detail="No actuator operations on this provider",
                    ),
                    405,
                )
            self.poll()
            device = dict(
                id="meter",
                driver="yocto_watt",
                transport="usb",
                identity=self.serial,
                state="ready" if self.online else "unavailable",
                output="off",
                capabilities=[
                    "components",
                    "voltage.measure",
                    "current.measure",
                    "power.measure",
                ],
            )
            path = urlsplit(url).path
            if path == "/api/v1/node":
                return self.reply(
                    dict(
                        id="yocto-" + self.serial,
                        boot_id=self.boot,
                        board="host-usb",
                        firmware_version="yoctopuce-usb-adapter",
                        api_version="0.1.0",
                    )
                )
            if path == "/api/v1/configuration":
                return self.reply(
                    dict(
                        revision=1,
                        node_id="yocto-" + self.serial,
                        board="host-usb",
                        devices=[
                            dict(
                                id="meter",
                                driver="yocto_watt",
                                transport="usb",
                                endpoint=self.serial,
                            )
                        ],
                    )
                )
            if path == "/api/v1/devices":
                return self.reply(dict(devices=[device]))
            prefix = "/api/v1/devices/meter"
            if path == prefix:
                return self.reply(device)
            if path == prefix + "/components":
                return self.reply(
                    dict(
                        components=[
                            dict(id=k, kind="sensor", actions=[], quantity=q, unit=u)
                            for k, (q, u) in CHANNELS.items()
                        ]
                    )
                )
            if path == prefix + "/readings":
                return self.reply(
                    dict(
                        device_id="meter",
                        boot_id=self.boot,
                        readings=[self.reading(k) for k in CHANNELS],
                    )
                )
            if path == prefix + "/measurements":
                result = dict(
                    device_id="meter",
                    boot_id=self.boot,
                    sequence=self.sequence,
                    received_ms=int(self.received * 1000),
                    age_ms=max(0, int((time.monotonic() - self.received) * 1000)),
                    quality="valid" if self.online else "unavailable",
                    temperature_channels=[],
                )
                for k, key in [
                    ("voltage", "voltage_v"),
                    ("current", "current_a"),
                    ("power", "power_w"),
                ]:
                    if self.online and k in self.values:
                        result[key] = self.values[k]
                return self.reply(result)
            if path.startswith(prefix + "/components/") and path.endswith(
                "/measurement"
            ):
                identifier = path.split("/")[-2]
                if identifier in CHANNELS:
                    return self.reply(self.reading(identifier))
            return self.reply(
                dict(
                    type="about:blank",
                    title="Not found",
                    status=404,
                    detail="Unknown read-only meter route",
                ),
                404,
            )

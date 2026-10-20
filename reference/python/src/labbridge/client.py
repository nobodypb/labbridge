from __future__ import annotations

import json
import threading
import time
import urllib.error
import urllib.request
from typing import Any


class BridgeError(RuntimeError):
    def __init__(self, message: str, status: int = 0):
        super().__init__(message)
        self.status = status


class UsbResponseTimeout(BridgeError):
    """The serial console did not return the requested response in time."""


class Client:
    def __init__(self, url: str, token: str, timeout: float = 8):
        if not url.startswith(("http://", "https://")):
            raise ValueError("URL must start with http:// or https://")
        self.url, self.token, self.timeout = url.rstrip("/"), token, timeout
        # Never send the bearer token through an environment-configured HTTP proxy.
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def request(self, method: str, path: str, body: dict | None = None,
                lease: str = "") -> dict[str, Any]:
        data = None if body is None else json.dumps(body, allow_nan=False).encode()
        headers = {"Authorization": f"Bearer {self.token}", "Content-Type": "application/json"}
        if lease:
            headers["X-Lab-Lease"] = lease
        req = urllib.request.Request(self.url + "/api/v1" + path, data=data,
                                     headers=headers, method=method)
        try:
            with self.opener.open(req, timeout=self.timeout) as response:
                return json.load(response)
        except urllib.error.HTTPError as exc:
            try:
                message = json.load(exc).get("error", str(exc.code))
            except (ValueError, AttributeError):
                message = str(exc.code)
            raise BridgeError(message, exc.code) from exc
        except (OSError, ValueError) as exc:
            raise BridgeError(f"Communication failed: {exc}") from exc

    def devices(self) -> list[dict]:
        return self.request("GET", "/devices")["devices"]

    def device(self, device_id: str) -> dict:
        if device_id not in ("load", "psu1", "psu2"):
            raise ValueError("Unknown device")
        return self.request("GET", f"/devices/{device_id}")

    def session(self, devices: list[str]) -> Session:
        return Session(self, devices)


class SerialClient:
    """Persistent USB console transport for the guarded bridge API."""

    def __init__(self, port: str, timeout: float = 8):
        try:
            import serial
        except ImportError as exc:
            raise BridgeError("USB transport needs pyserial: pip install 'labbridge[usb]'") from exc
        self.timeout = timeout
        self.lock = threading.Lock()
        self.sequence = 0
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
        # Native ESP32-S3 USB Serial/JTAG needs DTR asserted. On classic
        # ESP32 UART adapters it can hold GPIO0/BOOT low for the whole session.
        native_usb = port.startswith("/dev/ttyACM")
        try:
            from serial.tools import list_ports
            for info in list_ports.comports():
                if info.device == port and info.vid is not None:
                    native_usb = info.vid == 0x303A
                    break
        except (ImportError, AttributeError):
            pass
        self.port.dtr = native_usb
        self.port.rts = False
        self.port.port = port
        try:
            self.port.open()
            # Opening some CH340/CP210x UART adapters pulses ESP32 EN. Wait
            # until the bootloader has handed UART0 to the application.
            if not native_usb:
                time.sleep(2)
            self.port.reset_input_buffer()
        except Exception as exc:
            self.port.close()
            raise BridgeError(f"Cannot open USB port {port}: {exc}") from exc

    def close(self):
        self.port.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def request(self, method: str, path: str, body: dict | None = None,
                lease: str = "") -> dict[str, Any]:
        if method not in ("GET", "POST") or not path.startswith("/"):
            raise ValueError("Expected GET or POST and an API path starting with /")
        if (method == "GET" and body is not None) or (method == "POST" and not isinstance(body, dict)):
            raise ValueError("GET has no body; POST requires a JSON object")
        repeatable = method == "GET" or (method == "POST" and path == "/diagnostics/load/probe")
        with self.lock:
            for attempt in range(2 if repeatable else 1):
                try:
                    return self._request_once(method, path, body, lease)
                except UsbResponseTimeout:
                    if attempt or not repeatable:
                        raise
                    # A full probe and GET endpoints have no mutations. Drop
                    # any damaged partial line before repeating with a new ID.
                    self.port.reset_input_buffer()

    def _request_once(self, method: str, path: str, body: dict | None,
                      lease: str) -> dict[str, Any]:
        self.sequence += 1
        request_id = self.sequence
        frame = {"id": request_id, "op": "request", "method": method, "path": path}
        if body is not None:
            frame["body"] = body
        if lease:
            frame["lease"] = lease
        data = (json.dumps(frame, allow_nan=False, separators=(",", ":")) + "\r\n").encode()
        if len(data) > 4096:
            raise ValueError("USB request exceeds 4096 bytes")
        try:
            # The ESP32 UART console may use only its small hardware FIFO.
            # Pace long JSON lines so a CH340/CP210x burst cannot drop bytes.
            for offset in range(0, len(data), 32):
                self.port.write(data[offset:offset + 32])
                self.port.flush()
                if offset + 32 < len(data):
                    time.sleep(.01)
            deadline = time.monotonic() + self.timeout
            pending = b""
            while time.monotonic() < deadline:
                pending += self.port.read(self.port.in_waiting or 1)
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    marker = line.find(b"LABJSON ")
                    if marker < 0:
                        continue
                    try:
                        reply = json.loads(line[marker + 8:])
                    except (UnicodeDecodeError, ValueError):
                        continue
                    if reply.get("id") != request_id:
                        continue
                    status = reply.get("status", 0)
                    result = reply.get("result")
                    if not isinstance(result, dict):
                        raise BridgeError("Malformed USB response")
                    if status != 200:
                        raise BridgeError(str(result.get("error", "USB request failed")), status)
                    return result
                if len(pending) > 65536:
                    raise BridgeError("USB response exceeds 65536 bytes")
            raise UsbResponseTimeout(f"USB response timed out after {self.timeout:g} s")
        except OSError as exc:
            raise BridgeError(f"USB communication failed: {exc}") from exc

    devices = Client.devices
    device = Client.device
    session = Client.session


class Session:
    """Exclusive control with a background heartbeat and best-effort stop on exit."""
    def __init__(self, client: Client | SerialClient, devices: list[str]):
        self.client, self.devices = client, devices
        self.lease = ""
        self.stop_event = threading.Event()
        self.failure: Exception | None = None
        self.stop_result: dict | None = None
        self.worker: threading.Thread | None = None

    def __enter__(self):
        self.lease = self.client.request("POST", "/session", {"devices": self.devices})["lease"]
        self.worker = threading.Thread(target=self._heartbeat, daemon=True)
        self.worker.start()
        return self

    def _heartbeat(self):
        while not self.stop_event.wait(2):
            try:
                self.client.request("POST", "/session/heartbeat", {}, self.lease)
            except Exception as exc:
                self.failure = exc
                return

    def check(self):
        if self.failure:
            raise BridgeError(f"Heartbeat failed: {self.failure}")

    def command(self, device_id: str, **command):
        self.check()
        if device_id not in self.devices or len(command) != 1:
            raise ValueError("One command for an owned device is required")
        result = self.client.request("POST", f"/devices/{device_id}/command", command, self.lease)
        if result.get("status") != "confirmed":
            raise BridgeError("Device did not confirm command")
        return result

    def __exit__(self, exc_type, exc, tb):
        self.stop_event.set()
        if self.worker:
            self.worker.join(timeout=self.client.timeout + 1)
        stop_error = None
        try:
            self.stop_result = self.client.request("POST", "/session/stop", {}, self.lease)
        except Exception as error:
            stop_error = error
            try:
                self.stop_result = self.client.request("GET", "/session")
            except Exception:
                self.stop_result = {"last_stop_reason": "unreachable", "stop_outputs": [None]*3}
        selected = [("load", "psu1", "psu2").index(d) for d in self.devices]
        outputs = self.stop_result.get("stop_outputs", [None]*3)
        if "load" in self.devices and outputs[0] is not False:
            try:
                emergency = self.client.request("POST", "/devices/load/emergency-stop", {})
                self.stop_result["emergency_stop"] = emergency
                if (emergency.get("output_off_confirmed") is True and
                        emergency.get("zero_current_confirmed") is True):
                    outputs = list(outputs)
                    outputs[0] = False
                    self.stop_result["stop_outputs"] = outputs
            except Exception as error:
                self.stop_result["emergency_stop_error"] = str(error)
        if exc is None and (stop_error or any(outputs[i] is not False for i in selected)):
            raise BridgeError("Shutdown not confirmed; check device outputs") from stop_error
        return False

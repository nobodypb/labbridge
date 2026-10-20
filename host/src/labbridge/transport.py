"""USB carries the same HTTP-shaped contract, with correlated JSON lines."""

import json
import threading
import time
import uuid
from types import SimpleNamespace
from urllib.parse import urlsplit
from labbridge_generated.rest import RESTResponse


class SerialTransport:
    def __init__(self, path=None, *, port=None, timeout=12.0):
        if port is None:
            import serial
            from serial.tools import list_ports

            native = path.startswith("/dev/ttyACM")
            for info in list_ports.comports():
                if info.device == path and info.vid is not None:
                    native = info.vid == 0x303A
            port = serial.Serial(
                port=None, baudrate=115200, timeout=0.1, write_timeout=1
            )
            port.dtr = native
            port.rts = False
            port.port = path
            port.open()
            if not native:
                time.sleep(2)
            port.reset_input_buffer()
        self.port, self.timeout = port, timeout
        self.lock = threading.Lock()
        self.buffer = bytearray()

    def request(self, method, url, headers=None, body=None, **kwargs):
        request_id = uuid.uuid4().hex
        path = urlsplit(url).path
        if urlsplit(url).query:
            path += "?" + urlsplit(url).query
        envelope = dict(
            id=request_id, method=method, path=path, headers=headers or {}, body=body
        )
        timeout = kwargs.get("_request_timeout") or self.timeout
        if isinstance(timeout, tuple):
            timeout = sum(timeout)
        with self.lock:
            self.port.write(
                (json.dumps(envelope, separators=(",", ":")) + "\r\n").encode()
            )
            self.port.flush()
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                self.buffer.extend(self.port.readline())
                if len(self.buffer) > 65536:
                    self.buffer.clear()
                    raise ValueError("USB response exceeds limit")
                if b"\n" not in self.buffer:
                    continue
                line, _, remaining = self.buffer.partition(b"\n")
                self.buffer = bytearray(remaining)
                candidates = [line]
                offset = 0
                while True:
                    at = line.find(b"LABJSON ", offset)
                    if at < 0:
                        break
                    candidates.append(line[at + 8 :])
                    offset = at + 8
                reply = None
                for candidate in candidates:
                    try:
                        parsed = json.loads(candidate)
                    except (ValueError, UnicodeError):
                        continue
                    if isinstance(parsed, dict) and parsed.get("id") == request_id:
                        reply = parsed
                        break
                if reply is None:
                    continue
                return RESTResponse(
                    SimpleNamespace(
                        status=reply["status"],
                        reason="",
                        headers={
                            "Content-Type": "application/json",
                            **reply.get("headers", {}),
                        },
                        data=json.dumps(reply["body"]).encode(),
                    )
                )
        # Never replay a write: caller may reconnect and query its operation/key.
        raise TimeoutError(f"USB response timeout for {request_id}")

    def close(self):
        self.port.close()

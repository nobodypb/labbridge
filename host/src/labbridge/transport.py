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
            port = serial.Serial(path, 115200, timeout=.1, write_timeout=1)
        self.port, self.timeout = port, timeout
        self.lock = threading.Lock()

    def request(self, method, url, headers=None, body=None, **kwargs):
        request_id = uuid.uuid4().hex
        path = urlsplit(url).path
        if urlsplit(url).query:
            path += '?' + urlsplit(url).query
        envelope = dict(id=request_id, method=method, path=path,
                        headers=headers or {}, body=body)
        timeout = kwargs.get('_request_timeout') or self.timeout
        if isinstance(timeout, tuple):
            timeout = sum(timeout)
        with self.lock:
            self.port.write((json.dumps(envelope, separators=(',', ':'))+'\n').encode())
            self.port.flush()
            deadline = time.monotonic()+timeout
            while time.monotonic() < deadline:
                line = self.port.readline()
                if len(line) > 65536:
                    raise ValueError('USB response exceeds limit')
                if line.startswith(b'LABJSON '):
                    line = line[8:]
                try:
                    reply = json.loads(line)
                except (ValueError, UnicodeError):
                    continue
                if not isinstance(reply, dict) or reply.get('id') != request_id:
                    continue
                return RESTResponse(SimpleNamespace(status=reply['status'], reason='',
                    headers={'Content-Type':'application/json', **reply.get('headers', {})},
                    data=json.dumps(reply['body']).encode()))
        # Never replay a write: caller may reconnect and query its operation/key.
        raise TimeoutError(f'USB response timeout for {request_id}')

    def close(self):
        self.port.close()

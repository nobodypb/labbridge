"""Small orchestration layer above the generated typed operations."""

import threading
import time
import uuid
import importlib
import inspect
import pkgutil
from contextlib import contextmanager
from labbridge_generated.api_client import ApiClient
from labbridge_generated.configuration import Configuration
from labbridge_generated.api.core_api import CoreApi
from labbridge_generated.api.components_api import ComponentsApi
from labbridge_generated.api.dl24m_api import Dl24mApi
from labbridge_generated.api.rd60xx_api import Rd60xxApi
from labbridge_generated.api.gpio_interlock_api import GpioInterlockApi
from labbridge_generated.api.ds18b20_api import Ds18b20Api
from labbridge_generated.models.lease_request import LeaseRequest
from labbridge_generated.models.stop_request import StopRequest
from .transport import SerialTransport


class OperationFailed(RuntimeError):
    def __init__(self, operation):
        self.operation = operation
        super().__init__(
            f"{operation.id}: {operation.status} at {operation.stage}; output={operation.final_output}"
        )


class Node:
    def __init__(self, url, token="", *, rest=None):
        config = Configuration(
            host=(
                "http://usb"
                if url.startswith("serial:")
                else (
                    "http://yocto-usb"
                    if url.startswith("yocto+usb://")
                    else url.rstrip("/")
                )
            )
        )
        config.access_token = token
        # Mutations must not be retried by urllib3 independently of the operation engine.
        config.retries = 0
        self.client = ApiClient(config)
        if rest is not None:
            self.client.rest_client = rest
        elif url.startswith("serial:"):
            self.client.rest_client = SerialTransport(url.removeprefix("serial:"))
        elif url.startswith("yocto+usb://"):
            from .yocto import YoctoUsbTransport

            self.client.rest_client = YoctoUsbTransport(
                url.removeprefix("yocto+usb://")
            )
        import labbridge_generated.api as package

        self.apis = {}
        for module in pkgutil.iter_modules(package.__path__):
            loaded = importlib.import_module(package.__name__ + "." + module.name)
            for name, cls in inspect.getmembers(loaded, inspect.isclass):
                if name.endswith("Api") and cls.__module__ == loaded.__name__:
                    self.apis[module.name.removesuffix("_api")] = cls(self.client)
        self.core = CoreApi(self.client)
        self.components = ComponentsApi(self.client)
        self.dl24m = Dl24mApi(self.client)
        self.rd60xx = Rd60xxApi(self.client)
        self.interlock = GpioInterlockApi(self.client)
        self.temperature = Ds18b20Api(self.client)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def wait(self, operation, timeout=12.0):
        identifier = operation if isinstance(operation, str) else operation.id
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            result = self.core.get_operation(identifier, _request_timeout=2.0)
            if result.status == "succeeded":
                return result
            if result.status in ("failed", "cancelled"):
                raise OperationFailed(result)
            time.sleep(0.05)
        raise TimeoutError(
            f"Operation {identifier} not terminal; query before another mutation"
        )

    def stop(self, device_id):
        return self.wait(
            self.core.stop_device(
                device_id, uuid.uuid4().hex, StopRequest(), _request_timeout=2.0
            )
        )

    def close(self):
        rest = self.client.rest_client
        if hasattr(rest, "close"):
            rest.close()
        elif hasattr(rest, "pool_manager"):
            rest.pool_manager.clear()


class LeaseLost(RuntimeError):
    pass


class Session:
    def __init__(self, nodes, leases):
        self.nodes, self.leases = nodes, leases
        self.error = None
        self.stop_event = threading.Event()
        self.worker = threading.Thread(target=self._heartbeat, daemon=True)
        self.worker.start()

    def _heartbeat(self):
        while not self.stop_event.wait(1):
            try:
                self.heartbeat()
            except Exception as exc:
                self.error = exc
                self.stop_event.set()

    def heartbeat(self):
        for name, lease in self.leases.items():
            if self.stop_event.is_set():
                return
            response = self.nodes[name].core.heartbeat_lease(
                lease.id, StopRequest(), _request_timeout=2.0
            )
            if response.state != "active":
                raise LeaseLost(f"{name}: lease no longer active")

    def token(self, name):
        self.check()
        return self.leases[name].id

    def check(self):
        if self.error:
            raise LeaseLost("Cluster heartbeat failed") from self.error

    def close(self):
        self.stop_event.set()
        self.worker.join(timeout=3)


class Cluster:
    def __init__(self, nodes):
        self.nodes = nodes

    @contextmanager
    def lease(self, devices):
        acquired, errors, session = {}, [], None
        try:
            for name in sorted(devices):
                acquired[name] = self.nodes[name].core.create_lease(
                    LeaseRequest(device_ids=devices[name], ttl_ms=10000),
                    _request_timeout=2.0,
                )
            session = Session(self.nodes, acquired)
            yield session
            session.check()
        finally:
            if session:
                session.close()
            for name, lease in reversed(list(acquired.items())):
                try:
                    self.nodes[name].core.stop_lease(
                        lease.id, StopRequest(), _request_timeout=2.0
                    )
                except Exception as exc:
                    errors.append(exc)
            # Do not hide shutdown failures: local lease expiry remains last resort.
            if errors:
                raise ExceptionGroup(
                    "One or more node shutdown requests failed", errors
                )

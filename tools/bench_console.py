#!/usr/bin/env python3
"""Hardware acceptance of correlated USB replies across short/long API responses.

Run with PYTHONPATH=host/src:host/generated and an exclusive serial: endpoint.
This checks real hardware. The default is read-only; optional lease checks reserve
an already stopped actuator and request STOP. It never enables an output.
Only aggregate results are printed, never deployment configuration or identities.
"""

import argparse
import json
import time

from labbridge import Node


def check(endpoint, samples, timeout, lease_device=None):
    if not endpoint.startswith("serial:"):
        raise ValueError("Use an explicitly selected serial: endpoint")
    with Node(endpoint) as node:
        identity = node.core.get_node(_request_timeout=timeout)
        started = time.monotonic()
        for _ in range(samples):
            configuration = node.core.get_configuration(_request_timeout=timeout)
            devices = node.core.list_devices(_request_timeout=timeout)
            current = node.core.get_node(_request_timeout=timeout)
            assert (current.id, current.boot_id) == (
                identity.id,
                identity.boot_id,
            ), "Node changed or rebooted during USB acceptance"
            assert {d.id for d in configuration.devices} == {
                d.id for d in devices.devices
            }, "Configuration/device discovery disagree"
            if lease_device:
                target = next(d for d in devices.devices if d.id == lease_device)
                assert (
                    target.state == "ready" and target.output == "off"
                ), "Lease check requires a ready actuator with confirmed OFF"
                # Reserving does not enable an output. A short TTL bounds an
                # uncertain create response; independent STOP remains available.
                try:
                    lease = node.core.create_lease(
                        lease_request={"device_ids": [lease_device], "ttl_ms": 1000},
                        _request_timeout=timeout,
                    )
                    node.core.stop_lease(
                        lease.id, stop_request={}, _request_timeout=timeout
                    )
                finally:
                    node.stop(lease_device)
        return {
            "status": "passed",
            "samples": samples,
            "read_requests": 1 + 3 * samples,
            "lease_cycles": samples if lease_device else 0,
            "elapsed_s": round(time.monotonic() - started, 3),
        }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--samples", type=int, default=20)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument(
        "--lease-device",
        help="Optional stopped actuator: test reserve/release and STOP",
    )
    args = parser.parse_args()
    if args.samples < 1 or args.timeout <= 0:
        parser.error("samples and timeout must be positive")
    try:
        print(
            json.dumps(
                check(args.endpoint, args.samples, args.timeout, args.lease_device)
            )
        )
    except Exception as exc:
        print(json.dumps({"status": "failed", "error": type(exc).__name__}))
        raise SystemExit(1)

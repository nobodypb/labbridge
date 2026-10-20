#!/usr/bin/env python3
"""Read-only 60-minute AP/RD/BLE coexistence acceptance through the public API.

Run from the development venv with the host SDK installed. Results may contain
instrument serials; keep the output directory private. No leases or mutations.
"""

import argparse
import json
import os
import time
from pathlib import Path

from labbridge.sdk import Node


def check_sample(sample, boot_id, max_age):
    if sample.boot_id != boot_id:
        raise RuntimeError("Measurement boot changed")
    if sample.quality != "valid" or sample.age_ms > max_age:
        raise RuntimeError("Stale/unavailable measurement")
    if sample.voltage_v is None or sample.current_a is None:
        raise RuntimeError("Required voltage/current measurement missing")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--endpoint", default="serial:/dev/ttyACM0")
    p.add_argument("--token-env", default="LABBRIDGE_TOKEN")
    p.add_argument("--source", action="append", required=True)
    p.add_argument("--load", required=True)
    p.add_argument("--duration", type=float, default=3600)
    p.add_argument("--interval", type=float, default=1)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args(argv)
    if args.duration <= 0 or args.interval < 1:
        p.error("duration must be positive; interval must be at least one second")
    args.output.mkdir(parents=True, exist_ok=False)
    summary = {
        "passed": False,
        "read_only": True,
        "requested_duration_s": args.duration,
        "sources": args.source,
        "load": args.load,
        "samples": 0,
    }
    started = time.monotonic()
    try:
        with Node(args.endpoint, os.environ.get(args.token_env, "")) as node:
            identity = node.core.get_node()
            summary["node"] = identity.to_dict()
            devices = {d.id: d for d in node.core.list_devices().devices}
            if len(set(args.source + [args.load])) != len(args.source) + 1:
                raise RuntimeError("Sources and load must be distinct physical devices")
            for name in args.source:
                d = devices[name]
                if d.driver != "rd60xx" or d.transport != "tcp":
                    raise RuntimeError("Select configured TCP RD sources")
            if (
                devices[args.load].driver != "dl24m"
                or devices[args.load].transport != "ble"
            ):
                raise RuntimeError("Select the BLE DL24M for coexistence acceptance")
            wifi = node.core.get_wifi_state()
            summary["wifi"] = wifi.to_dict()
            if wifi.state != "connected":
                raise RuntimeError(
                    "Connect the ESP station to the lab WLAN before AP/STA/BLE acceptance"
                )
            original = node.rd60xx.get_rd_provisioning()
            if not original.listener_running:
                raise RuntimeError("RD AP listener is not running")
            for name in args.source:
                if not any(
                    c.serial == devices[name].identity and c.connected and c.configured
                    for c in original.candidates
                ):
                    raise RuntimeError(
                        "RD source is not connected/identified on the ESP AP"
                    )
            baseline = (original.metrics.disconnects, original.metrics.rejected)
            summary["initial_transport"] = original.to_dict()
            summary["configuration"] = node.core.get_configuration().to_dict()
            with (args.output / "samples.jsonl").open("w") as stream:
                observing = time.monotonic()
                previous_cycle = observing
                gap_limit = (
                    min(devices[d].limits.max_age_ms for d in args.source + [args.load])
                    / 1000
                )
                while time.monotonic() - observing < args.duration:
                    cycle = time.monotonic()
                    if cycle - previous_cycle > gap_limit:
                        raise RuntimeError(
                            "Host sampling gap exceeded the freshness limit"
                        )
                    previous_cycle = cycle
                    if node.core.get_wifi_state().state != "connected":
                        raise RuntimeError(
                            "Station WLAN disconnected during acceptance"
                        )
                    if node.core.get_node().boot_id != identity.boot_id:
                        raise RuntimeError("ESP restarted during acceptance")
                    for name in args.source + [args.load]:
                        sample = node.core.get_measurements(name)
                        check_sample(
                            sample, identity.boot_id, devices[name].limits.max_age_ms
                        )
                        stream.write(json.dumps(sample.to_dict()) + "\n")
                    transport = node.rd60xx.get_rd_provisioning()
                    if (
                        transport.metrics.disconnects,
                        transport.metrics.rejected,
                    ) != baseline:
                        raise RuntimeError("Unexpected RD connection loss/rejection")
                    stream.flush()
                    summary["samples"] += 1
                    remaining = args.interval - (time.monotonic() - cycle)
                    if remaining > 0:
                        time.sleep(remaining)
            summary["final_transport"] = node.rd60xx.get_rd_provisioning().to_dict()
            summary["passed"] = True
    except Exception as e:
        summary["error"] = f"{type(e).__name__}: {e}"
    finally:
        summary["elapsed_s"] = time.monotonic() - started
        (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({k: summary[k] for k in ("passed", "samples", "elapsed_s")}))
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

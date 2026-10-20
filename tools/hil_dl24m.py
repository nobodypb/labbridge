#!/usr/bin/env python3
"""Read-only DL24M audit; optional explicit bounded bench pulse and restore.

Install the local host packages first. JSON output contains no BLE address,
network name, token or device serial. This is not a battery-test procedure.
"""
import argparse
import json
from pathlib import Path
import time
import uuid
from labbridge.sdk import Cluster, Node
from labbridge_generated.models.current_request import CurrentRequest
from labbridge_generated.models.dl24m_settings import Dl24mSettings
from labbridge_generated.models.output_request import OutputRequest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="serial:/dev/ttyACM0")
    parser.add_argument("--device", default="load")
    parser.add_argument("--token-env", default="LABBRIDGE_TOKEN")
    parser.add_argument("--bench-profile", help="Enables a bounded bench pulse; JSON")
    parser.add_argument("--output", default="results/dl24m-audit.json")
    args = parser.parse_args()
    import os

    report = {"operations": [], "status": "running"}
    path = Path(args.output)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        parser.error("Output exists; choose a new path")
    with Node(args.endpoint, os.environ.get(args.token_env, "")) as node:
        try:
            device = node.core.get_device(args.device)
            if (
                device.driver != "dl24m"
                or device.output != "off"
                or device.state != "ready"
            ):
                raise RuntimeError("Ready DL24M with confirmed output OFF required")
            report["registers"] = node.dl24m.get_dl24m_registers(args.device).to_dict()
            if args.bench_profile:
                profile = json.loads(Path(args.bench_profile).read_text())
                voltage, current, duration = (
                    profile["expected_voltage_v"],
                    profile["pulse_current_a"],
                    profile["pulse_s"],
                )
                if not (
                    0 < voltage <= device.limits.voltage_v
                    and 0 < current <= min(0.2, device.limits.current_a)
                    and voltage * current <= device.limits.power_w
                    and 0.1 <= duration <= 2
                ):
                    raise ValueError("Pulse exceeds bench/bridge limits")
                sample = node.core.get_measurements(args.device)
                original = node.dl24m.get_dl24m_settings(args.device)
                if (
                    sample.quality != "valid"
                    or sample.age_ms > 3000
                    or abs(sample.voltage_v - voltage) > voltage * 0.05
                    or original.mode != 0
                    or sample.set_current_a is None
                    or original.overvoltage_v is None
                ):
                    raise RuntimeError(
                        "Expected bench voltage and fresh CC snapshot required"
                    )
                report["original"] = {
                    "current_a": sample.set_current_a,
                    "overvoltage_v": original.overvoltage_v,
                }
                path.write_text(json.dumps(report, indent=2) + "\n")
                with Cluster({"bench": node}).lease(
                    {"bench": [args.device]}
                ) as session:

                    def invoke(method, **body):
                        started = time.monotonic()
                        op = node.wait(
                            method(
                                device_id=args.device,
                                x_lab_lease=session.token("bench"),
                                idempotency_key=uuid.uuid4().hex,
                                **body
                            )
                        )
                        report["operations"].append(
                            {**op.to_dict(), "elapsed_s": time.monotonic() - started}
                        )
                        return op

                    try:
                        invoke(
                            node.dl24m.set_dl24m_settings,
                            dl24m_settings=Dl24mSettings(
                                overvoltage_v=min(
                                    device.limits.voltage_v, voltage * 1.05
                                )
                            ),
                        )
                        invoke(
                            node.dl24m.set_load_current,
                            current_request=CurrentRequest(current_a=current),
                        )
                        invoke(
                            node.core.set_output,
                            output_request=OutputRequest(
                                enabled=True, max_duration_ms=3000
                            ),
                        )
                        time.sleep(duration)
                        report["pulse"] = node.core.get_measurements(
                            args.device
                        ).to_dict()
                        measured = report["pulse"]
                        if (
                            measured["quality"] != "valid"
                            or measured["age_ms"] > 3000
                            or abs(measured["current_a"] - current)
                            > max(0.005, current * 0.15)
                        ):
                            raise RuntimeError("Pulse current was not confirmed")
                    finally:
                        # STOP does not depend on the session remaining valid.
                        node.stop(args.device)
                        invoke(
                            node.dl24m.set_load_current,
                            current_request=CurrentRequest(
                                current_a=sample.set_current_a
                            ),
                        )
                        invoke(
                            node.dl24m.set_dl24m_settings,
                            dl24m_settings=Dl24mSettings(
                                overvoltage_v=original.overvoltage_v
                            ),
                        )
            report["status"] = "succeeded"
        except Exception as exc:
            report.update(status="failed", error=str(exc))
            raise
        finally:
            report["final_output"] = node.core.get_device(args.device).output
            path.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({"status": report["status"], "final_output": report["final_output"]})
    )


if __name__ == "__main__":
    main()

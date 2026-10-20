import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys
import time
from .client import Client, SerialClient, BridgeError
from .simulator import Simulator
from .sweep import sweep


def main():
    p = argparse.ArgumentParser(description="Local laboratory bridge")
    p.add_argument("--url", default="http://labbridge.local")
    p.add_argument("--serial", default=os.environ.get("LABBRIDGE_SERIAL_PORT"),
                   metavar="PORT", help="USB serial port, e.g. /dev/ttyACM0 or COM5")
    p.add_argument("--timeout", type=float, default=8., help="Response timeout in seconds")
    p.add_argument("--token-env", default="LABBRIDGE_TOKEN", help="Environment variable holding API token")
    sub = p.add_subparsers(dest="action", required=True)
    sub.add_parser("devices")
    sub.add_parser("discover")
    sub.add_parser("stop-load", help="Absolute DL24M BLE OFF without a lease; retries persist on the bridge")
    setting = sub.add_parser("test-load-setting", help="USB-only reversible OFF/CC setting test with automatic restoration")
    setting.add_argument("setting", choices=[
        "cutoff_voltage", "discharge_time", "working_brightness", "standby_brightness",
        "standby_time", "current_resolution_1", "current_resolution_2",
        "overvoltage", "overcurrent", "overpower", "overtemperature",
        "fan_temperature", "battery_overtemperature", "mode_1", "mode_2", "mode_3",
        "setpoint_mode_1", "setpoint_mode_2", "setpoint_mode_3", "language"])
    set_setting = sub.add_parser("set-load-setting", help="Set an absolute DL24M BLE register while OFF/CC; confirms readback")
    set_setting.add_argument("setting", choices=[
        "cutoff_voltage", "discharge_time", "working_brightness", "standby_brightness",
        "standby_time", "current_resolution_1", "current_resolution_2",
        "overvoltage", "overcurrent", "overpower", "overtemperature",
        "fan_temperature", "battery_overtemperature",
        "setpoint_mode_1", "setpoint_mode_2", "setpoint_mode_3", "language"])
    set_setting.add_argument("wire_value", type=int, help="24-bit protocol value; see docs/api.md for bounds and readback units")
    counters = sub.add_parser("reset-load-counters", help="USB-only reset of time/capacity/energy, requires OFF/CC")
    counters.add_argument("--confirm-reset", action="store_true", required=True)
    get = sub.add_parser("get", help="Read an API path as JSON")
    get.add_argument("path", choices=["/devices", "/devices/load", "/devices/psu1",
                                     "/devices/psu2", "/discovery", "/config",
                                     "/session", "/diagnostics/load",
                                     "/diagnostics/network"])
    probe = sub.add_parser("probe-load", help="Read-only ATORCH protocol queries")
    probe.add_argument("--protocol", choices=["spp", "spp-legacy", "px100", "px100-v3"])
    probe.add_argument("--gap-ms", type=int, choices=range(501), metavar="0..500",
                       help="USB-only numeric BLE read diagnostic; default 0 ms")
    probe.add_argument("--query", choices=["all", "full", "output", "set_current",
                                           "voltage", "current", "temperature",
                                           "elapsed_time", "capacity", "energy",
                                           "cutoff_voltage", "discharge_time", "mode",
                                           *[f"setting_{r:02x}" for r in range(0x32, 0x3d)]],
                       default="all")
    watch = sub.add_parser("watch", help="Stream device readings as JSON Lines")
    watch.add_argument("device", choices=["load", "psu1", "psu2"])
    watch.add_argument("--interval", type=float, default=1.)
    watch.add_argument("--count", type=int, default=0, help="0 means until Ctrl-C")
    config = sub.add_parser("configure")
    config.add_argument("file", type=Path)
    commission = sub.add_parser("commission-load")
    commission.add_argument("--source-physically-disconnected", action="store_true")
    commission.add_argument("--cc-mode-selected", action="store_true")
    commission.add_argument("--display-scaling-checked", action="store_true")
    commission.add_argument("--bench-24v-1a", action="store_true",
                            help="USB DL24M BLE commissioning with a bounded 0.100 A pulse; source 24 V / limit 1 A")
    run = sub.add_parser("sweep", help="Simulated by default; --execute enables real devices")
    run.add_argument("--execute", action="store_true")
    run.add_argument("--source", choices=["psu1", "psu2", "none"], default="psu1")
    run.add_argument("--points", default="0.1,0.2,0.3")
    run.add_argument("--voltage", type=float, default=12.)
    run.add_argument("--supply-current", type=float, default=0.5)
    run.add_argument("--max-voltage", type=float, default=15.)
    run.add_argument("--max-current", type=float, default=0.5)
    run.add_argument("--max-power", type=float, default=6.)
    run.add_argument("--settle", type=float, default=2.)
    run.add_argument("--samples", type=int, default=3)
    run.add_argument("--output", type=Path)
    args = p.parse_args()
    try:
        if args.action == "sweep" and not args.execute:
            client = Simulator(source=None if args.source == "none" else args.source)
            for d in client.data.values():
                d["limits"] = dict(voltage_v=args.max_voltage, current_a=args.max_current, power_w=args.max_power)
            print("SIMULATION — no network or hardware access")
        else:
            if args.timeout <= 0:
                raise ValueError("--timeout must be positive")
            if args.action == "probe-load" and args.query == "full":
                args.timeout = max(args.timeout, 60.)
            if args.action == "commission-load" and args.bench_24v_1a:
                args.timeout = max(args.timeout, 60.)
            if args.action in ("test-load-setting", "set-load-setting", "reset-load-counters"):
                args.timeout = max(args.timeout, 60.)
            if args.serial:
                client = SerialClient(args.serial, timeout=args.timeout)
            else:
                token = os.environ.get(args.token_env)
                if not token:
                    raise ValueError(f"Set {args.token_env} to the API token")
                client = Client(args.url, token, timeout=args.timeout)
        if args.action == "devices":
            result = client.devices()
        elif args.action == "discover":
            result = client.request("GET", "/discovery")
        elif args.action == "stop-load":
            result = client.request("POST", "/devices/load/emergency-stop", {})
        elif args.action == "test-load-setting":
            result = client.request("POST", "/diagnostics/load/setting-check",
                                    {"setting": args.setting})
        elif args.action == "set-load-setting":
            result = client.request("POST", "/devices/load/setting",
                                    {"setting": args.setting, "wire_value": args.wire_value})
        elif args.action == "reset-load-counters":
            result = client.request("POST", "/diagnostics/load/reset-counters",
                                    {"confirm_reset": args.confirm_reset})
        elif args.action == "get":
            result = client.request("GET", args.path)
        elif args.action == "probe-load":
            body = {"query": args.query}
            if args.protocol:
                body["protocol"] = args.protocol
            if args.gap_ms is not None:
                body["command_gap_ms"] = args.gap_ms
            result = client.request("POST", "/diagnostics/load/probe", body)
        elif args.action == "watch":
            if args.interval <= 0 or args.count < 0:
                raise ValueError("--interval must be positive; --count must be nonnegative")
            index = 0
            while not args.count or index < args.count:
                print(json.dumps(client.device(args.device), separators=(",", ":")), flush=True)
                index += 1
                if not args.count or index < args.count:
                    time.sleep(args.interval)
            return 0
        elif args.action == "configure":
            result = client.request("POST", "/config", json.loads(args.file.read_text()))
        elif args.action == "commission-load":
            checks = (args.source_physically_disconnected, args.cc_mode_selected,
                      args.display_scaling_checked)
            if args.bench_24v_1a:
                if any(checks):
                    raise ValueError("Choose bench commissioning or disconnected-source checks")
                body = {"bench_24v_1a": True}
            elif all(checks):
                body = dict(source_physically_disconnected=True,
                            cc_mode_selected=True, display_scaling_checked=True)
            else:
                raise ValueError("Commissioning requires all three disconnected-source checks or --bench-24v-1a")
            result = client.request("POST", "/commission/load", body)
        else:
            dest = args.output or Path("results")/(datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")+".csv")
            result = str(sweep(client, dest, [float(x) for x in args.points.split(",")],
                source=None if args.source == "none" else args.source, voltage=args.voltage,
                supply_current=args.supply_current, max_voltage=args.max_voltage, max_current=args.max_current,
                max_power=args.max_power, settle=args.settle, samples=args.samples))
        print(json.dumps(result, indent=2))
        if args.action == "probe-load" and not result.get("all_queries_answered"):
            return 2
        if args.action in ("commission-load", "test-load-setting", "set-load-setting",
                           "reset-load-counters") and result.get("passed") is False:
            return 2
        if args.action == "stop-load" and result.get("status") != "confirmed":
            return 2
    except (BridgeError, ValueError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Interrupted; check shutdown metadata.", file=sys.stderr)
        return 130
    finally:
        if "client" in locals() and isinstance(client, SerialClient):
            client.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())

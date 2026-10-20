from __future__ import annotations
import csv
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import time
from .client import BridgeError


def sweep(client, destination: Path, points: list[float], *, source="psu1", voltage=12.,
          supply_current=0.5, max_voltage=15., max_current=0.5, max_power=6.,
          settle=2., samples=3, interval=1.) -> Path:
    """CC load sweep. Configuration is checked before acquiring a session."""
    limits = (max_voltage, max_current, max_power)
    if any(not math.isfinite(x) or x <= 0 for x in limits):
        raise ValueError("Positive finite DUT voltage/current/power limits required")
    if source not in ("psu1", "psu2", None):
        raise ValueError("Source must be psu1, psu2 or None")
    if not points or any(not math.isfinite(x) or x < 0 or x>max_current or abs(x*100-round(x*100))>1e-6 for x in points):
        raise ValueError("Load points must be nonnegative, within DUT limits and multiples of 0.01 A")
    if not math.isfinite(settle) or settle<0 or not math.isfinite(interval) or interval<=0 or samples<1:
        raise ValueError("Invalid sampling parameters")
    if source and (not math.isfinite(voltage) or not math.isfinite(supply_current) or voltage<=0 or supply_current<=0
                   or voltage>max_voltage or supply_current>max_current or voltage*supply_current>max_power):
        raise ValueError("Supply settings exceed DUT limits")
    devices = ["load"] + ([source] if source else [])
    initial = {name: client.device(name) for name in devices}
    for name, d in initial.items():
        required = {"measure", "current_a", "output"} | ({"voltage_v"} if name != "load" else set())
        if not required.issubset(d["capabilities"]) or not d["fresh"] or d["output"] is not False:
            raise BridgeError(f"{name}: not ready; verify profile, fresh telemetry and output OFF")
        configured = d["limits"]
        # Hardware watchdog must enforce at least the requested DUT limits, even if this process dies.
        if any(configured[k] <= 0 or configured[k] > value for k, value in zip(("voltage_v", "current_a", "power_w"), limits)):
            raise BridgeError(f"{name}: configure bridge limits at or below DUT limits before this run")
        if max(points)>configured["current_a"]:
            raise BridgeError(f"{name}: current point exceeds configured limit")
        if source and (voltage>configured["voltage_v"] or voltage*max(points)>configured["power_w"]):
            raise BridgeError(f"{name}: requested sweep exceeds configured limit")
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() or destination.with_suffix(destination.suffix+".json").exists():
        raise FileExistsError(f"Measurement output already exists: {destination}")
    metadata = dict(started_utc=datetime.now(timezone.utc).isoformat(), devices=initial,
                    points_a=points, settle_s=settle, samples_per_point=samples,
                    max_voltage_v=max_voltage, max_current_a=max_current, max_power_w=max_power,
                    simulation=all(d["transport"] == "simulation" for d in initial.values()),
                    result="running")
    session = client.session(devices)
    last_sequence = {n: d["sequence"] for n,d in initial.items()}
    fields = ["host_utc", "point_a", "device", "identity", "sequence", "received_ms", "age_ms", "voltage_v", "current_a", "power_w", "temperature_c"]
    created = False
    try:
        # Exclusive creation: never overwrite earlier measurements.
        with destination.open("x", newline="", encoding="utf-8") as output:
            created = True
            writer = csv.DictWriter(output, fieldnames=fields)
            writer.writeheader()
            with session:
                session.command("load", current_a=points[0])
                if source:
                    session.command(source, current_a=0.)
                    session.command(source, voltage_v=voltage)
                    session.command(source, current_a=supply_current)
                    session.command(source, output=True)
                session.command("load", output=True)
                for point in points:
                    session.command("load", current_a=point)
                    baseline = {n: client.device(n)["sequence"] for n in devices}
                    time.sleep(settle)
                    for sample in range(samples):
                        session.check()
                        for name in devices:
                            deadline = time.monotonic()+4
                            while True:
                                session.check()
                                d = client.device(name)
                                if not d["fresh"]:
                                    raise BridgeError(f"Stale measurements from {name}")
                                if d["identity"] != initial[name]["identity"]:
                                    raise BridgeError(f"Device identity changed: {name}")
                                if d["sequence"] > max(last_sequence[name], baseline[name]):
                                    break
                                if time.monotonic()>=deadline:
                                    raise BridgeError(f"No new measurement from {name}")
                                time.sleep(min(interval, 0.1))
                            if any(d[k]>limit for k,limit in zip(("voltage_v", "current_a", "power_w"),limits)):
                                raise BridgeError(f"DUT limit exceeded: {name}")
                            last_sequence[name] = d["sequence"]
                            writer.writerow(dict(host_utc=datetime.now(timezone.utc).isoformat(), point_a=point, device=name,
                                **{k:d[k] for k in fields if k not in ("host_utc", "point_a", "device")}))
                            output.flush()
                        if sample+1<samples:
                            time.sleep(interval)
            metadata["result"] = "completed"
    except BaseException as exc:
        metadata["result"] = "aborted"
        metadata["error"] = str(exc)
        raise
    finally:
        metadata["ended_utc"] = datetime.now(timezone.utc).isoformat()
        metadata["shutdown"] = session.stop_result
        # Metadata is only written for a CSV created by this run.
        if created:
            meta = destination.with_suffix(destination.suffix+".json")
            with meta.open("x", encoding="utf-8") as out:
                json.dump(metadata, out, indent=2, allow_nan=False)
    return destination

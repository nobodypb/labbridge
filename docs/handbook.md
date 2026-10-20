# LabBridge hardware and interface handbook

LabBridge connects an automation host to laboratory instruments through ESP32
nodes. Device IDs are local to a node; a host uses `node-name/device-id`. Each
node runs the same application source, with a board build and a validated device
configuration. Scripts own experimental sequences; nodes own instrument access,
readback, local limits and shutdown obligations.

This is a migration in progress. Consult [acceptance](acceptance.md) and the
[backlog](backlog.md) before relying on a module with real hardware. The preserved
implementation is in `reference/`; new drivers live under `firmware/drivers/`.
No manufacturer firmware update is needed for the confirmed DL24M protocol.

## Hardware

| Board | Practical instrument connections | Constraints |
|---|---|---|
| M5Stack StickS3 / ESP32-S3 | BLE DL24M | Native USB remains the console. USB host disabled. No Bluetooth Classic. |
| ESP32-S3-USB-OTG | BLE and up to two USB serial RD6024 endpoints | Powered USB hub for two instruments; separate UART console. Internal USB PHY cannot serve console and host simultaneously. |
| Classic ESP32 devkit | UART; Classic Bluetooth hardware available | Numeric SPP integration remains experimental. No native USB host. |

Resource profiles reject excess connections, duplicate instrument identities
and shared pins before creating drivers. GPIO/1-Wire pins must be explicitly
approved in a board profile; an empty pin allowlist is intentional. Do not infer
that an exposed pin is free on a display/power-management board.

RD6024 USB links use 115200 baud, 8N1. The original adapter supports CH34x, CP210x
and FTDI converters. Bind a PSU by its register serial number, not enumeration
order or USB converter VID/PID. Two converters can have identical VID/PID.
A PSU's optional Bluetooth module is not automatically compatible with BLE;
its transport needs separate identification and commissioning.

GPIO reference interlocks require two normally-open paths and two independent
feedback inputs. Boot, stop and fault command both paths open. A change first
opens both, verifies feedback after a configurable break (minimum 20 ms), then
closes only the requested path and verifies feedback. Hardware must provide
appropriate isolation and contact ratings. No specific relay wiring has yet
been commissioned. DS18B20 probes need a validated bus, pull-up and probe ROM
identity. A device heatsink/internal sensor is never a cell-temperature probe.

## DL24M hardware and firmware

The photographed instrument is an ATORCH modular DC electronic load. Its PCB
is marked `DL24M-a`; the display reports firmware `Ver: 2.0`. Treat `-a` as a PCB
marking rather than a proven separate firmware family. Hardware compatibility
with downloadable DL24_M_V22 firmware is plausible, **not established**. Do not
flash it based only on the PCB suffix.

BLE uses service `FFE0`, characteristic `FFE1`, notification subscription through
CCCD `2902`. Prefer ATT Write Request when supported and wait for its completion
before another write. An ATT acknowledgement confirms transport delivery only.
The numeric protocol works over BLE on the tested display firmware, including
reads and reversible writes. Phone-app remote keys are not required for numeric
setpoints. Avoid calibration, factory reset and unknown special-test commands.

### Numeric wire format

All bytes below are hexadecimal. There is no CRC, application write ACK or
request ID. Values are unsigned 24-bit big-endian.

```
request: B1 B2 CMD VH VM VL 00 00 B6       (9 bytes)
reply:   CA CB CMD VH VM VL 00 00 CE CF    (10 bytes)
```

Example: set 0.100 A in CC mode:
`B1 B2 02 00 00 64 00 00 B6`; query setpoint:
`B1 B2 17 00 00 00 00 00 B6`.

| Function | Write | Read | Value / conversion |
|---|---:|---:|---|
| Absolute output | 01 | 10 | ON write **010000**, OFF write 000000; read 0/1 |
| Active mode setpoint | 02 | 17 | CC: mA; verify mode before interpreting other banks |
| Voltage | — | 11 | mV |
| Current | — | 12 | mA |
| Elapsed time | — | 13 | Packed HH MM SS |
| Capacity / energy | 05 resets counters | 14 / 15 | Raw units not independently calibrated; integrate host V/I |
| Device temperature | — | 16 | tenths °C; internal, not cell temperature |
| Discharge cutoff | 03 | 18 | write: V × 10; read: V × 100 |
| Duration | 04 | 19 | write seconds, minute resolution; read packed HH MM 00 |
| Mode bank | 06 | 21 | 0: CC; 1–3 separate banks, full active semantics unverified |
| Working brightness | 42 | 32 | ASCII '0'–'9' |
| Standby brightness | 43 | 33 | ASCII '0'–'9' |
| Standby time | 44 | 34 | Raw setting; units need confirmation |
| Current resolution | 45 | 35 | 0–2 |
| OVP | 47 | 37 | mV |
| OCP | 48 | 38 | mA |
| OPP | 49 | 39 | write centiwatts; read milliwatts |
| Device OTP | 4D | 3A | °C |
| Fan threshold | 4E | 3B | °C |
| Battery OTP threshold | 50 | 3C | °C; this is not PID Kp |
| Language | 41 | 31 | 0/1; read 31 enters settings |
| Leave settings | 4C | — | 0 |

There are **22 audited passive reads**: `10–19`, `21`, `32–3C`.
`22` reads/clears an event; `31` enters settings. Neither belongs in passive
polling. `36` is read-only diagnostic data of uncertain meaning. `46` and `4F`
are calibration-related; `4B` factory reset; `51/52` uncertain Ki/Kd. Commands
`07/08` have incomplete special-test semantics. They are not normal automation
capabilities.

Some notifications contain legacy `FF 55` telemetry alongside numeric replies.
Notifications may split a frame at any byte or insert a numeric reply into an
incomplete FF55 report. Preserve valid checksum FF55 frames intact; recover a
complete numeric reply from a corrupted prefix. Never use an incomplete frame,
old cached value or ATT ACK to confirm a write. The old six-byte numeric variant
is not a fallback for this device.

### Timing, recovery and shutdown

The PC v2.0.8 implementation waits about 60 ms before setpoint, cutoff, duration,
mode and menu-exit writes; one multi-parameter sequence separates writes by
200 ms. Its selected write handlers have no observed general retry mechanism.
The new transaction engine uses these write timings, immediate ordinary reads,
readback, bounded retries and a 10-second upper bound. A fixed 500 ms around every
request is not justified by parsing needs. The faster policy still needs the
new hardware acceptance described in [acceptance](acceptance.md).

No request ID means a sufficiently late reply to an identical read can be
ambiguous. Drain old receive data before a transaction, serialize requests per
endpoint and reject connection-generation changes. Treat timeout as unknown,
not as proof that the write did not execute. Never automatically replay ON
following a disconnect. Persist an OFF obligation before enabling; clear it
only after OFF readback. Persist original settings before a multi-step change;
on incomplete execution, stop and restore. Safety freshness stays at most
3 seconds during recovery, regardless of the transaction deadline.

## RD6024 protocol

Modbus RTU, address 1 on each independent cable. Function 03 reads holding
registers, function 06 writes one register. CRC16 polynomial A001, initial FFFF,
CRC low byte first. Validate address, function, payload length and CRC.

| Register | Meaning | Conversion |
|---:|---|---|
| 0 | model | raw / 10 = 6024 |
| 1–2 | serial | high word, low word |
| 4–5 | internal temperature | sign, magnitude °C |
| 8 / 9 | set voltage / current | /100 V, /100 A |
| 10 / 11 | measured voltage / current | /100 V, /100 A |
| 12–13 | measured power | combined 32-bit /100 W |
| 16 | protection | raw enum |
| 17 | CC/CV | probable 0=CV, 1=CC; commissioning required |
| 18 | output | 0/1 |

Before writing, re-read and match instrument serial/model. Require function-06
byte-for-byte echo, then fresh register readback with the same identity. The
external NTC channel is not mapped yet; internal temperature cannot substitute.

## Public API and USB

The authoritative contract is [OpenAPI](../api/openapi.json), assembled from
core and module fragments. `tools/generate.sh` produces Python, TypeScript and
C models plus firmware routes and validation constraints. Changes belong in the
fragments, not generated files. Modules declare transports and capabilities.

All instrument routes live below `/api/v1`. `GET /devices` lists local IDs and
capabilities; `GET /devices/{id}/measurements` returns quality, sequence and age.
An unavailable channel is absent or explicitly unavailable, never a plausible
zero. Time fields are monotonic milliseconds since the reported `boot_id`.
Never compare monotonic timestamps across nodes or reboot boundaries.

Reserve devices with `POST /leases`, heartbeat at least every second (TTL up to
10 seconds), and send `X-Lab-Lease` with writes. Use an `Idempotency-Key` per
logical mutation. A 202 response contains an operation ID and Location; poll to
a terminal state. Reusing the key with different content is a conflict. Keys
and operation IDs are scoped to one boot and bounded in memory. Do not replay
an uncertain write after reboot. `POST /devices/{id}/stop` needs no lease.
Errors use `application/problem+json`, including final output and restoration
state when known. A failed operation can still have changed the instrument.

HTTP uses bearer authentication on a trusted LAN. Provisioning on the open
`LabBridge-Setup` AP exposes only Wi-Fi setup/scan, not authenticated laboratory
controls. `/wifi` and `/devices` are separate linked pages. No AP password is
required; a target network's PSK is entered on `/wifi` and never returned.
USB is a physically trusted management interface and uses the same routes:

```json
{"id":"unique-host-id","method":"GET","path":"/api/v1/node","headers":{},"body":null}
```

One response line: `LABJSON {"id":"unique-host-id","status":200,"headers":{},"body":{...}}`.
Ignore boot logs and responses with another ID. The SDK serializes requests and
does not replay mutations on a response timeout. Configuration uses revisions
and `If-Match`; reject stale updates and changes while leased or outputs unsafe.

## Battery automation

External scripts coordinate named nodes with the generated clients and host
SDK. Distributed lease acquisition is not atomic: acquire in a stable order and
unwind already acquired nodes if a later acquisition fails. Heartbeat all nodes;
stop all on error. Each node must enforce its own limits without a host.

An explicit cell profile must name chemistry, series count, voltage/current/
power/temperature limits, charge termination, discharge cutoff, durations, rest
and pulse settings. Support begins with **1S Li-ion and 1S LiFePO4**. Example
profiles are commissioning templates, not universal cell specifications. Use
limits from the actual cell manufacturer. Require a fresh external cell probe,
a feedback interlock, a source and load with confirmed OFF before transitions.
Internal temperatures are useful device-health measurements only.

Charge uses source CC/CV and termination current plus a timeout; discharge uses
load CC and cutoff plus a timeout. Open both paths during rest. Current pulses
must respect node/profile limits. Record CSV samples and JSON run metadata,
including configuration, device identities, boot IDs and errors. Integrate
capacity/energy by trapezoids only across consecutive fresh samples with
acceptable gaps. Never integrate stale periods or infer zero current during
missing data. Raw instrument counters remain supplementary.

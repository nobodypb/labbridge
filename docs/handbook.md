# LabBridge hardware and interface handbook

LabBridge connects an automation host to laboratory instruments through ESP32
nodes. Device IDs are local to a node; a host uses `node-name/device-id`. Each
node runs the same application source, with a board build and a validated device
configuration. Scripts own experimental sequences; nodes own instrument access,
readback, local limits and shutdown obligations.

The DL24M driver is bench-tested; other hardware modules require commissioning. Consult [acceptance](acceptance.md) and the
[backlog](backlog.md) before relying on a module with real hardware. The preserved
implementation is in `reference/`; new drivers live under `firmware/drivers/`.
No manufacturer firmware update is needed for the confirmed DL24M protocol.

## Hardware

| Board | Practical instrument connections | Constraints |
|---|---|---|
| M5Stack StickS3 / ESP32-S3 | BLE DL24M and up to two AP TCP RD6018/RD6024 sources | Native USB remains the console. USB host disabled. No Bluetooth Classic. |
| ESP32-S3-USB-OTG | BLE, up to two AP TCP sources and up to two USB serial RD60xx endpoints | Powered USB hub for two instruments; separate UART console. Internal USB PHY cannot serve console and host simultaneously. |
| Classic ESP32 devkit | BLE and UART | Bluetooth Classic hardware exists, but SPP is not enabled in this runtime. No native USB host. |

Resource profiles reject excess connections, duplicate instrument identities
and shared pins before creating drivers. GPIO/1-Wire pins must be explicitly
approved in a board profile; an empty pin allowlist is intentional. Do not infer
that an exposed pin is free on a display/power-management board.

RD6018/RD6024 USB links use 115200 baud, 8N1. The original adapter supports CH34x, CP210x
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
request is not justified by parsing needs. The policy is bench-tested with bounded retries; see [acceptance](acceptance.md).
Foreground work waits for an in-flight background application reply. A timed-out
exchange is drained and followed by a 250 ms recovery quiet period. Normal
requests do not pay this recovery cost. Do not cancel a poll after its ATT ACK
and immediately overlap a new application request.

No request ID means a sufficiently late reply to an identical read can be
ambiguous. Drain old receive data before a transaction, serialize requests per
endpoint and reject connection-generation changes. Treat timeout as unknown,
not as proof that the write did not execute. Never automatically replay ON
following a disconnect. Persist an OFF obligation before enabling; clear it
only after OFF readback. Persist original settings before a multi-step change;
on incomplete execution, stop and restore. Safety freshness stays at most
3 seconds during recovery, regardless of the transaction deadline.

## RD6018 / RD6024 protocol

Modbus RTU, address 1 on each independent cable or TCP connection. Function 03 reads holding
registers, function 06 writes one register. CRC16 polynomial A001, initial FFFF,
CRC low byte first. Validate address, function, payload length and CRC.

| Register | Meaning | Conversion |
|---:|---|---|
| 0 | model/revision | RD6018: 60180..60184; RD6024: raw / 10 = 6024 |
| 1–2 | serial | high word, low word |
| 3 | firmware | e.g. 136 = 1.36 |
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

The ordinary RD6018 and RD6024 profiles use 0.01 V/A setpoint steps. RD6018
model ceilings are 60 V / 18 A / 1080 W; RD6024 ceilings are 60 V / 24 A /
1440 W. These ceilings do not guarantee that the input supply/cooling can deliver
that power. Explicit configured limits remain authoritative and can be lower.
RD6018P IDs 60185..60189 are rejected because their scaling differs. Configurations
without `rd60xx.model` retain the earlier RD6024 profile; select `6018` explicitly
for the user's RD6018 instruments. Unsupported models are rejected before writes.

### Original RD Wi-Fi modules on the ESP AP

The first deployment hosts sources on the ESP AP while the ESP is also a station
on the laboratory WLAN and a BLE central for DL24M. AP+STA is already enabled.
The AP is open, defaults to `LabBridge-Setup`, and has address `192.168.4.1`.
Optional `NodeConfiguration.ap_ssid` changes its name; the lab LAN must use a
different subnet. Four AP stations are permitted, including phones/PCs.

Original RIDEN Wi-Fi modules are TCP **clients**. They connect to the configured
server on port **8080**, carrying CRC-protected **Modbus RTU** bytes. There is no
MBAP header. LabBridge listens on its AP address; this delivery does not expose
that instrument socket on the station LAN. No NAT, cloud, MQTT broker or Internet
access is needed. The host accesses LabBridge through its existing LAN API or
trusted USB CLI.

Set each PSU's Modbus address to **1**. Bind each device by its decimal instrument
serial, never its DHCP address. Up to two configured TCP devices have independent
streams, connection generations and recovery. The listener permits four sockets,
probes each newcomer using only function 03, and remembers up to eight candidate
identities per boot. Unknown devices remain read-only. Duplicate live serials,
wrong configured models and unsupported models are rejected. Socket buffers and
identity timeouts are bounded; fragmented/coalesced TCP data is parsed by frame
length and CRC. A timed-out command discards its TCP stream to prevent a late
response from confirming a later command. Reconnection verifies identity and
requests OFF before allowing work; it never resumes ON.

### Pairing through LabBridge

**Commissioning status:** The RIDEN PC profile below paired a W01 RD6018 successfully from the ESP AP using WPA2 and an associated phone, including the displayed server address, UDP join result and TCP identity. Earlier general-profile open/WPA2 attempts failed. Open-network compatibility, which profile changes are essential, source control and the coexistence soak remain unvalidated. See [compatibility notes](uncertainties.md).

The AP is open by default. `core.configure_access_point` (`PUT /api/v1/wifi/ap`) accepts only `{"psk":"..."}`: 8..63 printable ASCII characters enable WPA2-PSK/CCMP; an explicit empty string restores the open AP. This requires quiescent instruments and authenticated HTTP or trusted USB, including when called through the open AP. The change disconnects AP clients after the response; use USB or the station address to retain management access. Station credentials and the configured AP SSID are unchanged. Secrets are never returned. `getWifiState.ap_secured` reports current authentication; poll after the delayed change. Supply secrets through a private CLI args file or stdin, not shell command history.

For protected provisioning, connect a phone/client to the AP and keep it associated during transmission. Both AP modes now use normal multicast UDP; on WPA2 the WLAN driver encrypts packets. Radio-completion counters belong to the older raw diagnostic path and remain zero for this sender. Complete-burst and join-ACK counters describe distinct stages; neither substitutes for TCP identity. PMF is not required, for compatibility with the original ESP8266 module.

Keep the target PSU output OFF and configure one module at a time. On the PSU,
select WiFi/address 1, reset the previous Wi-Fi setup and restart. In `/devices`,
enter the API token, open **RD60xx Wi-Fi setup**, confirm output OFF and start.
These setup operations require a bearer token or UI session with CSRF even on the open AP; USB
management is trusted. Existing leases/operations must be released and shutdown
obligations resolved. Wi-Fi scans/changes, configuration writes and new leases
are blocked while pairing; independent STOP remains available.

1. In `server_address`, the ESP transmits its AP address using the RIDEN
   two-stage ESPTouch convention. When the PSU shows `192.168.4.1` /
   **Connecting WiFi**, acknowledge **ESP address shown / Connecting WiFi**.
2. In `wifi`, the ESP sender transmits the AP SSID and configured
   AP password (empty for an open AP). When
   the PSU shows **Connecting server**, acknowledge **Connecting server shown**.
3. `waiting_connection` completes when a supported incoming identity has been
   seen. Each management phase has a 120-second deadline; the sender repeats bursts for at most 60 seconds, matching the PC. Cancellation stops transmission;
   settings already received by the PSU are not undone. Status includes AP client count/channel and separate radio submissions, successful transmission completions and failed completions. Successful broadcast transmission does not confirm PSU reception; use the display and eventual TCP identity. Old session IDs are
   rejected and repeated phase acknowledgements never skip a step.
4. Check the displayed model/serial. Enter a device ID and explicit voltage,
   current and power limits, then select **Add RD source and restart**. This
   appends a serial-bound TCP device using the configuration ETag. Saving
   reboots the ESP; restart the PSU if its original module does not reconnect.
   Discovery/registration never enables an output.

The same operations are generated for HTTP and USB CLI:

```sh
labbridge --endpoint serial:/dev/ttyACM0 call rd60xx.get_rd_provisioning
labbridge --endpoint serial:/dev/ttyACM0 call rd60xx.start_rd_provisioning --args-file examples/commands/rd-pair-start.json
labbridge --endpoint serial:/dev/ttyACM0 call rd60xx.advance_rd_provisioning --args '{"rd_provision_advance":{"session_id":"YOUR-SESSION","expected_phase":"server_address"}}'
labbridge --endpoint serial:/dev/ttyACM0 call rd60xx.advance_rd_provisioning --args '{"rd_provision_advance":{"session_id":"YOUR-SESSION","expected_phase":"wifi"}}'
labbridge --endpoint serial:/dev/ttyACM0 call rd60xx.cancel_rd_provisioning --args '{"rd_provision_cancel":{"session_id":"YOUR-SESSION"}}'
```

See [single-node configuration](../examples/configuration/rd6018-ap-sticks3.json).
Replace all synthetic identities and bench limits for the actual circuit.

Provisioning defaults to `profile: "pc_v22"`. For a controlled commissioning
comparison, trusted USB/CLI may explicitly select `pc_v22_target_ssid` in the
start request. That variant changes only the phase-one SSID field to the AP
SSID, with the resulting CRC/XOR/length changes; phase two, multicast and timing
are identical. W01 trials received the server IP but have not established
Wi-Fi/TCP pairing with this variant; retain the default `pc_v22` profile.
Status returns the selected `profile`. Confirm the displayed server IP and
advance within the 120-second management phase; the sender itself runs for
up to 60 seconds. A stale candidate listing does not prove a new pairing: check
for a new join result and TCP identity exchange. Start a new session for each comparison;
never change profiles midway through a phase. Keep the known working baseline
and re-pair the module with it after a failed comparison.


### RIDEN PC provisioning wire profile

The original-module setup uses **two ESPTouch length-coded transmissions**.
LabBridge's PC-profile sender implements the wire rules of RIDEN PC software
1.0.0.22. A W01 RD6018 accepted this profile from the ESP AP with WPA2 and an
associated phone. No initial TCP connection or IP address on the
module is needed: it observes Wi-Fi frame lengths before joining the network.

| Phase | SSID field `S` | Password field `P` | Transition |
| --- | --- | --- | --- |
| Server address | Literal ASCII `RD6006`, also for RD6018 | Server IPv4 as dotted ASCII, e.g. `192.168.4.1` | Operator sees that address / Connecting WiFi on the PSU and selects Next |
| Wi-Fi credentials | Actual target SSID | Actual target PSK | Module joins, sends a UDP result, then connects to the server |

In both phases the four-byte IP field contains the sender/server IPv4. The PC
chooses the last address returned for its hostname and lets the OS select the
outgoing interface. An independent sender should explicitly select the intended
IPv4 and interface rather than reproduce that adapter-selection weakness. The
PC UI rejects an empty SSID or PSK; open-network commissioning remains unproven.

For ASCII inputs, construct `N = 9 + len(P) + len(S)` logical bytes:

| Offset | Value |
| --- | --- |
| 0 | `N` |
| 1 | `len(P)` |
| 2 | CRC-8/MAXIM of `S` |
| 3 | Fixed **199 (`0xC7`)**, rather than the actual BSSID CRC |
| 4 | XOR of all the other logical bytes; XOR of the completed payload is zero |
| 5..8 | Sender/server IPv4 octets in dotted-address order |
| 9 onward | `P` followed by `S`, without terminators |

No six BSSID bytes are appended or interleaved. This differs from the current
general Espressif Android encoder. The PC uses Windows `Encoding.Default` but
string character lengths, so non-ASCII credentials are code-page dependent;
the reproducible profile here is limited to ASCII.

CRC-8/MAXIM uses initial value zero, reflected polynomial `0x8C` and no final
XOR. For each logical byte `v` at index `i`, compute `c = CRC8([v, i])` and send
three UDP datagrams with these **UDP payload lengths**, in order:

```text
40 + (c & 0xF0) + (v >> 4)
296 + i
40 + ((c & 0x0F) << 4) + (v & 0x0F)
```

The UDP payload contents are zero-filled and carry no data of their own.
Send logical indices `0..N-1` in order, including checksum index 4. Each burst
starts with **39 guide groups** of four lengths `515, 514, 513, 512`, followed
by the complete logical payload as triples. Thus a burst has `156 + 3*N`
datagrams. The PC has no explicit per-packet sleep; its loop sleeps **1 second
after each complete burst**, repeating for up to 60 seconds per phase. This is
a software scheduling rule, not a measured over-the-air packet spacing. Its
phase-two startup also arms the UDP receiver and waits 300 ms before starting
the sender; this is not a delay between data triples.

Destinations are **`234.n.n.n:7001`**, with `n` advancing once per guide group
and once per data triple. The counter starts at zero, increments before the
first packet, wraps `100 -> 1`, and is retained between bursts/phases until
the application's connection state is reset. Every packet of one group/triple
uses the same destination. The source UDP port is ephemeral. For example, with
initial counter zero the first guide group targets `234.1.1.1` and the first
data triple targets `234.40.40.40`.

For server `192.168.4.1`, phase one has 26 logical bytes, begins with header
`1A 0B 67 C7 E4 C0 A8 04 01`, and sends 234 datagrams per burst. Its first
data triple has lengths `41, 296, 226`. See the
[synthetic reference vectors](../examples/protocol/rd60xx-pc-provisioning-v22.json)
for both phases and the initial-counter convention.

The PC binds **UDP `0.0.0.0:18266`** for the phase-two result. It accepts an
11-byte datagram and reads bytes `7..10` as the module IPv4, deduplicating those
addresses until the requested device count is reached. The standard ESPTouch
layout is length/MAC/IP, but the PC does not validate the first seven bytes.
An implementation should validate the expected result and current session;
an ACK alone does not establish instrument identity or authorize an output.
The first phase instead relies on operator confirmation of the PSU display.

After joining, the module initiates the **TCP 8080** connection described above.
The PC listens on all interfaces and filters clients by matching the first three
IPv4 octets and a different final octet. That is an application-specific /24
assumption, not a protocol requirement. Keep LabBridge's AP-bound listener and
serial/model identity checks rather than copying that filter.

Primary references: [RIDEN software downloads](https://drive.google.com/drive/folders/1V0l6P1sIJilN1yBOsTO9YGLVdkuO0cX9),
[manufacturer manual, PC connection section](https://download.bastelgarage.ch/Produkte/User_Manual_RD60xx-x.pdf),
[independent original-module implementation](https://github.com/pgreenland/RidenRD60xxMQTT/blob/main/provisioning/README.md),
and [Espressif's general datum encoder](https://github.com/EspressifApp/EsptouchForAndroid/blob/master/esptouch/src/main/java/com/espressif/iot/esptouch/protocol/DatumCode.java).
The exact RIDEN constants and schedule above come from the PC binary; the
independent implementation corroborates the two-phase convention but uses a
different transmission schedule. Provenance and remaining RF questions are in
[uncertainties](uncertainties.md#riden-pc-software-protocol-analysis).

### Coexistence and a second ESP

AP and STA share one channel, with the STA channel taking priority. Espressif
classifies SoftAP with associated stations plus BLE as supported but potentially
unstable; this combination requires actual commissioning, not just compilation.
The initial configuration deliberately tries one ESP. Unexpected disconnects,
measurement gaps beyond configured freshness, ESP resets or unconfirmed STOP on
an otherwise intact connection fail acceptance.

If unstable, move the TCP source devices to a second ESP running the same
firmware. Use the [sources-only configuration](../examples/configuration/rd6018-ap-sources-sticks3.json)
with AP `LabBridge-Sources`, keep DL24M on the first node and use
[named nodes](../examples/nodes-rd-split.json) in host bindings. No PSU clients
then share the DL24M node's AP. Do not switch topology during an active run.
A lost radio link cannot prove physical OFF; independent circuit protection is
required wherever shutdown must remain possible during that loss.

Protocol/provisioning references: [original-module bridge and pairing documentation](https://github.com/pgreenland/RidenRD60xxMQTT),
[Espressif ESPTouch framing](https://github.com/EspressifApp/EsptouchForAndroid),
[ordinary/P model scaling](https://github.com/morgendagen/riden-dongle/blob/main/src/riden_modbus/riden_modbus.cpp),
[Espressif RF coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/coexist.html).
No third-party implementation code is vendored for this transport/provisioner.
The portable PC-profile codec/scheduler is separate from the general ESPTouch
reference codec. It keeps the multicast counter across bursts/phases, commits a
packet only after UDP submission, and waits only on transient queue backpressure
(up to one second) rather than inserting a fixed packet delay. STOP/cancellation
is checked between packets and during the quiet interval. An eleven-byte result
is counted only during phase two, for a matching peer/embedded AP-subnet address;
it stops credential transmission but does not advance management or enable output.

Unregistered TCP discovery peers are monitored with read-only identity probes:
one second after a valid reply, with a two-second reply deadline. A missed reply
or identity change closes that stream and releases its serial claim. A second
live stream with the same serial remains rejected. Configured devices use their
existing driver polling/recovery. This does not confirm an output state.

### Stable host USB names

Use `/dev/serial/by-id/` paths or locally configured udev aliases instead of
unstable `/dev/ttyACM0` numbering. The SDK resolves USB identity for native ESP
CDC/JTAG line control while opening the selected stable path.

The [example udev rules](../examples/host/99-labbridge-usb.rules.example) use
synthetic aliases and serial placeholders. Adapt and install rules locally;
never commit physical USB serials, installed host rules, node assignments or
configured network identities. Recheck the physical device before flashing.

```sh
labbridge --endpoint serial:/dev/serial/by-id/REPLACE_WITH_DEVICE_ID call core.get_node
```

## Public API and USB

The authoritative contract is [OpenAPI](../api/openapi.json), assembled from
core and module fragments. `tools/generate.sh` produces Python, TypeScript and
C models plus firmware routes and validation constraints. Changes belong in the
fragments, not generated files. Modules declare transports and capabilities.

The specification includes operation summaries, prerequisites, confirmation and
failure behavior, parameter descriptions, units, continuity rules and examples.
Import the assembled JSON into an OpenAPI viewer to browse this reference.
Generated Python docstrings/model fields and TypeScript API/model comments carry
the same descriptions. Referenced object fields use their shared schema's
documentation; device-specific uncertainties are stated where they affect use.

All instrument routes live below `/api/v1`. `GET /devices` lists local IDs and
capabilities; `GET /devices/{id}/measurements` returns quality, sequence and age.
An unavailable channel is absent or explicitly unavailable, never a plausible
zero. Time fields are monotonic milliseconds since the reported `boot_id`.
Never compare monotonic timestamps across nodes or reboot boundaries.

Reserve devices with `POST /leases`, heartbeat at least every second (TTL up to
10 seconds), and send `X-Lab-Lease` with writes. Use an `Idempotency-Key` per
logical mutation. A 202 response contains an operation ID and Location; poll to
a terminal state. Reusing the key with different content is a conflict. Keys
and operation IDs are scoped to one boot. Terminal operations are retained for
five minutes in a 128-entry bounded cache; priority STOP remains available when
that cache is full. Do not replay
an uncertain write after reboot. `POST /devices/{id}/stop` needs no lease.
Errors use `application/problem+json`, including final output and restoration
state when known. A failed operation can still have changed the instrument.

HTTP accepts a node bearer token for scripts or a password session for the UI.
Both grant the same laboratory access; leases, idempotency and output safety
checks still apply. The shared device password has no username or default.

Choose the first password through the setup AP at `http://192.168.4.1` or trusted
USB. LAN access cannot claim an unconfigured password. An existing node retains
its API token and receives no default UI password during upgrade. Set/recover
its UI password with `labbridge --endpoint serial:/dev/ttyACM0 ui-password`;
the CLI asks twice with hidden input and never emits the password. UI password
changes require the current session, old password and matching new entries;
USB recovery bypasses the old password. Passwords contain 12..128 Unicode code
points, with spaces preserved. Only a salted PBKDF2-HMAC-SHA256 verifier
(6,000 iterations, version-two parameters) is persisted separately from the API
token and Wi-Fi credentials.

The UI has password-manager-compatible forms on `/wifi` and `/devices`. LAN
Wi-Fi discovery prompts for login and resumes automatically after success.
Actuator mutations are never automatically repeated after authentication loss.
At the setup AP, Wi-Fi status, scan and station SSID/PSK configuration remain
public, including when the AP itself uses WPA2. AP security changes, RD
provisioning and laboratory control still require authentication. Unknown AP
secrets and saved station PSKs are never returned.

Browser sessions use opaque `HttpOnly; SameSite=Strict` cookies, remain usable
across page navigation/reload for an absolute twelve hours, and are limited to
eight concurrent sessions. They are boot-local: a reboot or password replacement
invalidates all of them; logout ends only the current session. IP and mDNS
hostname visits use separate browser cookies. The UI first attempts STOP/release
of its current lease on logout. A failed request does not prove OFF; existing
lease expiry and on-node safety checks remain active. Password processing is
limited to one low-priority worker and runs outside the node mutex and HTTP
handler. Five failed password checks in a rolling minute cause `429` with
`Retry-After`; a busy worker or full session table also returns `429`.
The operator-approved policy prioritizes a login below three seconds on the
ESP over maximum offline password resistance. The 6,000-round work factor is
deliberately below server password-storage recommendations; random salts,
12–128-character passwords and online throttling still provide basic protection.
Experimental version-one/600,000-round records require `ui-password` recovery
over trusted USB. Firmware rejects them promptly and never treats their hashes
as lower-cost verifiers. Recovery preserves the API bearer token and other
settings; it does not open LAN password setup.

`/api/v1/auth/status`, `/setup`, `/login`, `/session` and `/password` describe
this lifecycle in OpenAPI. Cookie-authenticated mutations require the session's
`X-Lab-CSRF`, a matching Origin and JSON content when a body is required. Login
and initial setup also require same-origin JSON. The server accepts its local
interface IP or configured `<node_id>.local` as UI hosts. Bearer clients need no
cookie or CSRF header. `/management/token` remains strictly USB-only.

The current server uses **unencrypted HTTP**, including passwords and session
cookies, and is intended for a trusted network. HTTPS and optional client
certificate authentication for both browsers and API clients are tracked in
LB-021. A valid client certificate will be an alternative login, alongside
password and bearer authentication; LB-020 adds no certificate requirement.

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
a source and load with confirmed OFF before transitions. A feedback interlock
is optional; when configured its feedback and break-before-make checks apply.
Internal temperatures are useful device-health measurements only.

Charge uses source CC/CV and termination current plus a timeout; discharge uses
load CC and cutoff plus a timeout. Open both paths during rest. Current pulses
must respect node/profile limits. Record CSV samples and JSON run metadata,
including configuration, device identities, boot IDs and errors. Integrate
capacity/energy by trapezoids only across consecutive fresh samples with
acceptable gaps. Never integrate stale periods or infer zero current during
missing data. Raw instrument counters remain supplementary.


### Host SDK

```python
import uuid
from labbridge.sdk import Node, Cluster
from labbridge_generated.models.current_request import CurrentRequest
from labbridge_generated.models.output_request import OutputRequest

with Node("serial:/dev/ttyACM0") as node:
    with Cluster({"bench": node}).lease({"bench": ["load"]}) as session:
        args = dict(device_id="load", x_lab_lease=session.token("bench"))
        node.wait(node.components.set_component_current(
            **args, component_id="sink", idempotency_key=uuid.uuid4().hex,
            current_request=CurrentRequest(current_a=0.1)))
        # Optional bounded enable, after commissioning the connected circuit:
        # node.wait(node.components.set_component_output(
        #     **args, component_id="sink", idempotency_key=uuid.uuid4().hex,
        #     output_request=OutputRequest(enabled=True, max_duration_ms=1000)))
        node.stop("load")
```

The session heartbeats and attempts independent STOP on exit. Each actual device
must be ready, fresh and within configured limits. A failed cleanup is an error,
not a confirmed safe output. `node.apis` discovers generated modules for generic
CLI calls; convenience attributes expose the current instrument APIs.

### Persistent and local safeguards

Before ON, persist that the instrument may be on; clear this only after confirmed
OFF. Reconnect/reboot does not resume ON. Incomplete multi-step setting changes
retain their original register snapshot and menu-exit obligation in NVS; stop,
leave the menu and restore before accepting new work. Configuration changes are
blocked while such an obligation could abandon a live output.

A lease-group fault stops all devices in that lease. Freshness cannot be extended
by transaction recovery. Optional local cell guards bind a fresh external channel
to each controlled device; missing/stale/out-of-range cell temperature stops the
lease group. `OutputRequest.max_duration_ms` provides a per-activation local cap
(100 ms to 24 h), bounded by any configured static cap. Time starts when enable
is accepted; it is an upper bound, not a precision pulse trigger.

### Battery wiring and workflow

Keep the load's Kelvin voltage-sense connection on the cell independently of
switched current paths, so rest measurements remain meaningful. Commission this
wiring against a meter before running. A source path must use a verified battery
connection and reverse-current protection suitable for the source; an isolated
or powered-down ordinary PSU output is not automatically safe to backfeed.
When a switch is installed, its feedback must represent the actual current path,
not just an output pin. Without one, confirmed instrument OFF is the transition
mechanism; it does not imply galvanic isolation or reverse-current blocking.

Every controlled node needs a local temperature guard and cell-sized voltage,
current and power limits. Host profile ceilings are separate from requested
charge voltage/current to allow measurement headroom. A fresh internal heatsink
reading never satisfies the cell-probe requirement. The DS18B20 adapter uses the
Espressif [sensor driver](https://components.espressif.com/components/espressif/ds18b20/versions/0.4.0/readme?language=en)
and [1-Wire bus driver](https://components.espressif.com/components/espressif/onewire_bus/versions/1.1.1/readme?language=en),
with nonblocking node access to a conversion worker and explicit probe identity.

The Python runner performs preflight, snapshots original source/load settings,
reserves nodes, charges in CC/CV to termination, rests, pulses and discharges to
cutoff for each cycle. With a feedback switch it keeps the source path isolated until the source is
configured and enabled. Direct routing starts with both instrument outputs OFF. Voltage changes first reduce the source current setting
to zero, so intermediate setpoints stay within the local power limit. STOP requests to source, load and interlock are attempted
independently. Active phases supply local duration caps. Host/lease failures do
not start another phase. There is no distributed atomic transaction across nodes.

`run.json` is checkpointed before actuation and includes original settings,
identities, configuration, boot IDs and final restoration status. A process crash
can leave restoration pending; use the saved snapshot after confirming OFF.
Lease loss can prevent automatic settings restoration and is reported explicitly.
CSV integration excludes stale samples, repeated sequences, long gaps and stage
boundaries; a node reboot aborts the run. The simulator is deterministic workflow
coverage, not a cell model or proof of relay/charger behavior.

## Firmware images and PC updater

The manufacturer pages for [DL24M](http://en.atorch.cn/ProDetail.aspx?ProID=17)
and related [modular products](http://en.atorch.cn/ProDetail.aspx?ProID=12) /
[loads](http://en.atorch.cn/ProDetail.aspx?ProID=16) led to 2.1 and 2.2 images.
These contain a Cortex-M Thumb application starting at flash address `0x1000`.
Their numeric parser belongs to the same 9-byte request / 10-byte reply family.
The 2.1 and 2.2 binaries differ; matching parser structure does not prove complete
hardware compatibility. A genuine Ver 2.0 image has not been obtained.

The PC v2.0.8 updater uses a separate `6D AC … A6 DA` framed protocol, sequence
and complement, length, command, 32-bit address, payload and reflected CRC with
polynomial `0x8408`. Upgrade entry command `26` writes the flag `0x67890123` at
flash `0x0F00` and requests a system reset. The application then hands over to a
separate bootloader. The PC sequence initializes (`26/11`, `20/11`), erases
(`24/12`), transfers blocks up to 512 bytes (`22/12`), checks CRC (`25/12`) and
finishes (`21/11`), with roughly 1–5 s stage timeouts and a final device CRC check.
The bootloader is absent from the application image, so erase/recovery behavior
cannot be established from that image alone. No manufacturer update was flashed
on the tested load. This updater is not an automation transport.

### Configuration examples

`examples/configuration/` includes a StickS3 load, two independent USB sources,
and a single-node battery layout on a bare classic ESP32. All identities are
synthetic placeholders. The battery example's GPIO numbers are an illustrative
bare-board allocation, not StickS3 wiring: approve them in the board build only
after checking the exact hardware. UART requires a verified compatible electrical
interface; do not connect RS-232 levels or assume an unknown PSU connector pinout.
For distributed battery nodes, configure a local fresh cell channel on every
controlled node; a remote host reading alone cannot enforce a local safeguard.

Read the current configuration before updating. Replace the template revision
and board with the actual values; use its current ETag as `If-Match`:

```python
import json
from labbridge_generated.models.node_configuration import NodeConfiguration
current = node.core.get_configuration()
requested = NodeConfiguration(**json.load(open("configured-node.json")))
requested.revision = current.revision
node.core.set_configuration(f'"{current.revision}"', requested)
# A successful update reboots the node. Reopen USB and rediscover its boot ID.
```

The IDs in workflow bindings must match configured IDs. The single-node binding
example uses `battery-node` for all roles; `examples/battery/bindings.json` shows
three nodes. Lease identities remain node-scoped in both layouts.

### Reproducing a bounded DL24M bench check

With the host package installed, `python tools/hil_dl24m.py` performs a read-only
audit. For an explicitly prepared 24 V / 1 A-limited bench source, confirmed
CC/OFF and bridge limits 25 V / 0.2 A / 5 W:

```sh
python tools/hil_dl24m.py --bench-profile examples/bench/dl24m-24v.json \
  --output results/bench-pulse.json
```

The profile enables a bounded 0.100 A pulse, records readback and restores the
original current/OVP with output OFF. Local duration and lease limits still
apply. A failed cleanup is reported and the original snapshot stays in the
result. This bench procedure does not commission a battery circuit.

### Legacy telemetry framing

The 36-byte `FF 55` reports have a checksum in byte 35 (zero-based): the sum of
bytes 2–34 modulo 256, XOR `0x44`. Types 01/02 occur on the tested device.
Their voltage/current fields are coarser than numeric replies; capacity/energy
scales remain uncalibrated. Use the audited numeric registers and host V/I
integration for quantitative automation. Numeric replies may be interleaved
with these notifications and must be parsed as an independent frame stream.


## Device components: the common integration API

A physical device owns its transport identity, leases and recovery. It exposes
any number of components: `source`, `sink` or `sensor`. A source/load combination,
a multi-channel source, and a monitor with several electrical/temperature channels
therefore use the same public interfaces. `Device.components` and
`GET /devices/{id}/components` describe component IDs, kind, supported actions,
and sensor quantity/unit. Do not infer purpose from the model name.

| Route below `/api/v1/devices/{deviceId}` | Purpose |
|---|---|
| `GET /components` | Component discovery |
| `GET /readings` | One sensor snapshot with per-channel freshness |
| `GET /components/{componentId}/measurement` | Scalar reading in its declared unit |
| `GET /components/{componentId}/state` | Actor state and typed settings |
| `PUT /components/{componentId}/current` | Source current limit or sink CC target |
| `PUT /components/{componentId}/voltage` | Source voltage target |
| `PUT /components/{componentId}/cutoff` | Sink cutoff, when advertised |
| `PUT /components/{componentId}/duration` | Instrument timer, when advertised |
| `PUT /components/{componentId}/output` | Bounded actor enable/disable |
| `POST /components/{componentId}/stop` | Priority stop, no lease required |

Writes keep the same lease, idempotency, operation polling and readback rules.
`Operation.component_id` identifies the target when a component route was used.
Component operations report `requested_value/unit` and, on success,
`confirmed_value/unit` in the common interface units. Compatibility operations
retain their earlier raw confirmation behavior.
Leases and in-flight protocol operations are currently serialized per physical
device. Component STOP defaults to whole-device STOP; multi-channel drivers may
implement a confirmed narrower stop. Lease expiry/device STOP always stops all
actors on that device. Never assume sibling channels remain enabled after STOP
unless the provider documents that guarantee.

Use the generated `ComponentsApi`, `node.components`, or the CLI's `call
components.<method>`. These APIs do not require a model-specific command. The
old DL24M/RD60xx routes remain compatibility/specialized interfaces. Supported
precision is enforced by the driver; targets are never silently rounded.

Sensor values preserve sign and state their quantity/unit (`V`, `A`, `W`, `Wh`,
`Ah`, `degC`, or dimensionless `1`). A valid value must also have acceptable age;
an unavailable value is never a fabricated zero. Pair sequence and `boot_id` with
an optional sensor `epoch`: an upstream monitor can restart while its ESP stays
running. Do not integrate across either boundary. Internal temperatures remain
explicitly distinct from external/cell probes. Local cell guards accept both
component IDs and the earlier native temperature channel IDs.

## Optional meters and direct-output battery routing

Omit `interlock` from workflow bindings to switch direction using confirmed
source/load OFF. Keep it to use the existing feedback switch module. OFF and
settings cleanup still apply independently to each physical device. A direct
layout must be commissioned for the actual instruments' off-state behavior and
reverse current; the workflow cannot turn an unsuitable output circuit into an
isolating switch. Maintain continuous cell-voltage sensing during rest.

Actors are bound with `{node, device, component}`. Existing bindings without
`component` use `source` and `sink` defaults. Optional `meters` bindings select a
measurement provider for `charge`, `discharge` and `rest`. Each selects voltage,
current and power component IDs (defaults `voltage/current/power`) and explicit
`current_sign` / `power_sign` of +1 or -1. Verify orientation with known current;
no absolute-value conversion hides a reversed connection. INA228 power is
unsigned, so its power sign stays +1 even when current changes direction.

CSV retains raw and normalized I/P plus measurement and actor identities.
Integration uses selected current and measured power, rather than recomputing
power when an independent meter is selected. Actor telemetry and local safeguards
remain active. Stale/missing/mixed-sequence data, a changed sensor epoch or reboot
aborts the run. Additional `sensors` bindings select any sensor component and a
label, with optional `minimum`/`maximum`; they are recorded in `sensors.csv`.
Additional internal temperatures are useful for diagnostics and do not replace
the mandatory external cell probe.

The direct INA228 and Yocto binding examples are under `examples/battery/`.
Both are optional: omitting meters uses instrument telemetry, and omitting extra
sensors leaves only the cell-temperature channel. Both chemistries support either
routing arrangement. These remain commissioning templates.

## INA228

The ESP driver uses I²C0 with an explicitly approved SDA/SCL pair and decimal
address 64–79. Multiple addresses may share that same pair; other pin conflicts
and duplicate addresses are rejected. External pull-ups and suitable voltage
levels are part of the wiring. `ina228` configuration requires `shunt_ohms`,
`max_current_a` and `adc_range`; full-scale and calibration representability are
validated before allocation.

The monitor exposes bus V, signed A, unsigned W, accumulated Wh, signed Ah and
internal die temperature. Calibration is quantized to the actual programmed
register. Continuous conversion with averaging is used; conversion-ready,
identity/configuration checks, saturation and overflow prevent false fresh
values. Counters are relative to the first validated sample of the sensor epoch, so
prior calibration cannot corrupt their scale; hardware counters are not reset.
Counter overflow suppresses the affected counter channel. Reinitializing
the monitor advances its sensor epoch. Register conversions follow the
[TI INA228 datasheet](https://www.ti.com/lit/ds/symlink/ina228.pdf).
Hardware/shunt calibration and absolute accuracy have not been commissioned.

## Yocto-Watt-V2-C

For native USB on the automation PC, install `host[yocto]` and configure a named
node endpoint `yocto+usb://YWATMK2C-<serial>`. The SDK provider exposes a read-only
device `meter` through the same generated discovery/sensor API. The official
Yoctopuce library handles USB and user calibration. Multiple serials can share
its USB session. Ensure the OS grants USB access to the application; only one
native USB owner may hold a device at a time.

For ESP access, the `yocto_watt` HTTP provider reads a serial-bound module from
VirtualHub/YoctoHub; use a reachable plain HTTP hub URL without credentials/path.
It validates identity/status/units and decodes fixed-point values. Custom user
calibration requires the native SDK provider. This ESP backend does not implement
the vendor's native USB protocol; the USB serial-adapter driver is unrelated.

Channels are DC V/A, active W, optional AC RMS V/A and power factor. Current is
normalized from mA to A while preserving direction. Disabled/unavailable channels
remain unavailable. The implementation follows the manufacturer's
[user guide](https://www.yoctopuce.com/EN/products/yocto-watt-v2-c/doc/YWATMK2C.usermanual.html).
No physical Yocto-Watt has yet been exercised in this project.

## DL24M Bench page

Open `/dl24` on the node (also linked from `/devices`). The complete DL24 Bench
interface is adapted from the GPL-3.0-or-later upstream page. Its unchanged source,
licence and SHA-256 are recorded in `web/vendor/dl24-bench`; the device serves the
licence and corresponding frontend source at `/dl24-license.txt` and
`/dl24-source.tar.gz`. All runtime assets are local and use the existing CSP.

Sign in with the shared device password, then choose one configured DL24M.
The same cookie session works across `/wifi`, `/devices` and `/dl24`, including
reloads. Login resumes read-only discovery and never reserves or repeats a
device command. Cookie mutations, heartbeats and independent STOP carry the
session CSRF header. Reading does not reserve devices. **Reserve device**
acquires only the selected physical device, with a 10 s lease and 1 s heartbeat. Setpoints and output
changes wait for confirmed operations. **STOP** remains available independently
of a lease. Device switching and release first request STOP and confirm OFF;
a failed STOP prevents the switch. Closing the page attempts STOP, but delivery
is not guaranteed: the existing node-side lease expiry remains the fallback.
Loss of the connection, reservation or boot invalidates control; reconnect never
replays an ON command. Session expiry cancels queued commands and requires a
new explicit reservation after login. Sign out first attempts confirmed STOP
and release, then ends the session even if shutdown could not be confirmed;
the UI reports that uncertainty and relies on lease expiry as the fallback.
Settings and device-counter reset require confirmed OFF.

CC current, cutoff, minute-resolution timer, display settings and supported
protection limits use the existing typed API. Device language is explicitly the
backend's raw 0/1 enum, and standby time is a raw value with unconfirmed units.
Active CV/CR/CP, calibration, no-load zero, factory reset, HID frames/snapshots
and fan RPM remain visible with unavailable controls/values. No HID commands are
sent by this page. These retained features do not imply commissioned support.

The default HTTP refresh is 1 s (selectable 500 ms/1 s/2 s); this reads cached
backend samples and does not alter BLE polling. API requests share one in-flight connection so
monitoring, heartbeat and command confirmation fit the ESP socket budget. STOP
has priority and cancels queued commands; transport loss cancels queued requests
without replay. Password requests use a separate connection so password work
does not occupy the instrument queue or hold up STOP and heartbeats. Settings
refresh every 5 s, subject to the driver's slower settings cache. Live records continuously; Load
records only during confirmed ON, while state monitoring and heartbeat continue.
The graph retains 900 points. At 100,000 CSV rows, recording pauses visibly until
export/clear; device control and monitoring continue. Recording lives only in
the browser session. Export before switching devices or closing/reloading.

Capacity (mAh), energy (Wh) and observed time are browser-session values, not native
DL24M counters or certified battery-test results. Integration uses trapezoids
between distinct, fresh, finite samples with increasing device-monotonic time,
within the configured freshness limit and one device/boot. Missing/stale samples
and larger gaps break the trace/integration. CSV retains sample identity, boot,
sequence, timestamps, units and quality. Missing measurements appear as unknown,
never measured zero. Internal temperature is not a cell probe. **Clear** affects
only the browser recording; **Zero all data** explicitly resets device counters.

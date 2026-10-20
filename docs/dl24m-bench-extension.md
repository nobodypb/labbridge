# DL24M Bench capabilities and stream (LB-021)

The retained Bench controls use `Device.features` to distinguish supported,
unverified and unavailable functions. An unavailable value is never replaced by
zero or by a different temperature channel. This page records the protocol facts
and commissioning prerequisites behind those statuses; real results belong in
[acceptance.md](acceptance.md).

| Area | Native protocol, units | Preconditions and evidence boundary |
|---|---|---|
| CC | write 02 / read 17, bank 0, 0.001 A | Existing commissioned CC path, configured current/power limits. Common component routes stay compatible. |
| CV | write 06 / read 21, bank 1; target 0.01 V | Confirmed OFF for mode/target changes. ON requires fresh OCP and OPP no greater than local limits, valid voltage target and fresh V/I. A voltage target alone cannot bound sink current. |
| CR | bank 2, target 0.1 Ohm | OFF/readback; ON checks current and power at the configured maximum input voltage. Positive resistance, conservative maximum 240 Ohm. |
| CP | bank 3, target 0.1 W | OFF/readback; ON checks power and worst-case current at the fresh nonzero device cutoff. Present voltage must exceed that cutoff. |
| Elapsed time | read 13, packed hours/minutes/seconds | Invalid minutes/seconds rejected; individual receive time, age, unit s and source are exposed. Hour byte is bounded by the wire format. |
| Capacity / energy | reads 14 / 15 | Native integer mAh/mWh are converted to Ah/Wh. Individual source, age and quality remain available; fresh counters also use the common charge/energy sensor routes. A bounded 90 s CC run verified relative scaling; absolute accuracy remains unverified. Browser integrals are separate. |
| Internal temperature | read 16, 0.1 degC | Existing internal channel, never cell temperature. |
| External probe / fan RPM | no audited scalar mapping | Kept visible with a concrete restriction; no invented numeric query, HID field or substitution. |
| Settings / counter reset | existing audited routes | OFF and lease; journal original reversible settings before writes. Reset 05 destroys counters and requires explicit UI confirmation. Standby time remains raw. |
| BLE diagnostics | actual GATT submissions and received notifications | At most 32 observations, 64 bytes each; overflow/truncation reported, connection generation and receive time retained. TX submission does not establish ATT or instrument success. The existing parser remains authoritative. No extra device reads. |
| Register snapshots / compare / export | cached 22 audited registers | Read-only, no lease. Comparison and Clear affect browser snapshots only. Exports carry boot/device identity. No HID frames are fabricated. |
| Calibration | calibration-related 46 / 4F | Unverified effects and missing complete backup/restore and reference measurement. No automatic calibration or absolute-accuracy claim. No writes exposed. |
| No-load zero | incomplete effect/backup mapping | Retained and locked until calibration effects and reversible recovery are established. |
| Factory reset | command 4B | Known reset family, but full calibration backup/restoration is unproven. Retained and locked; excluded from automatic bench testing. |

The mode mapping follows the native firmware control branches, rather than the
HID application. Offline V2.2 May analysis shows mode 1 using the voltage
regulator (target global 0x20000118), mode 2 computing voltage/resistance (target
0x2000011c), and mode 3 computing power/voltage (target 0x20000120). Command 02
stores native scaled targets at 0xf3e8–0xf484; read 17 uses matching scales near
0xf6a8–0xf6d0. Mode/target operations take a fresh OFF/mode/target snapshot,
persist the original mode and target before mutation, verify readback, and STOP
and restore on failure/restart. Success clears the rollback journal. Existing
inactive-bank access still returns to CC and retains its conservative raw ranges.
Manufacturer binaries and private captures are not distributed here.

## WebSocket contract

Connect `/api/v1/ws` and send `StreamAuth` first, within three seconds. Failed
or late authentication closes the connection without device data. The bearer
secret is never put in a URL. The board profile allows one UI stream session;
unauthenticated sessions occupy the same bounded slot until rejected/timed out.
The transport revalidates the token and prohibits USB-only token retrieval.

`StreamRpc` is the existing USB envelope `{id,method,path,headers,body}`. Replies
remain `{id,status,headers,body}`. Normal dispatch handles the same schemas,
leases, idempotency and asynchronous operations as HTTP/USB. The session owns
Authorization; only lease, idempotency and configuration headers are forwarded.
No arbitrary remote paths or raw device commands are accepted. Input JSON
nesting is limited to 16 before parsing to protect the task stack.

Subscribe to one configured device using `StreamSubscription`, at 500/1000/2000
ms (default 1000). Initial snapshots carry node/boot identity and the device
catalogue. Compact runtime snapshots repeat selected measurements, actor states,
driver settings and up to 16 outstanding
operations initiated through this connection. Operations are pushed while
pending and at completion. The catalogue is refreshed every five seconds; state
checks run at 100 ms without rebuilding unused measurement or catalogue trees.
Terminal IDs leave the stream send list when their confirmation frame is taken for transmission; the node's normal HTTP/USB
operation lookup and idempotency retention remain unchanged. Measurement
sequences/receive times remain independent
of stream sequence. A replaced pending snapshot sets `gap`; the browser breaks
curves and integration. Full snapshots permit explicit resubscription after loss.

RPC replies precede telemetry; pending inbound RPC suppresses a telemetry send.
Periodic heartbeats preserve a queued measurement rather than creating recording
gaps. On outstanding-operation
overload the accepted operation ID is replied before closing the connection.
Snapshots are serialized before entering the telemetry slot; HTTPD sends the
encoded bytes without building another JSON tree. RPC handling suspends snapshot
construction and can discard pending telemetry before allocating a larger reply. There
is one pending telemetry slot and at most one queued/in-flight HTTPD send.
A blocked send suspends further JSON construction and marks missed intervals;
the next recovered frame reports the gap. Replacing a slot releases its old
encoded payload first. Status comparison uses a fixed-size fingerprint instead
of retaining another serialized snapshot. Limits are 16 KiB
inbound and 32 KiB outbound messages, plus the fixed TCP stack buffers. Oversized
messages, failed sends, a full server work queue or a send timeout close the
connection. A send override completes partial socket writes and bounds each
header/payload write to one second; incomplete frames close the connection.
Network I/O runs on HTTPD outside the node safety mutex; snapshots
read cached data and perform bounded encoding under that mutex, preventing
telemetry allocations from competing with journal persistence. Snapshot assembly
does not call request dispatch or drive device ticks. No network wait runs in the
local tick task.

Only explicit lease heartbeat RPC renews ownership. Ping/pong, snapshots and an
open socket do not. After loss the browser rejects all pending requests, locks
control and requires an explicit reconnect. It never replays writes. HTTP remains
available for existing clients and recovery. Page closure is best-effort STOP;
lease expiry and local guards remain the independent shutdown mechanism.

The Bench generated clients use WebSocket RPC for discovery, commands,
heartbeats, and explicit state/diagnostic requests. Normal monitoring and
operation completion use pushed snapshots, with no HTTP polling. Load view
pauses recording while OFF, while readings, state checks and heartbeats continue.
Native values and their quality/source are also retained in CSV separately from
browser-session charge/energy.
The browser anchors the device clock conservatively to the subscription request
that caused its first sample. Later arrival does not renew freshness: queued
transport time also ages scalar, temperature and native readings. Expired samples
lock control, break curves and do not enter session integrals. Device changes and
reconnects establish a new clock bound; changing push interval retains it.

## Memory and response integrity

Request validation loads only the selected operation's generated rules and
reachable schema references, and frees them before returning. It preserves the
same API constraints without a permanent all-request schema tree. A failing
retention test characterized the former cache before this change. Device
discovery, one stream snapshot and bounded capture replies have explicit
cJSON memory ceilings (32/48/64 KiB respectively). The DL24M component and actor
views preserve the common API shape while avoiding repeated construction of the
full capability description. JSON ownership is moved at serialization boundaries.
An exact-size preallocated printer writes directly into the returned string,
including matching cJSON escaping/number formatting, without a second expanding
print buffer. Canonical request signatures use the same checked ownership path,
so allocation failure cannot silently drop a field. Stream allocation failure
closes the connection at the task boundary and cannot silently remove a field or
replace a recorded original setting with an empty value. An authenticated inbound RPC may preempt a pending
telemetry snapshot; the next event reports the gap. Diagnostic UART builds report
the initial free heap and largest block. These checks supplement physical stream
acceptance; a desktop allocation test is not a measurement of ESP free RAM.

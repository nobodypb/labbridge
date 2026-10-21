# DL24M Bench capabilities and stream (LB-023)

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
| Internal temperature magnitude | reads 16 / 36 of the same sensor state, 0.1 / 0.01 degC encoding | Both numeric replies omit the sign. Prefer fresh 36 in the instrument tile; retain 16 for compatibility. Encoding resolution is not accuracy. Neither is an external/cell-temperature channel. |
| External probe / fan RPM | no audited scalar mapping | Kept visible with a concrete restriction; no invented numeric query, HID field or substitution. |
| Settings / counter reset | existing audited routes | OFF and lease; journal original reversible settings before writes. Reset 05 destroys counters and requires explicit UI confirmation. Standby time remains raw. |
| BLE diagnostics | actual GATT submissions and received notifications | At most 32 observations, 64 bytes each; overflow/truncation reported, connection generation and receive time retained. TX submission does not establish ATT or instrument success. The existing parser remains authoritative. No extra device reads. |
| Register snapshots / compare / export | cached 22 audited registers | Read-only, no lease. Comparison and Clear affect browser snapshots only. Exports carry boot/device identity. No HID frames are fabricated. |
| Calibration gains / backup | no native gain read/write/backup primitive | The original HID gain controls cannot be mapped on the audited native revision. 46 overwrites live temperature; it is not a persistent gain. Retained and unavailable. |
| No-load zero | 4F samples current and rewrites flash calibration | No independent coefficient/offset readback or backup/restore primitive. The required checked, reversible operation cannot be implemented on this revision; retained and unavailable. |
| Factory reset | 4B rewrites settings and calibration | Broader than the original HID calibration-preserving reset. No coefficient backup/restore primitive; do not substitute destructive native reset semantics. Retained and unavailable. |

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

## Native protocol boundary

The audited V2.2 May native command switch has 22 non-mutating reads:
10–19, 21 and 32–3C. Read 22 consumes/clears an event and 31 enters a menu;
neither is passive diagnostics. Other cases through 40 use the default branch,
not additional sensor or calibration registers. The native FF55 status packet
contains voltage, current, counters, energy, one instrument temperature and
runtime. Its reserved bytes do not carry a fan tachometer, external probe or
calibration coefficients. This is a limitation of this audited revision, not a
claim about every device sold under a similar name.

Reads 16 and 36 reference the same live temperature field (RAM 0x20000064),
multiplied by 10 and 100 respectively. The sensor computation at 0x10a28 updates
that field. The original parser, executed offline with synthetic positive and
negative values, produces magnitudes: 25.12 becomes 251/2512; -5.2 becomes
52/520. Command 46 overwrites this same live field. A parser-only 46/36
round-trip therefore does **not** establish a persistent calibration setting and
must not be offered as one. No 46 write was used for physical acceptance.

The 4F branch samples the current ADC and copies the result into its offset,
then erases/rewrites calibration flash. The 4B branch initializes defaults and
also writes calibration words. Neither command has a matching coefficient
backup/restore query in the native switch or status sender. Readback of an
already-zero current cannot prove a zero-offset change. Consequently the original
HID gain/backup controls, checked reversible zero and calibration-preserving
factory reset have no faithful native implementation under this story's recovery
contract. Their UI remains visible with these concrete reasons, and no raw write
escape hatch is provided. Physical destructive experiments or a future different
firmware/protocol could change that boundary; they are not an unperformed
ordinary integration task.

These are sanitized findings from command dispatch, sensor calculation, flash
write paths and the status sender, checked against offline parser execution.
Manufacturer binaries, decompiled bodies and private captures are not shipped.
An independent reference is still required for any absolute measurement-accuracy
claim; display precision alone supplies no such evidence.

## WebSocket contract

Connect `/api/v1/ws` and send `StreamAuth` first, within three seconds.
API clients supply `token`; the Bench supplies `csrf_token` from its shared
password session. Exactly one is required. Browser access is bound to the upgrade
request's HttpOnly cookie, trusted server origin and matching Origin. Session
expiry/logout/password replacement ends stream access; client-supplied Cookie,
Origin or Authorization RPC headers cannot replace that binding. Password and
session-management operations stay on their dedicated HTTP worker so hashing
cannot hold the instrument queue, STOP or explicit heartbeats. Failed
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

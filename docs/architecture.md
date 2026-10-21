# Architecture

```
external workflows → SDK / automation CLI
                     ↓ generated typed clients
HTTP or USB → generated routes / validation → node operation registry
                                              ↓ capability dispatch
                     drivers: DL24M, RD60xx, interlock, temperature
                                              ↓ instance-owned endpoints
                              BLE / USB serial / UART / GPIO / 1-Wire
```

`firmware/core` is portable C++17: identity/resource validation, node routing,
leases, operation identity, guards and transactions. It does not know any
particular instrument ID or ESP-IDF. A `Driver` owns protocol behavior and an
`Endpoint` owns one byte stream and connection generation. Driver factories
register by driver name. Node IDs are arbitrary validated configuration strings.
The API manifest is generated from module fragments; add module operations
without inserting fixed device-name branches in the core.

ESP-IDF owns persistence, Wi-Fi, console, BLE/USB workers and GPIO timing.
Transport send operations must be nonblocking to the safety loop; an ATT worker
waits for delivery acknowledgement. Generation counters invalidate in-flight
transactions. Each driver has independent parser, register cache and recovery
state. Stop preempts queued foreground work. An output obligation survives
restart in NVS; a recovered connection performs OFF, never automatic ON.

Configuration validates all resource claims before mutation. Board profiles
explicitly budget transport instances and available pins. Multiple devices of
the same driver have separate endpoints. A node may host a single instrument
or any supported resource combination. More nodes are the answer when a board
cannot accommodate a combination; the host SDK coordinates them.

OpenAPI is assembled from `api/core.json` and `api/modules/*.json`. Generated
models are committed for review and must regenerate without drift. Python and
TypeScript integrations use generated clients. The firmware uses generated
routes/constraints and C model build checks. Handwritten behavior should stay
small and testable around generated types.

Workflow state belongs on the host, safeguards on the node. A reference switch
provider implements the `isolated/charge/discharge` capability; a future Wi-Fi
relay provider must offer the same confirmation and fail-open semantics. It
must preserve feedback semantics whenever an interlock is selected. Direct
source/load output switching is an explicit optional workflow arrangement.

Startup configuration failure inhibits leases and instrument mutations while
keeping read/management and priority STOP available. Already-created drivers
continue ticking their shutdown/restore obligations. Guard dependencies and
ordered temperature limits are validated before any factory allocation; a
partially initialized module set must never become an unguarded control surface.


Physical drivers expose component descriptors and virtual sensor/actor adapters.
Standard routes translate component actions into the same checked driver
transactions as compatibility routes; the core never branches on a model name.
Multi-channel drivers can override component command/state/stop behavior.
Read-only meters publish immutable snapshots from worker tasks; node routing and
safety do not wait on I²C or network I/O. I²C endpoints share only their validated
bus/pin pair, not their address or sample state. A host-native Yocto provider
implements the same generated read-only API over the official USB SDK.


RD60xx original Wi-Fi modules connect inward to the ESP AP. A bounded socket
worker probes identity read-only, then assigns separate endpoints by serial and
expected model. Portable framing, identity-binding and provisioning state live
with the driver module; the ESP worker owns socket/datagram I/O. The safety loop
never waits for network operations. Provisioning is node-level management because
the instrument identity is unknown initially, and never creates a lease or actor
implicitly. Its authenticated API is shared by generated CLI and a registered
node-setup UI module. Distinct configurable AP SSIDs allow the same firmware to
move source devices onto another node without adding a second integration path.


## Bench frontend modules

The `/dl24` entry point composes four responsibilities under `web/src/bench`:
`adapter.ts` manages node discovery, physical-device ownership, per-device
operations and a single prioritized request queue; `measurements.ts` discovers
channel views; `recording.js` retains bounded channel histories and traceable
CSV/integrals; `modules/` owns specialized device behavior. Device/component
selection does not change ownership or recording state.

`BenchModule` declares matching, input steps, control prerequisites, optional
typed settings/reset/target adapters, pushed-settings decoding, status descriptors
and an optional UI mount.
Its view supplies render and draft save/restore hooks. Register an extension
in `benchModules`; specialized UI can mount elements marked `data-module` with
its module ID. Common components work without a module: actor actions generate
the available controls and scalar readings generate channel tiles/curves.
Driver-specific clients, setting fields and protocol interpretation belong in
modules, not the common adapter or chart. Backend protocol facts stay in drivers.
The existing single-device stream subscription supplies selected-device pushes;
all instruments share its authenticated RPC channel for cache polling and
commands. Operation events resolve per-device waiters without operation polling.
Cookie/CSRF binding, conservative sample clocks and explicit HTTP STOP recovery
are retained from the native DL24M stream integration.

The UI keeps one state per physical device and actor states keyed by actual
component IDs. Pending operations capture their target and generation rather
than consulting the dropdown during confirmation. Multi-device shutdown submits
all STOPs through the same serial RPC queue before awaiting their results;
confirmation failures are collected independently. Heartbeats retain priority
over ordinary work. Display preferences do not control acquisition. Native
counters and computed per-device integrals use separate CSV row kinds.


The Bench layout shell owns only presentation: a single 56 px global header,
modeless global panels and the mobile selected-device drawer. Device selection,
reservation and instrument settings live in the left rail. The main area gives
remaining height to the Canvas viewport. Content-sized device groups wrap in a
flex grid inside a measurement dock that defaults to at most 144 px. A persistent,
viewport-bounded presentation preference controls its pointer/keyboard resizer;
expanded menus and diagnostics never grow the normal header. Canvas
resolution follows the viewport and pixel ratio. Measurement values are native
pressed-state buttons that toggle the same curve preference as Display; value
visibility remains independent. Both Canvas strokes and tiles use one channel
colour, derived from channel identity and the current theme, independent of
discovery order. The measurement area provides curve identity and selection
without a separate legend. These layout changes do not alter device ownership
or the measurement/recording pipeline.

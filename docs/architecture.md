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

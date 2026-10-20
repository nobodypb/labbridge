# User stories

These stories are the version-controlled source of acceptance criteria. GitHub
issues mirror their IDs and link here. A story is done only when its acceptance
checks pass and its hardware claims match the evidence.

| ID | Story | Depends on | Status |
|---|---|---|---|
| LB-001 | As a contributor, I can reproduce the baseline and run CI without private artifacts. | — | Done |
| LB-002 | As an integrator, I generate clients and firmware types from the same API contract. | LB-001 | Done |
| LB-003 | As an operator, I configure multiple instrument instances within board limits. | LB-001 | Done |
| LB-004 | As a driver author, I use isolated transport endpoints and add a module without core changes. | LB-003 | Done |
| LB-005 | As a DL24M user, I retain the confirmed codec, readback and persistent shutdown behavior. | LB-004 | Done |
| LB-006 | As an automation author, I receive confirmed results after bounded on-node recovery. | LB-005 | Done |
| LB-007 | As an operator, I use multiple RD60xx sources with independent identities and CC/CV telemetry. | LB-004 | Hardware acceptance open |
| LB-008 | As a battery tester, I select interchangeable interlocks and cell-temperature channels. | LB-004 | Hardware acceptance open |
| LB-009 | As an operator, I retain local limits and safe shutdown when host or network fails. | LB-005, LB-007, LB-008 | Done |
| LB-010 | As a script author, I address devices across named nodes over HTTP or USB using the SDK/CLI. | LB-002, LB-009 | Done |
| LB-011 | As an operator, I provision Wi-Fi and operate device capabilities through modular UI panels. | LB-002, LB-003 | Done |
| LB-012 | As a cell tester, I run external charge/discharge/rest/pulse scripts and export traceable results. | LB-010 | Done |
| LB-013 | As an integrator, I read one factual hardware/API/protocol handbook rather than an investigation diary. | LB-002, LB-005 | Done |
| LB-014 | As a maintainer, I release only with explicit simulation and hardware acceptance results. | all | Hardware acceptance open |
| LB-015 | As a script author, I address source/sink/sensor components through one typed API regardless of device model. | LB-002, LB-004 | Done |
| LB-016 | As a cell tester, I optionally use direct source/load switching and independent meters/additional temperature channels. | LB-012, LB-015 | Done |
| LB-017 | As an operator, I configure INA228 I²C and Yocto-Watt-V2-C measurement providers. | LB-015 | Implemented; hardware acceptance open |
| LB-018 | As an API consumer, I understand operations, units, prerequisites and failure handling directly from OpenAPI and generated clients. | LB-002, LB-015 | Done |

| LB-019 | As an operator, I provision and automate RD6018 sources on the ESP AP alongside DL24M BLE, with bounded recovery and measured coexistence acceptance. | LB-007, LB-011 | Provisioning and no-load source control validated; loaded/two-source/coexistence acceptance open |

## Acceptance criteria

- **LB-001:** reproducible tool versions; clean public-tree checks; preserved protocol and controller tests pass; CI builds documented boards; no secrets/manufacturer binaries.
- **LB-002:** OpenAPI validates; generated Python/TypeScript/C compile; generation is deterministic; real responses and errors satisfy schemas; no untyped command bag.
- **LB-003:** two identical devices and mixed devices work; duplicate identities, over-capacity transports and pin conflicts are rejected before allocation.
- **LB-004:** independent streams and connection generations; cancellation and priority stop; a new test module registers without central device-ID branches.
- **LB-005:** 22 audited reads, exact 9/10-byte frames, corrupted FF55 recovery, settings/readback and OFF journal retain behavior; no six-byte fallback.
- **LB-006:** PC timing; at most 10 s; bounded read/write/reconnect handling; no stale acknowledgement accepted; no automatic ON after disconnect; benchmark and fault-injection results recorded.
- **LB-007:** register/CRC/address tests; independent serial identities; output/setpoint verification; external-probe identity distinct from internal temperature; real support remains unverified until exercised.
- **LB-008:** both paths open on boot/stop; break-before-make with feedback; failure cancels transition; DS18B20 CRC/freshness; equivalent provider contract for future network relays.
- **LB-009:** lease expiry, stale telemetry, threshold crossing, disconnect and restart produce explicit safe/unknown states; local safeguards continue without the host.
- **LB-010:** generated typed operations over both transports; per-node namespaced IDs; failed distributed lease acquisition unwinds; write timeout does not replay a mutation.
- **LB-011:** open AP, scan/select/PSK; separate Wi-Fi/device pages; capability panels; disabled/unknown states visible; browser integration tests.
- **LB-012:** both 1S chemistries require explicit profile limits; full simulated cycles through SDK/API; fresh-cell-temperature/interlock required; gaps excluded from integration; CSV and JSON metadata; cleanup after exceptions.
- **LB-013:** frames, scales, hardware constraints and examples; probable claims labeled; a small separate uncertainty/evidence document.
- **LB-014:** reproducible board builds and clean-tree generation; DL24M bench checks preserve original settings; full battery hardware acceptance remains open until source, interlock and sensor are available and commissioned.

## Definition of done

Observed red test → implementation → green tests → refactor → required checks →
reviewable PR. Update this status and evidence with the same change. Do not mark
a story done using a narrower test than its acceptance criteria.

## Delivery evidence

The runtime and board adapters are delivered in PR #15. CLI, complete simulated
battery workflows, profiles and consolidated documentation are delivered in PR #16. [Acceptance](acceptance.md) separates actual DL24M bench evidence from portable
and browser tests. LB-007/008 have software implementations; their physical
commissioning, and the complete battery circuit in LB-014, remain open. There is
no claim of a production-ready battery test system or of tested optional PSU BT.

## New acceptance criteria

- **LB-019:** host commissioning sends both stages without an intermediate operator callback. After the bounded PC server-address window, send stored AP credentials automatically; a fresh session-scoped TCP identity confirms completion. A cached candidate or UDP ACK alone cannot confirm success. Timeout/cancellation/session changes must not advance or cancel another session. The initial physical OFF/setup prerequisites remain required.

- **LB-010:** stable udev/by-id aliases identify a physical ESP despite tty renumbering; native USB/JTAG line control uses the resolved USB identity while opening the selected durable path. Separate bench roles never rely on ttyACM enumeration.
- **LB-010:** native USB responses remain complete through reserve/release/STOP cycles as well as read-only discovery. Use buffered interrupt-driven console I/O; exercise the actual connected board, not only host framing fixtures.
- **LB-001:** the StickS3 profile enables and boot-tests its octal PSRAM for the application heap; a repeated hardware operation run retains its boot identity. Memory-pressure admission/recovery on boards without PSRAM remains a separate physical acceptance limit.
- **LB-019:** tolerate bounded missing reads before the first activation write; never repeat a sent activation after uncertain echo or mismatched readback. A one-second response deadline adds no mandatory pause after a successful reply. Preserve the ten-second operation deadline and local freshness/activation safeguards.

- **LB-019:** ordinary RD6018 and existing RD6024 profiles enforce identity/scaling/model ceilings before writes; up to two AP TCP sources use independent serial-bound streams; fragmented/corrupt/late replies cannot confirm the wrong command; reconnect requests verified OFF and never replays ON. Authenticated UI/generated USB CLI guide two-phase original-module provisioning with phase/session checks, deadlines and cancellation; explicit limits/ETag registration. First deploy AP+STA+DL24M BLE on one ESP, run 60-minute coexistence acceptance and fault tests, split onto a second ESP with the same firmware if unstable. Physical provisioning/bench/soak evidence remains open until performed; a simulated second source is not hardware acceptance. Optional AP WPA2 security is configured independently of station credentials, remains open by default, and can be explicitly restored. Protected SmartConfig uses driver-encrypted UDP. After a W01 module did not receive the first UDP-based attempt, radio submission/completion diagnostics and a bounded direct AP broadcast sender characterize the actual delivery path; no reception is inferred from successful submission. PC v1.0.0.22 analysis identifies a separate profile (literal phase-one RD6006 SSID, fixed 0xC7 byte, no BSSID suffix, rotating multicast and burst scheduling); the separate codec/scheduler must match published synthetic fixtures, retain packets under queue backpressure, honor the one-second inter-burst pause and expose join results independently before repeating radio trials. UDP join ACK remains distinct from TCP instrument identity. Discovery-only TCP connections must continue read-only identity probes and expire after a bounded missing reply, so a silently rebooted unregistered source cannot leave a stale serial claim blocking reconnect; preserve rejection of a second live identity. A W01 control exposed a stale discovery claim; read-only heartbeat acceptance and a physical source power-cycle check now cover its release and reclaim. Configured-source fault/STOP and coexistence checks remain open. After a successful PC-profile baseline, compare one change at a time using an explicit driver-local profile, record display/join/identity outcomes, and retain the baseline; optional target-SSID comparison changes only phase one.

- **LB-018:** every operation, parameter, schema and inline field has useful English documentation (reference fields use the shared schema); asynchronous confirmation, leases, STOP, freshness, units and device limitations are explicit; generated clients retain documentation; firmware validation excludes prose while preserving constraints; generation and contract checks pass.
- **LB-015:** discover multiple actor/sensor components per device; identical typed routes for sources/sinks and scalar readings; per-component routing validated; physical-device leases and priority STOP preserved; SDK/CLI/UI use the abstraction; hardware-specific routes remain optional compatibility APIs.
- **LB-016:** both chemistries run with and without a feedback switch; confirmed OFF before direction changes; independent meter selections and additional temperature channels recorded with identity/freshness/sign semantics; stale or rebooted selected sensors stop the run; original settings restored.
- **LB-017:** INA228 calibration/identity/register scaling/sign/overflow/freshness tests, shared-I²C address/resource validation, explicit shunt calibration; Yocto serial binding, function/unit/status and finite-value validation through supported official transports; unavailable sensors never fabricate zeros. Actual new hardware commissioning remains open.

Component routes, optional routing and measurement providers are delivered in PR
#20. LB-015/016 have portable, generated-client, browser and DL24M bench acceptance.
LB-017 software is implemented; actual INA228/Yocto hardware commissioning remains
open in issue #19. The native ESP Yocto USB transport is not part of this delivery.

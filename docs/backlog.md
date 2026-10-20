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
| LB-021 | As a DL24M operator, I use verified modes, native telemetry and passive diagnostics over one authenticated WebSocket connection. | LB-020 | Accepted; implementation and commissioning in progress |
| LB-020 | As a DL24M operator, I use the complete Bench interface on the ESP with confirmed backend control and retained future features. | LB-005, LB-011 | Implemented; OFF-only hardware acceptance passed |

| LB-019 | As an operator, I provision and automate RD6018 sources on the ESP AP alongside DL24M BLE, with bounded recovery and measured coexistence acceptance. | LB-007, LB-011 | PC provisioning profile correction and hardware acceptance open |

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

- **LB-019:** ordinary RD6018 and existing RD6024 profiles enforce identity/scaling/model ceilings before writes; up to two AP TCP sources use independent serial-bound streams; fragmented/corrupt/late replies cannot confirm the wrong command; reconnect requests verified OFF and never replays ON. Authenticated UI/generated USB CLI guide two-phase original-module provisioning with phase/session checks, deadlines and cancellation; explicit limits/ETag registration. First deploy AP+STA+DL24M BLE on one ESP, run 60-minute coexistence acceptance and fault tests, split onto a second ESP with the same firmware if unstable. Physical provisioning/bench/soak evidence remains open until performed; a simulated second source is not hardware acceptance. Optional AP WPA2 security is configured independently of station credentials, remains open by default, and can be explicitly restored. Protected SmartConfig uses driver-encrypted UDP. After a W01 module did not receive the first UDP-based attempt, radio submission/completion diagnostics and a bounded direct AP broadcast sender characterize the actual delivery path; no reception is inferred from successful submission. PC v1.0.0.22 analysis identifies a separate profile (literal phase-one RD6006 SSID, fixed 0xC7 byte, no BSSID suffix, rotating multicast and burst scheduling); implement it with the published synthetic fixtures before repeating radio trials. UDP join ACK remains distinct from TCP instrument identity.

- **LB-018:** every operation, parameter, schema and inline field has useful English documentation (reference fields use the shared schema); asynchronous confirmation, leases, STOP, freshness, units and device limitations are explicit; generated clients retain documentation; firmware validation excludes prose while preserving constraints; generation and contract checks pass.
- **LB-015:** discover multiple actor/sensor components per device; identical typed routes for sources/sinks and scalar readings; per-component routing validated; physical-device leases and priority STOP preserved; SDK/CLI/UI use the abstraction; hardware-specific routes remain optional compatibility APIs.
- **LB-016:** both chemistries run with and without a feedback switch; confirmed OFF before direction changes; independent meter selections and additional temperature channels recorded with identity/freshness/sign semantics; stale or rebooted selected sensors stop the run; original settings restored.
- **LB-017:** INA228 calibration/identity/register scaling/sign/overflow/freshness tests, shared-I²C address/resource validation, explicit shunt calibration; Yocto serial binding, function/unit/status and finite-value validation through supported official transports; unavailable sensors never fabricate zeros. Actual new hardware commissioning remains open.

Component routes, optional routing and measurement providers are delivered in PR
#20. LB-015/016 have portable, generated-client, browser and DL24M bench acceptance.
LB-017 software is implemented; actual INA228/Yocto hardware commissioning remains
open in issue #19. The native ESP Yocto USB transport is not part of this delivery.

## LB-020 — DL24M Bench page (implemented; OFF-only hardware acceptance passed)

As a DL24M operator, I use the complete DL24 Bench interface at `/dl24` on the
ESP with LabBridge HTTP control. Preserve every upstream function area: unsupported
controls remain visible, disabled and explained, ready for later backend support.
Keep the unchanged GPL-3.0-or-later upstream snapshot separate from adaptations.

Acceptance: choose one DL24M; authenticate; reserve only that device; confirmed
operations and independent STOP; stop/confirm OFF before releasing or switching;
no automatic ON/replay after loss or reboot. Existing current/cutoff/timer/settings
routes enforce their limits. All modes, calibration/reset and protocol diagnostics
remain visible; unsupported actions never send commands. Retain themes, chart,
all measurement tiles, live/load recording and CSV. Missing values remain unknown;
capacity/energy are labelled browser session integrals with freshness, identity,
boot and gap handling. Bound chart/log memory. Serve all assets locally under the
existing CSP. Browser/portable/generation checks and all board builds pass;
physical bench acceptance is recorded separately with original settings restored.

Follow-ups: active CV/CR/CP commissioning, calibration/zero/factory-reset APIs,
HID-frame/snapshot diagnostics, fan RPM and verified native counter scales,
external probe telemetry, other device themes and persistent recording. Their
absence is not permission to expose raw commands or claim hardware support.

LB-020 evidence: 11 Web unit tests and 21 browser scenarios pass, alongside
portable/generation checks and all three board builds. The embedded StickS3 page
passed authentication, reservation, brightness change/restoration, STOP and
release on 2026-10-20; all 14 settings, the original current target and confirmed
OFF were restored. Real HTTP resets under parallel polling led to a single-request
queue with priority STOP/heartbeat and cancellation of queued commands. Loaded
operation through the new page and battery testing were not performed (0 V input);
the existing physical limitations in LB-014 remain open.

## LB-021 — DL24M capabilities and one-connection Bench (accepted)

Provide a factual supported/unverified/unavailable status for every retained
Bench feature. Extend the typed API, DL24M driver and UI for confirmed modes,
measurements, passive frame/register diagnostics and supported maintenance.
Reuse the same authenticated dispatch/leases/operations over a single WebSocket
connection, including explicit lease heartbeats. HTTP/USB remain compatible.

Acceptance: red tests before behavior changes; existing codec/recovery and OFF
journal preserved; no fabricated HID mappings, gains, sensor values or counter
scales. Mode changes snapshot and restore on failure, require OFF and verified
readback; active operation enforces local limits and bounded deadlines. Diagnostics
are passive and bounded. Maintenance functions without proven semantics
remain explicitly unavailable. WebSocket authenticates before any data, validates
messages, shares API authorization, prioritizes STOP/replies, bounds buffering,
reports gaps and never replays writes after loss/reboot. One authenticated UI
session per board; socket resources are board-owned. UI uses pushed snapshots and
operation events with no steady-state HTTP polling. Portable/generated/browser
checks and all board builds pass. Physical StickS3 tests record/restore settings;
mode pulses use a manually limited <=25 V / <=0.15 A source, no battery. No external
reference is available, so absolute accuracy/calibration are not commissioned.


LB-021 capability evidence and restrictions: [protocol/stream matrix](dl24m-bench-extension.md).
Remaining physical limits are explicit: external probe mapping and fan RPM;
calibration 46/4F reference, full data backup
and restore; no-load zero effects; full factory-reset restoration. No destructive
maintenance operation is exposed merely because its command number is known.
Legacy inactive-bank transactions occasionally timed out on hardware after mode
trials; checked STOP/recovery was retained. Typed mode/target operations and
explicit readback restored the original banks. Further BLE timing/soak evidence
is a follow-up, rather than a reason to relax recovery or replay writes.

# User stories

These stories are the version-controlled source of acceptance criteria. GitHub
issues mirror their IDs and link here. A story is done only when its acceptance
checks pass and its hardware claims match the evidence.

| ID | Story | Depends on | Status |
|---|---|---|---|
| LB-001 | As a contributor, I can reproduce the baseline and run CI without private artifacts. | — | In progress |
| LB-002 | As an integrator, I generate clients and firmware types from the same API contract. | LB-001 | TODO |
| LB-003 | As an operator, I configure multiple instrument instances within board limits. | LB-001 | In progress |
| LB-004 | As a driver author, I use isolated transport endpoints and add a module without core changes. | LB-003 | TODO |
| LB-005 | As a DL24M user, I retain the confirmed codec, readback and persistent shutdown behavior. | LB-004 | TODO |
| LB-006 | As an automation author, I receive confirmed results after bounded on-node recovery. | LB-005 | TODO |
| LB-007 | As an operator, I use multiple RD60xx sources with independent identities and CC/CV telemetry. | LB-004 | TODO |
| LB-008 | As a battery tester, I select interchangeable interlocks and cell-temperature channels. | LB-004 | TODO |
| LB-009 | As an operator, I retain local limits and safe shutdown when host or network fails. | LB-005, LB-007, LB-008 | TODO |
| LB-010 | As a script author, I address devices across named nodes over HTTP or USB using the SDK/CLI. | LB-002, LB-009 | TODO |
| LB-011 | As an operator, I provision Wi-Fi and operate device capabilities through modular UI panels. | LB-002, LB-003 | TODO |
| LB-012 | As a cell tester, I run external charge/discharge/rest/pulse scripts and export traceable results. | LB-010 | TODO |
| LB-013 | As an integrator, I read one factual hardware/API/protocol handbook rather than an investigation diary. | LB-002, LB-005 | TODO |
| LB-014 | As a maintainer, I release only with explicit simulation and hardware acceptance results. | all | TODO |

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

# Acceptance status

This is a development baseline, not a fully commissioned battery-test system.

## Automated acceptance

- 30 portable C++ behavior executables cover framing, queued transactions,
  connection changes, resource validation, driver identity, leases, idempotency,
  local guards, per-activation deadlines, interlock feedback and temperature.
- Python tests validate actual runtime responses against OpenAPI, generated
  request models, USB chunk framing, distributed lease rollback and error
  propagation. Host tests include both chemistries through complete simulated
  cycles, cleanup and reboot rejection, plus the CLI.
- The frozen baseline's codec/controller and 13 Python tests remain mandatory.
- 21 Web unit tests and 56 browser scenarios cover Wi-Fi scan/select/PSK,
  operation polling, lease/output behavior, unknown-state STOP and mixed modules.
- CI regenerates the API, checks drift, builds Python/TypeScript/C and firmware
  for StickS3, ESP32-S3-USB-OTG and classic ESP32. Board builds do not establish
  that every peripheral combination has been physically commissioned.
- LB-018 documents all 38 operations, 40 schemas, parameters and inline fields;
  shared reference fields use the referenced schema documentation. Generated
  Python/TypeScript descriptions and deterministic regeneration were checked.
  The annotation regression test preserves public prose and firmware property
  names/constraints while excluding prose/examples from ESP validation data.
  This documentation update required no hardware writes or flashing.

## DL24M bench acceptance

New asynchronous runtime on StickS3; actual display firmware Ver 2.0. Bench source
24 V, current limited to 1 A; bridge limits 25 V / 0.2 A / 5 W; CC 0.100 A.

- All 22 audited register queries answered over BLE.
- 33 setting/output/counter operations succeeded with fresh readback and bounded
  bridge recovery. CC 0.150 → 0.100 A, a measured 23.992 V / 0.101 A pulse, then
  independent OFF were included. All 14 setting families were changed/restored.
- Median host operation latency 0.851 s, maximum 1.655 s in that run. The timer
  required three writes on change and restoration; some other operations needed
  an extra write/read. The caller received successful confirmed operations.
- All three inactive setpoint banks were read, changed by one raw unit, restored,
  and returned to CC with the original CC target. Active non-CC regulation is
  intentionally uncommissioned and cannot be enabled through this driver.
- Original settings, CC 0.100 A and confirmed output OFF were restored. OVP was
  temporarily raised from its original 21 V to 25 V for the 24 V pulse, then
  restored to 21 V. Counter reset was explicitly authorized.

## Remaining physical acceptance

- Local lease expiry confirmed OFF on the final runtime. Reset during ON returned
  confirmed OFF without resuming. Reset during a multi-setting operation restored
  the original values. All 14 settings were subsequently re-read and matched;
  CC 0.100 A and OFF were confirmed. New-firmware USB Wi-Fi scan completed.
- Open-AP UI scan/select/PSK was confirmed by the operator in the earlier setup;
  new UI browser tests pass. Actual STA provisioning/reconnect needs a configured
  target network; it is not established by browser fixtures.
- Commission RD6024 serial identity, CC/CV register behavior and verified writes
  on actual sources; determine the optional Bluetooth module's transport.
- Commission relay polarity, feedback, power-up behavior, Kelvin voltage sensing,
  source reverse-current protection and external cell probe before any battery run.

Hardware acceptance stays open in LB-014; no simulated result closes it.


## Component and optional-meter extension

Portable checks cover source/sink routing, multiple actors on one physical
device, independently addressed STOP/state, signed scalar readings and common
external-temperature guards. Both chemistries complete direct-output and feedback
routing scenarios. An inline signed meter contributes capacity/energy; additional
temperature logging, stale-meter cleanup and reboot boundaries are checked.
Native Yocto USB is exercised through the generated API with injected SDK data;
the installed official SDK imports and the offline path returns unavailable.
INA228 tests cover identity, calibration, signed current/charge, conversion-ready,
overflow and freshness. Five browser scenarios include a composite source and a
read-only meter. Actual INA228 shunt accuracy, Yocto USB/network sampling and
all new sensor wiring remain uncommissioned. No new battery hardware run occurred.


The common component routes were also exercised on the physical StickS3/DL24M:
discovery, actor state and scalar readings, then eight verified operations
(current 0.150/0.100 A, cutoff 0.8/0.7 V, timer 60/0 s, bounded ON and independent
component STOP). Confirmations use A/V/s with units. The pulse measured 23.990 V /
0.100 A. All 14 original settings, CC 0.100 A and confirmed OFF were restored;
22/22 audited registers remained valid. This extends the DL24M evidence and does
not commission the optional new meter hardware.


## RD6018 AP extension (LB-019)

Portable tests cover ordinary RD6018/RD6024 selection and ceilings, explicit
RD6018P rejection, independent identities/two sources, corrupt/fragmented RTU
frames, duplicate/wrong-model binding, interrupted ON without replay, provisioning
packet vectors/checksums and session/phase/replay/deadline behavior. Generated
CLI tests exercise provisioning requests and usable USB token pairing; browser scenarios cover physical-OFF
confirmation, both phases, cancellation and ETag/explicit-limit registration.
The StickS3 firmware builds with the AP TCP service and provisioning sender.

Sustained loaded CC/CV, a second physical source, intentional connection-fault
tests and full coexistence acceptance remain open. Coexistence is tracked
separately and does not gate the initial single-source integration. None of
these results may be inferred from simulated sources. The read-only acceptance runner requires configured TCP sources, BLE
DL24M and an active station connection:

```sh
python tools/rd_wifi_acceptance.py --endpoint serial:/dev/ttyACM0 --source source-a --load load --output results/rd-ap-soak
# Add --source source-b when its Wi-Fi module is available. Default duration: 3600 s.
```

It records boot IDs, measurement freshness, configuration and connection counters;
an unexpected disconnect/rejection, stale sample or restart fails the run. Files
contain hardware identities and remain ignored/private. Active bench writes and
intentional fault injection are separate checks; this runner makes no mutations.

Initial general-profile broadcasts did not establish W01 pairing. Radio
submission/completion counts alone do not prove receiver acceptance. Switching
to WPA2 alone did not resolve the general sender's protocol differences.

Offline PC-profile checks independently verified all 256 CRC table entries
and synthetic phase-one/phase-two payloads, including per-triple CRC and total
XOR. Vendor binaries/decompiled sources are excluded. These checks establish
encoder consistency, not RF delivery.

Physical W01/RD6018 pairing with the PC-compatible profile was verified:
displayed server address, a new UDP join result, TCP model/firmware identity and
completed setup. The first incoming socket can time out before identity; a
subsequent connection succeeded. This establishes provisioning; the no-load
bench evidence below separately covers source control and shutdown.

The optional target-SSID profile changes only phase one's SSID field;
phase two remains identical. Codec and generated-client tests cover that
invariance. The default remains the PC-compatible profile.

Native ESP USB line-control behavior was verified through symbolic paths;
UART adapters retain their reset wait. Tests use synthetic aliases and USB
metadata. Physical setup details and installed host rules are not published.

Automatic host provisioning tests cover both phase advances without an operator
callback, a fresh TCP identity, rejection of cached-candidate/ACK-only success,
initial OFF confirmation, timeout, cancellation and foreign-session isolation.
The default first-stage window is 60 seconds. This is orchestration acceptance;
the automatic timing still needs a fresh physical pairing trial. Previous manual
handoff trials establish PC-profile RF compatibility, not this new timing.

A target-SSID trial displayed the server address but produced no new join
result or TCP identity. Its late management transition prevents a clean
end-to-end comparison.

A same-session target-SSID repeat likewise produced no new join/TCP evidence.
The PC-compatible profile remains the supported default. Differing transition
delays prevent claiming a universal receiver requirement.

A subsequent full-PC control restored WLAN setup and fresh identity replies,
but exposed a stale discovery socket retaining its serial claim. The listener
rejected new connections as duplicates. This is separate from WLAN reception.

Discovery-recovery acceptance covers read-only identity probes, a bounded
missing-response deadline, rejection of late/changed identities, serial
release/reclaim and live-duplicate rejection. Required checks and generation
drift checks pass; configured-device driver recovery is preserved.

### No-load source control

An ordinary RD6018 (raw model 60181, firmware 1.39, W01) was exercised through
the real AP TCP connection. Fourteen checked setting/output requests succeeded,
each with a single write and fresh identity/readback. These included 1/3.3/5 V
targets, 0.05/0.10/0.20 A limits, compatibility and common-component routes, and
output switching. Median reported operation duration was 602.5 ms, maximum
1646 ms. One activation recovered a missing pre-activation read; the activation
write itself was sent once. The no-load reading settled to 5.02 V / 0 A for a
5 V target. Output-enable confirmation preceded voltage settling.

Independent priority STOP, a two-second activation deadline and expiry of a
two-second lease each returned fresh confirmed OFF. Lease-expiry OFF was observed
2582 ms after lease creation, before the five-second activation deadline.
Protection status remained zero. Sensor discovery/readings, actuator state,
idempotent replay/conflict and invalid step/model-limit rejections passed.
Original voltage/current targets were restored with fresh confirmed OFF.
This establishes command/telemetry behavior; it does not calibrate the voltage
or current measurements or validate CC under load, hardware OVP/OCP, presets,
battery wiring or a battery workflow. Additional telemetry is covered below.

### Complete source telemetry

The configured-source read covers registers 0–41 in one identity/CRC-checked
response. API/codec tests cover the 32-bit native counters, 0.01 V input/battery
scales, /1000 Ah/Wh scales, a valid zero-temperature reading and unavailable NTC
sentinels. Runtime responses, component discovery and readings are validated
against OpenAPI. Discovery retains the shorter identity read.

Physical RD6018 firmware 1.39 returned all ten common sensor components:
output V/I/W, input voltage, battery voltage/presence, charge, energy and
internal/external temperatures. Both temperature probes produced valid readings.
Thirty consecutive coherent snapshots supplied 300 valid sensor readings across
21 distinct samples, with unchanged boot identity. Generated USB CLI and HTTP
returned the complete telemetry, including the native counters.

The extended-read firmware repeated all fourteen checked setting/output requests
with one write each (median 472 ms, maximum 704 ms), plus independent STOP,
activation-duration and lease-expiry shutdown. Five additional setting/output
requests and STOP passed over authenticated HTTP. Original targets were restored
with fresh confirmed OFF and protection status zero. This commissions the
requested source controls and telemetry transport. Native counter accuracy under
load, sensor calibration, sustained loaded CC/CV, a second physical source and
coexistence remain separate acceptance work.

Regression tests reproduce missing background reads before ON, lost ON echo,
ON readback mismatch and 600 ms device replies. They require bounded recovery
before activation and no second activation write after one was sent. The
response deadline is one second, with no added pause after successful replies.

### Console and memory acceptance

Repeated physical reserve/release/STOP checks exposed truncated console replies
and an allocation abort while serializing retained-operation responses. Native
USB uses the interrupt-driven buffered console driver. The StickS3 profile
enables and boot-tests its octal PSRAM and adds it to the application allocator.
RAM-only board profiles still need separate memory-pressure admission/recovery
acceptance; larger PSRAM does not establish that those profiles are commissioned.

With both changes, 80 consecutive physical reserve/release/STOP cycles plus 241
read requests passed in 60.242 seconds with unchanged boot identity. The boot
memory test detected 8 MB and passed. Authenticated HTTP separately confirmed
five setting/output requests and independent STOP, restored the original targets
and left OFF. Three generated USB CLI calls (actor state, measurements and
temperature) returned valid results with exit code zero.
After a bridge restart, the source reconnected automatically and returned fresh
confirmed OFF, restored setpoints and zero protection status.

The hardware check prints aggregate results only and uses no fixed deployment
identity. Its default mode is read-only. Optional lease checks require a ready
actuator with confirmed OFF and never enable it:

```sh
PYTHONPATH=host/src:host/generated python tools/bench_console.py --endpoint "$BRIDGE_ENDPOINT" --samples 80 --lease-device example-source
```

Use an exclusive `serial:` endpoint and a configured device ID. Keep any local
captures or deployment mappings private. This check does not substitute for
active source or coexistence acceptance.

Physical discovery-recovery acceptance verified release of a stale serial
claim after a source power cycle, automatic reconnection to the same identity
and continued heartbeat replies without restarting the bridge. No duplicate
identities were rejected after recovery. Output/setpoint writes were not part
of this check. This validates unregistered discovery recovery only; configured
source STOP/fault tests, battery commissioning and the full coexistence soak
remain open. All documented board builds and required CI checks passed.

## DL24M Bench (LB-022)

The missing recording module and `/dl24` route were observed failing before
implementation. Further regression tests caught and fixed out-of-order sample
integration and overwritten language selection. Browser coverage checks every
original HTML ID, unsupported action gating, single-device leases, confirmed
commands, failed STOP blocking device switching, lease/network/reboot failures,
stale measurements, live/load monitoring, local assets/source/licence downloads,
CSV, both themes and narrow layouts. Browser screenshots use synthetic devices.
Session tests cover units, bounded buffers, invalid/duplicate/late samples,
identity/boot separation and gap handling. A real HTTP connection-reset failure
under parallel browser requests was reproduced with bounded-connection browser
tests. All API traffic now shares one connection, with priority STOP/heartbeat,
queued-command cancellation and no transport retry. A regression test verifies
that a queued ON cannot run after priority STOP.

On 2026-10-20, the final frontend was served entirely by the physical StickS3
running application commit `c355180`, with no browser asset override. Authentication,
single-device reservation, brightness change/readback/restoration, independent
STOP and release passed through the real HTTP API. All original 14 settings and
the 0.100 A current target matched the pre-flash snapshot afterwards; output was
confirmed OFF. Local JavaScript, CSS, GPL text and corresponding-source downloads
returned successfully. The load input was fresh 0 V / 0 A throughout this check;
no energized-load pulse or battery test is claimed.

Validation passed: `tools/check.sh` (including 21 portable C++ tests and the
Python/reference suites), deterministic generation/drift, TypeScript/Webbuild,
11 Web unit tests, 21 browser scenarios, formatting and public-tree checks.
ESP-IDF 5.5.2 builds succeeded for StickS3, USB-OTG and ESP32; each app fits its
configured partition. Physical acceptance above covers StickS3/DL24M only.


### Bench password integration (2026-10-21)

The initial Bench password scenario failed because `/dl24` had no password form.
It now uses the same `UiAuth` forms and cookie session as `/devices` and `/wifi`.
The Bench backend fixture accepts only the session cookie and rejects mutations
without the session CSRF header. Its original fourteen control/scheduling
scenarios now use password login, and ten additional scenarios cover cookie-only
commands/heartbeat/STOP, reload/navigation, expired sessions without ON replay or
automatic reservation, successful and failed STOP before logout, cancelling a
queued ON during logout, STOP/heartbeat during a held password change, AP/LAN
bootstrap guidance, and expiry during initial discovery. The last regression
first reproduced a false `Connected` message after expiry; discovery now checks
its connection generation before reporting success. Late reservation responses
also cannot restore ownership after the session changes.

Password requests use a separate client from the serialized instrument queue;
only the latter has a four-second request timeout. Session loss clears pending
commands, polling and lease heartbeat. A new login resumes read-only discovery.
The logout path waits for confirmed device STOP and lease release before ending
the cookie session; a failed shutdown still ends login and reports uncertainty.
The unchanged upstream snapshot and corresponding-source delivery remain intact.

Software acceptance: `tools/check.sh` passes (23 portable C++ executables,
three contract tests, 31 host tests and the reference suites), as do 11 Web unit
tests, TypeScript/Webbuild and all 45 browser scenarios (24 for Bench). Black,
public-tree checks and generation drift pass. ESP-IDF 5.5.2 builds pass for
StickS3, USB-OTG and classic ESP32 with the updated embedded frontend.

These are browser and portable software checks. The earlier Bench hardware
acceptance used bearer login; the new Bench password workflow has not been
flashed or exercised with laboratory devices. Hardware connections remain paused
at the operator's request. The version-one 114–116 s password cost failed the new three-second requirement.
Version-two performance evidence is recorded separately below.

## UI password sessions (LB-020; initial version-one commissioning)

The initial acceptance tests failed on missing authentication services and USB
password recovery; the browser LAN-scan scenario failed because no password
login existed. Portable tests now exercise the same access policy used by the
firmware: AP/LAN isolation, USB-only token retrieval, password bootstrap and
recovery, Unicode/space preservation, invalid JSON NUL rejection, CSRF/origin
checks, session capacity/expiry/logout/reboot/password replacement, failed
storage and login throttling. A deliberately blocked password derivation leaves
authorization, portable node lease heartbeats and STOP dispatch usable.

Browser tests cover LAN scan after login, public AP scan, initial-password
guidance/confirmation, reload/navigation, wrong credentials, password changes,
logout after a failed lease STOP and no actuator replay after session loss.
Generated Python password models redact credentials while transport serialization
preserves the exact value. Password computation is queued separately from HTTP
and the USB console; USB replies retain their original LABJSON request ID.

Hardware commissioning on 2026-10-20 found that ESP-IDF's IPv6 dual-stack HTTP
socket was decoded as `sockaddr_in`. This rejected valid LAN login origins and
failed to recognize the AP. A regression test first reproduced the failure;
the shared transport helper now decodes IPv4, IPv4-mapped IPv6 and IPv6 from
`sockaddr_storage`. Tests include actual dual-stack socket acceptance and the
firmware's AP/LAN authorization path. Captive-portal classification uses the
same helper.

Before stacking on PR #26, the corrected application was flashed on the DL24M StickS3 (application SHA-256
prefix `97d2eb7e09370640`). A private full-flash backup was verified against the
device, with the original NVS retained separately. Before the authenticated
tests, configuration, all 14 DL24M settings, current target and bearer token
matched the baseline; output was confirmed OFF with valid fresh telemetry.

| Actual StickS3 check | Result |
| --- | --- |
| LAN scan without credentials / LAN first-password setup | `401` / `403` |
| USB first password | 114.772 s |
| LAN password login | 115.365 s; HttpOnly, SameSite=Strict, 43,200 s cookie |
| Cookie-authenticated LAN scan | Four networks; scanning → complete in 5.326 s |
| Real LAN browser | Login resumes scan; reload/navigation retain login; reserved lease STOP accepted before logout |
| Browser login plus automatic scan | 119.432 s in the instrumented successful run |
| Missing CSRF / foreign Origin on cookie mutations | Both `403` |
| Logout / simultaneous second login while hashing | Scan returns `401` / second login returns `429` |
| Existing bearer / USB password recovery | Both work; recovery 116.151 s |
| AP Wi-Fi status and scan without UI credentials | `200`; four networks through an additional ESP32 WLAN client |

During a real HTTP password verification, HTTP STOP was accepted in 99 ms and
USB STOP in 57 ms. Both reached confirmed OFF before login completed; operation
durations were 179 ms and 116 ms respectively. During USB password recovery,
USB STOP was accepted in 56 ms and confirmed OFF in a 116 ms operation before
the password response. The boot ID remained unchanged. These were STOP tests
with the load already OFF; no ON command or battery operation was performed.

The AP client associated with the original protected AP, received an AP-subnet
address and verified the target bridge's boot ID. This establishes a real
wireless AP scan. The temporary client application was subsequently removed:
the additional ESP32's original overwritten flash region, including its NVS,
was restored and hash-verified through the dedicated test bridge connection.

The original DL24M-stick NVS was restored and hash-verified, removing the
temporary UI password and restoring its existing Wi-Fi/AP/token configuration.
The operator then restricted further hardware access to the test-stick alias
and requested no laboratory connections while other tests run. The final live
device recheck and AP first-password setup were consequently left open. One
earlier real-browser scan wait timed out without sufficient diagnostics; the
instrumented repeat above passed. Repeated/soak behavior remains open.

`tools/check.sh` passes (23 C++ executables, three contract tests, 31 host tests
and 13 reference tests); all 45 browser scenarios pass on an isolated local
test server. ESP-IDF 5.5.2 builds for StickS3, classic ESP32 and USB-OTG pass.
The 282 generated files still match the deterministic regeneration snapshot.
The roughly two-minute version-one calculation was rejected for usability and
superseded by the version-two policy below.
HTTP remains unencrypted; optional HTTPS/mTLS login is LB-021.

### Usability-first version-two verifier

The accepted requirement is a complete login below three seconds on the target
ESP, with basic protection preferred over maximum offline resistance. New
verifiers use PBKDF2-HMAC-SHA256 with 6,000 iterations, random salts and an
explicit version-two record. Length rules, throttling, session limits, cookie
flags, CSRF/origin checks, USB-only token reading and nonblocking worker behavior
are retained. A new failing test first rejected the old 600,000-round policy.
Tests also require prompt rejection of legacy experimental records, no slow
legacy derivation or silent hash reinterpretation, and successful USB recovery.

Every published user entry point (`/`, `/wifi`, `/devices`, `/dl24`, `/dl24/`)
is covered by password-form, cookie reload and no-automatic-mutation browser
acceptance. Physical timing uses a fully backed-up isolated bridge with zero
configured laboratory devices; no device connection or actuation is part of
this commissioning. Deployment-specific connection details, credentials, raw
logs and backups remain in ignored local notes.

The real eight-client regression initially exhausted the default shared socket
pool after three accepted logins. The platform now reserves 16 sockets and
bounds HTTP to four connections with idle connection reclamation. All eight
sessions then logged in, the ninth was rejected, and an older cookie still
worked after its TCP connection was reclaimed. Session state is never evicted
by TCP cleanup. A real-browser submission initially stalled; two controlled
regressions reproduced a delayed pre-login scan clearing password-manager input
or a newly established login. Request/session tracking and disposal of obsolete
scan views fix both cases; the final real-browser run succeeds without delays
inserted to hide the race.

Integration with the current main branch exposed a separate USB regression:
resetting a 128-code-point Unicode password timed out when its JSON-escaped
password and confirmation exceeded the new 2 KiB receive buffer. The request
was sent as one ordinary USB JSON frame; HTTP remained responsive. The console
receive buffer now accommodates the complete permitted 16 KiB request plus its
delimiter, so descheduling the reader cannot truncate that frame in the ISR.
The same unchunked recovery request then passed on hardware.

Final version-two commissioning was repeated after integrating current main's
console, automatic provisioning, telemetry and StickS3 PSRAM changes. It used
the actual embedded assets from source `de6856d` (application SHA-256 prefix
`52223e798ee032ce`), with zero configured instruments throughout:

| Check | Observed result |
| --- | --- |
| Browser login on `/` | 0.975 s; cookie reload and CSRF logout pass |
| Browser login on `/wifi` | 0.960 s; cookie reload and CSRF logout pass |
| Browser login on `/devices` | 0.933 s; cookie reload and CSRF logout pass |
| Browser login on `/dl24` | 0.999 s; cookie reload and CSRF logout pass |
| Browser login on `/dl24/` | 1.020 s; cookie reload and CSRF logout pass |
| Shared session across pages | One `/wifi` login, then `/dl24`, `/devices` and `/dl24/` without another login; reload and logout pass |
| 17 correct API logins | 0.841–0.955 s; all below 3 s |
| Incorrect passwords and throttling | Six `401` responses across separate runs, maximum 0.920 s; after five consecutive failures the next attempt receives `429` |
| 128 Unicode code points / 512 UTF-8 bytes | Correct password succeeds; altered final character rejected |
| Password change with current password | 1.696 s; previous sessions invalidated |
| Session cap and recovery | Eight valid sessions, ninth rejected, existing cookie retained; USB recovery revokes sessions |
| STOP route during an active password job | HTTP 27.7 ms / USB 21.9 ms |
| ESP reboot | Old session rejected; persisted password logs in again in 0.885 s |

The STOP probes deliberately address an unconfigured synthetic ID and receive
`404`; they establish route/authorization/dispatch responsiveness, **not** a new
physical shutdown result. Portable actor tests exercise actual STOP dispatch;
the earlier version-one physical OFF-only STOP evidence remains separately
recorded above. No instrument was configured, connected, reserved or actuated
during version-two hardware commissioning. Bearer access, USB-only token
retrieval, LAN bootstrap denial, CSRF and foreign-origin denial also pass on the
isolated bridge.

The original complete 8 MiB test image, including all NVS and firmware, was
restored and checked against the device's full-flash digest. Its original
firmware did not provide a correlated USB JSON status response before or after
testing; restoration evidence is the complete image comparison, not an invented
post-restore API readback. Credentials, deployment identity, backups and raw
logs remain private. The final software checks pass: 23 C++ executables, three
contract tests, 37 host tests, the reference suites, 11 Web unit tests and 45
browser scenarios. Generation drift, Black and public-tree checks pass, and
ESP-IDF 5.5.2 builds fit StickS3, USB-OTG and ESP32 with the final frontend and
socket policy. Remaining AP-first-setup and laboratory commissioning limits are
still explicit; the three-second user-login requirement is now satisfied.


## DL24M native capabilities and WebSocket (LB-023)

Behavior tests were observed failing before changes to mode/target guarding,
rollback, native readings, passive tracing, shared stream dispatch/authentication,
explicit heartbeats, gap/operation retention, bounded queues, partial writes,
JSON depth/allocation handling and request-schema retention. A further browser
regression reproduced an enabled CC setpoint button rejecting an adjustment while
ON; the common current operation now retains that existing behavior. Codec files
and the upstream vendored Bench remain unchanged.

Initial stream validation before password-session integration: `tools/check.sh`
passes (27 portable C++ executables, API/Python/host
and frozen reference suites), generated drift is zero across 306 files, Black and
public-tree checks pass. TypeScript/Webbuild, 20 Web unit tests and 27 browser
scenarios pass. Browser scenarios cover all upstream control IDs, unavailable
controls without writes, authentication, lease conflicts/loss, delayed/failed
operations, STOP priority/fallback, reboot/disconnect without replay, mode/target
RPC, passive diagnostics, CSV, themes, narrow layouts and local-only assets.
Session tests distinguish native counters from browser integrals and reject
invalid, duplicate, late, stale, missing and cross-boot/device samples. Red tests
also reproduced queued telemetry appearing fresh on arrival. A conservative
subscription-to-device-clock bound now includes transport delay in reading ages,
including native values/temperatures. Delayed values lock ON and are excluded from
integration; fresh samples recover without resetting this bound.

ESP-IDF 5.5.2 builds of application revision `7f9eb61` pass on all three targets;
partition checks are included in each build. The build version has a `-dirty`
suffix for the pending acceptance documentation/local target-lock state; the
application source corresponds to the recorded revision:

| Target | Application bytes | App partition | Free |
|---|---:|---:|---:|
| StickS3 | 2,179,936 | 4 MiB | 48% |
| USB-OTG | 2,258,432 | 4 MiB | 46% |
| ESP32 | 2,163,360 | 3 MiB | 31% |

Hardware checks on 2026-10-21 used the operator-selected exclusive DL24M
StickS3. Deployment-specific connection details remain in ignored local notes.
It was connected as a station to the requested LAN; browser/stream checks used LAN access. The source
was manually set to 24 V / 0.15 A by the operator, with no battery. Original app
and configuration evidence stayed private. No unknown command, calibration,
factory reset, no-load zero or device-counter reset was performed.

Native mode commissioning used the actual BLE driver through both USB and
WebSocket APIs. Successful trials used a local 5,000 ms activation deadline and
explicit independent STOP; the WebSocket trials stopped after approximately
3.1–3.4 s. Representative fresh loaded readings were:

| Mode | Target | Voltage | Current | Result |
|---|---:|---:|---:|---|
| CR | 240 Ohm | 23.992 V | 0.099 A | confirmed ON, fresh samples, STOP OFF |
| CP | 2.4 W | 23.992 V | 0.099 A | confirmed ON, fresh samples, STOP OFF |
| CV | 23.5 V | 23.515 V | 0.154 A | confirmed ON, fresh samples, STOP OFF |

The USB CV trial reported up to 0.156 A. These are uncalibrated instrument
readings against a nominal 0.15 A source limit, not an independently measured
current-limit or absolute-accuracy result. Guards use the configured local
25 V / 0.2 A / 5 W envelope in addition to the physical source limit. CV also
requires fresh OCP/OPP, CR checks at maximum configured voltage, and CP checks
at the fresh nonzero cutoff. Mode/target changes remain OFF-only.

A separate approximately 92 s CC stream run at a 0.050 A target, with a local
100,000 ms deadline, recorded 160 samples and no stream gaps. The native deltas
were 89 s, 1 mAh and 30 mWh; the fresh-sample trapezoidal integrals were
1.21389125 mAh and 29.13025296 mWh. Together with the native firmware analysis,
this confirms relative mAh/mWh scaling of reads 14/15. The API converts these to
Ah/Wh and exposes each value's source, quality and age, including common charge
and energy sensor routes. Browser-session integrals remain separate. No external
reference or absolute calibration was available.

Actual stream checks confirmed no device data before authentication, 401 for
unauthenticated RPC/wrong token, closure after the three-second authentication
deadline and admission of only one authenticated session. WebSocket ping did not
renew a one-second lease; explicit heartbeat RPC did. Independent STOP confirmed
OFF. A slow receiver combined with 1,000 read-only diagnostic RPCs caused stream
closure and released its slot. The lease expired to stopped/OFF while fresh valid
measurements remained readable over HTTP (approximately 0.40 s for recovery
reads). Oversized and excessively nested messages closed their connections.
No ESP panic, stack overflow or allocation abort was observed in these checks.
An earlier build without PSRAM reached only 4,776 bytes of minimum free heap.
After incorporating the current base and rebuilding the StickS3 PSRAM profile,
the same tests passed with 8,368,027 bytes of aggregate minimum free 8-bit heap
(including external RAM), 4,012 bytes of stream-worker stack headroom and a 39 ms
maximum observed snapshot build. The new authenticated probe confirmed that the
overloaded server session released its slot; the slow client still had unread
frames buffered and had not reached EOF within its four-second drain window.
This is bounded stress evidence, not a long-duration internal-RAM/coexistence soak.

The real `/dl24` page and assets were served by the ESP under its CSP, without
asset overrides or internet dependencies. Authentication, reservation, supported
mode controls, native units, brightness change/readback/restoration, disabled
maintenance, passive BLE/register diagnostics, CSV, browser-only Clear, keyboard
Escape and both themes at a 390 px viewport passed. Normal monitoring and command
confirmation used one WebSocket with zero HTTP API polling. Explicit STOP after
closing that socket used HTTP successfully. Resetting the ESP while OFF broke the
connection and locked control; explicit reconnect saw a different boot identity,
OFF and no reservation. No ON request was sent or replayed by these browser tests.

Intermittent native BLE readback timeouts were observed during legacy bank cleanup
and, on a later run, a typed CP target transaction after a successful CR pulse.
The failed transaction reported unknown output, invalidated ownership and did not
lead to another ON. STOP and persistent rollback remained in force. One explicit
restoration attempt also timed out; after fresh OFF/state readback a separate
paced restoration confirmed all 14 original settings, all three original inactive
targets (5 V / 5 Ohm / 5 W), original CC 0.100 A and final OFF. Failed writes were
not automatically replayed. BLE timing/recovery soak remains open in LB-023.
External probe/fan mapping, full maintenance backup/restore and absolute accuracy
remain explicitly uncommissioned. None of these checks establishes battery-test
readiness or RD-source/BLE coexistence acceptance. Final read-only checks on the
integrated application reconfirmed all 14 settings, CC 0.100 A, ready/OFF, fresh
measurements and Ah/Wh units on both native and common sensor routes; no mode
mutations occurred after the original inactive targets were restored.


### Completion with the current password-session baseline

The final implementation includes the current main branch's password worker,
sessions, console and PSRAM profile. Portable authentication tests bind stream
access to the upgrade cookie, CSRF and trusted Origin; reject credential/header
replacement; and revoke access on logout, expiry and password replacement.
Browser tests preserve the existing password flows and all original control IDs.
Additional red tests reproduced loss of a late native readback during backoff,
repeated uncertain activation, missing fine temperature rendering, an enabled but
ignored Snapshot during an in-flight settings operation, and a missing persistent
CV-notice preference and a recording-gap notice erasing a confirmed STOP result. Those cases now pass. Counter-reset confirmation/cancellation
and OFF gating are characterized without discarding real device counters.

Final software validation: 30 C++ executables, three contract tests, 37 host tests,
the frozen reference suites including 13 Python cases, 21 Web unit tests and 55
browser scenarios pass. TypeScript/Webbuild, Black, public-tree and deterministic
generation drift checks pass (321 tracked generated files). All three ESP-IDF
5.5.2 application builds fit their board partitions. The unchanged native codec
and vendored upstream snapshot have no diff against the main baseline.

The actual integrated StickS3 implementation was exercised over station LAN with
an operator-confirmed 24 V / 0.15 A PSU and no battery. Deployment assignments,
credentials, exact configuration backups and raw captures remain private.
Successful separate native mode trials again confirmed ON, fresh readings and
STOP/OFF: CR 240 Ohm at 23.991 V / 0.101 A; CP 2.4 W at 23.990 V / 0.100 A;
CV 23.5 V at 23.470 V / 0.153 A. Each activation had a 5,000 ms local deadline
and was stopped after about 3.2–3.8 s. These are instrument readings, not an
independent verification of the PSU limit or absolute accuracy.

A separate CC 0.050 A acceptance confirmed that a two-second lease expires to
OFF, and resetting the ESP during a bounded four-second ON activation returns
to a new boot, confirmed OFF and no automatic resumption. The helper waits for
fresh zero-current measurements and a complete post-boot settings catalogue;
OFF readback alone is not treated as a simultaneous fresh V/I/settings snapshot.
All 14 original settings, three original inactive targets and CC 0.100 A were
separately confirmed/restored. No native counter reset, calibration, no-load zero
or factory reset was used in this unattended completion run.

Actual cookie-stream authentication rejected absent cookies, wrong CSRF and
foreign Origin, and accepted the correct browser binding. A complete API password
login took 1.311 s. During a real password replacement, physical STOP admission
took 180 ms and an explicit lease heartbeat took 103 ms; the replacement then
revoked the existing stream. Independent bearer recovery confirmed physical OFF.
These are actual instrument routes, unlike the earlier isolated auth-only
unconfigured-ID latency probes.

The final real-browser acceptance loaded the ESP's own assets under its CSP,
with external requests blocked. Login, cookie reload, selected-device reservation,
brightness change/readback/restoration, locked maintenance, passive capture and
register snapshots, CSV, browser Clear, keyboard Escape, both themes and a 390 px
viewport pass. Normal monitoring/commands use one WebSocket with zero HTTP API
polling; explicit STOP after socket loss succeeds over HTTP. A boot change loses
the session and requires a new login, with OFF/no lease and no replayed ON.
One immediate post-flash connection attempt did not establish a stream; the
instrumented explicit-login repeat passed. No uncertain command was repeated.

The current raw-protocol hardware run again rejected unauthenticated/wrong-token
RPC, enforced the three-second auth deadline and one-session limit, distinguished
ping from explicit heartbeat, and confirmed independent STOP. A slow receiver
with 1,000 read-only diagnostics requests released its server slot; the lease
expired/OFF and fresh HTTP recovery reads completed in 0.406 s. Buffered data
prevented observing client EOF within the short drain window; a new authenticated
probe established slot release. Oversized/deep messages closed. The run recorded
no panic, stack overflow or allocation abort; aggregate minimum free 8-bit heap
was 8,343,075 bytes including PSRAM, stream-worker stack headroom 4,004 bytes,
and maximum snapshot construction 39 ms. This does not separately measure
internal-only RAM or establish other boards' physical memory headroom.

The repeated mode sequence still reproduced native `read_timeout`, including an
OFF-only sequence. A passive capture showed three submitted read-10 requests with
no matching native reply after a successful OFF snapshot. The failed operation
reported uncertainty, invalidated its lease and did not proceed to ON. STOP and
separate original-setting restoration passed. The late-reply fix does not pretend
to fix absent replies: this known reliability limitation is tracked in
[issue #34](https://github.com/nobodypb/labbridge/issues/34).

The firmware audit established why gain/backup, reversible zero, fan/external
telemetry and calibration-preserving reset cannot be mapped on the audited
native revision; see [the protocol matrix](dl24m-bench-extension.md#native-protocol-boundary).
In particular, parser-only 46/36 round-trip evidence was rejected after tracing
both to the live sensor field. Only read 36 is integrated, explicitly as a
signless temperature magnitude, never a cell probe or accuracy guarantee.
Remaining physical acceptance is [issue #35](https://github.com/nobodypb/labbridge/issues/35).
The RD/AP/STA/BLE 60-minute topology in #28 and battery release #14 remain open.


A 600.9 s standalone station/BLE/WebSocket run at a 500 ms subscription interval
inspected 1,218 snapshots: zero reported stream gaps, zero expired/invalid scalar
measurement cases and no boot change. It maintained an explicit lease heartbeat
while OFF and performed 29 passive diagnostic fetches over the same connection.
Fine-temperature magnitudes were individually fresh in 1,036 inspected snapshots
(24.86–25.13 degC); other snapshots do not promote an aged native value to fresh.
No ON was sent. Original settings remained unchanged; cleanup confirmed OFF and
released the lease. Serial observation recorded no panic, stack overflow or
allocation abort; aggregate minimum free heap was 8,342,879 bytes including PSRAM,
worker-stack headroom 4,004 bytes and maximum observed snapshot construction
42 ms. This bounded standalone test does not close the different 60-minute
RD/AP/STA/BLE topology in #28. It used the same driver/stream implementation as
the final image; subsequent changes restore a browser-local CV-notice preference
and separate recording-gap notices from confirmed operation results, with no
transport/driver behavior change.


Final embedded assets and application source `e4cf39b` (StickS3 application
SHA-256 prefix `6ab8d630a31880c9`) passed the complete real-browser repeat after
separating gap notices from operation results. The build version has a `-dirty`
suffix for pending acceptance documentation and the target-specific dependency
lock. No driver/stream source differs from the bounded mode/auth/soak tests.

| Target | Application bytes | App partition | Free |
|---|---:|---:|---:|
| StickS3 | 2,241,520 | 4 MiB | 46% |
| USB-OTG | 2,320,144 | 4 MiB | 44% |
| ESP32 | 2,224,960 | 3 MiB | 29% |

After the final browser run the exact pre-test NVS backup was restored and
hash-verified, preserving the original station/AP configuration and bearer and
removing the temporary test password. A STOP submitted before BLE re-established
reported `connection_changed`/unknown; it was not counted as successful shutdown.
The boot guard, subsequent fresh ready/OFF readback and a separate explicit STOP
established the final state. All 14 original settings, CC 0.100 A, fresh valid
measurements and station LAN connectivity were rechecked. The final authenticated
bearer WebSocket provided four native readings and passive BLE diagnostics in the
same boot. The original password configuration was unset and remains unset;
first UI password setup still uses the setup AP or trusted USB. No credentials,
private logs or configuration backups are published.

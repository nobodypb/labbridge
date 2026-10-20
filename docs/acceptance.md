# Acceptance status

This is a development baseline, not a fully commissioned battery-test system.

## Automated acceptance

- 23 portable C++ behavior executables cover framing, queued transactions,
  connection changes, resource validation, driver identity, leases, idempotency,
  local guards, per-activation deadlines, interlock feedback and temperature.
- Python tests validate actual runtime responses against OpenAPI, generated
  request models, USB chunk framing, distributed lease rollback and error
  propagation. Host tests include both chemistries through complete simulated
  cycles, cleanup and reboot rejection, plus the CLI.
- The frozen baseline's codec/controller and 13 Python tests remain mandatory.
- Three UI policy tests and five browser scenarios cover Wi-Fi scan/select/PSK,
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

Actual RD pairing, setpoint/output readback, fault tests and full coexistence
acceptance remain open until performed. This must not be inferred from simulated
sources. The read-only acceptance runner requires configured TCP sources, BLE
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
subsequent connection succeeded. This is provisioning evidence, not source
output/readback, configured-source STOP or full coexistence acceptance.

The optional target-SSID profile changes only phase one's SSID field;
phase two remains identical. Codec and generated-client tests cover that
invariance. The default remains the PC-compatible profile.

Native ESP USB line-control behavior was verified through symbolic paths;
UART adapters retain their reset wait. Tests use synthetic aliases and USB
metadata. Physical setup details and installed host rules are not published.

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

Final version-two commissioning used the actual embedded assets from source
`2d11b24` (application SHA-256 prefix `c9ca3960940abd7d`), with zero configured
instruments throughout:

| Check | Observed result |
| --- | --- |
| Browser login on `/` | 0.945 s; cookie reload and CSRF logout pass |
| Browser login on `/wifi` | 1.207 s; cookie reload and CSRF logout pass |
| Browser login on `/devices` | 1.055 s; cookie reload and CSRF logout pass |
| Browser login on `/dl24` | 0.987 s; cookie reload and CSRF logout pass |
| Browser login on `/dl24/` | 0.932 s; cookie reload and CSRF logout pass |
| 17 correct API logins | 0.817–0.984 s; all below 3 s |
| 6 incorrect passwords | `401`, maximum 0.868 s; sixth attempt limited with `429` |
| 128 Unicode code points / 512 UTF-8 bytes | Correct password succeeds; altered final character rejected |
| Password change with current password | 1.678 s; previous sessions invalidated |
| Session cap and recovery | Eight valid sessions, ninth rejected, existing cookie retained; USB recovery revokes sessions |
| STOP route during an active password job | HTTP 31.4 ms / USB 41.4 ms |
| ESP reboot | Old session rejected; persisted password logs in again in 0.861 s |

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
contract tests, 31 host tests, the reference suites, 11 Web unit tests and 45
browser scenarios. Generation drift, Black and public-tree checks pass, and
ESP-IDF 5.5.2 builds fit StickS3, USB-OTG and ESP32 with the final frontend and
socket policy. Remaining AP-first-setup and laboratory commissioning limits are
still explicit; the three-second user-login requirement is now satisfied.

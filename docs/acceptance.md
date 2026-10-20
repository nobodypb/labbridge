# Acceptance status

This is a development baseline, not a fully commissioned battery-test system.

## Automated acceptance

- 20 portable C++ behavior executables cover framing, queued transactions,
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

The old NVS namespace and source application are preserved. The rollback image
and private pre-migration backup remain outside the public repository.

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

StickS3 bench smoke acceptance on firmware `0.1.0+026063a`: AP listener starts,
status/invalid-session handling works over USB and authenticated LAN, the updated
UI asset is served, and port 8080 is not reachable through the station address.
The original DL24M settings matched the pre-flash snapshot; CC 0.100 A, fresh
measurements and confirmed output OFF were preserved. A private system/application
backup was taken before flashing. No RD source was paired or actuated in these
checks, and no AP-client/BLE coexistence result is claimed from them.

W01 provisioning investigation on firmware `0.1.0+c29b29d`: direct AP broadcasts
reported 7,413 successful radio-driver completions and zero failed completions,
with no associated AP client. The operator still observed W01 without a server
IP; no incoming RD TCP connection occurred. This is a failed pairing result, not
RD commissioning or coexistence acceptance. An intermittent USB request timeout
was also observed; later status remained available without a confirmed reset.
A protected-network comparison was subsequently performed as recorded below.
Output actuation was not performed.

WPA2 comparison preparation on firmware `0.1.0+4af9dc4`: the USB API rejected a
short passphrase with 422, then accepted an AP-only WPA2 configuration. LAN
status confirmed the security change without a boot-ID change, station address
change or station disconnect. The AP secret was absent from Wi-Fi, provisioning
and full-configuration responses. All original DL24M settings were re-read and
unchanged; CC 0.100 A, output OFF and fresh BLE measurements were preserved.
The protected-network server-address phase subsequently ran its full 120-second
deadline with one associated client and 11,991 accepted UDP submissions. It
timed out; the operator confirmed W01 still displayed Server IP placeholders
without an address. WPA2 alone did not resolve AP-origin provisioning. The AP was
restored explicitly to open through USB and station connectivity was preserved;
DL24M remained OFF / 0.100 A. The operator confirmed the correct WiFi Config
dialog with red Server IP placeholders and the skip button. No RD actuation
occurred. The subsequent PC investigation found concrete packet-profile
mismatches; correct these before another radio comparison.

Offline PC-protocol acceptance: the manufacturer-linked v1.0.0.22 executable
was decompiled and its protected strings resolved without running it. CRC-table
values agree with a separate bitwise implementation for all 256 byte values;
two synthetic packet sets decode to their original logical payloads and satisfy
per-triple CRC and total XOR. The current sender differs in phase-one SSID,
BSSID handling, multicast addressing and scheduling. The older v1.0.0.8 has
matching visible framing/control flow, but its encrypted strings were not
resolved. Public reference vectors contain synthetic inputs only. These checks
validate the documented PC sender profile, not ESP RF delivery or W01 acceptance.
No additional flashing or pairing attempt occurred during this investigation.

## DL24M Bench (LB-020)

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

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
or current measurements or validate CC under load, hardware OVP/OCP, external
NTC, presets, counters, battery wiring or a battery workflow.

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

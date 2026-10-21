# Uncertainties and evidence boundaries

The hardware handbook describes confirmed wire behavior and labels probable
mappings. This file records the remaining decisions, not a complete chronology.

- The actual load displays firmware 2.0. The manufacturer supplied 2.1/2.2
  images, with the same numeric parser family. A genuine 2.0 binary was not
  obtained. Cross-flashing modular boards is unverified; no load update occurred.
- Firmware decompilation and PC v2.0.8 analysis support the numeric commands.
  Real BLE tests confirmed reads and reversible settings, including OVP,
  duration, separate mode banks and language. PC method names are not always
  reliable: command 50 corresponds to a battery temperature threshold.
- Zero-pause writes were mostly successful but intermittently failed even
  after ATT acknowledgement. The cause may include internal device state.
  Parsing alone does not guarantee execution. Bounded readback/retry is required.
  The new 60/200-ms policy passed the recorded bench checks. Foreground/background
  overlap was also a concrete cause: the new queue waits for the application
  reply rather than treating an ATT ACK as completion. Remaining sporadic
  failures are recovered by bounded readback/retry; their device-internal cause
  is not established.
- FF55 counter/energy units and numeric counter scaling need calibration against
  host integration. Do not use guessed scaling for battery capacity claims.
- Ordinary RD6018 operating telemetry, including external NTC and register 17
  regulation state, is implemented and its no-load transport is physically
  commissioned. Loaded CC/CV transitions, RD6024 hardware and optional
  Bluetooth-module transport remain uncommissioned. Portable codec tests alone
  are not PSU hardware evidence.
- Reference GPIO switching and DS18B20 scratchpad decoding have portable tests;
  actual wiring, power-up behavior, contact feedback and probe identity require
  commissioning. No battery run has been validated on the physical bench.
- USB occasionally required reopening in the original diagnostic tool. Reads
  may be retried by a host after reconnect; uncertain writes must be resolved by
  operation ID/readback, not blind replay.

Manufacturer downloads and private captures are deliberately excluded from this
public repository. They retain their original licensing and may contain private
addresses. Sanitized synthetic wire fixtures are sufficient for regression tests.


The new INA228 and Yocto-Watt providers have portable/SDK-contract coverage but
no actual sensor hardware evidence. INA228 sampling is not simultaneous with
instrument telemetry; cross-device timestamps do not imply hardware triggering.
The native Yocto SDK handles its USB/calibration protocol; the ESP HTTP adapter
supports uncalibrated/default user-calibration parameters only. Native Yocto USB
on an ESP USB-host port is a separate, unimplemented transport. Direct-output
routing relies on the commissioned off-state/reverse-current behavior of the
selected source and load. It is not a claim of galvanic isolation.

The deterministic simulator models the selected meter polarity conventions from
bindings. It cannot verify actual wiring polarity; commission that on hardware
using the preserved raw values. INA228 accumulation is epoch-relative from the
first validated sample after initialization, avoiding historical counts under a
different calibration without clearing hardware counters.


Ordinary RD6018 register scales/model selection are characterized in portable
tests. W01 pairing, TCP identity/readback, voltage/current targets, output
switching and the complete operating telemetry have physical no-load acceptance
through the bridge's USB and HTTP interfaces. RD6018P is explicitly excluded.
These results do not commission a second source, loaded CC/CV behavior or every
module revision; see [acceptance](acceptance.md).

The W01 module showed no server IP in open-AP UDP trials, a direct AP broadcast
trial, or a WPA2 AP-origin trial with an associated phone. The direct trial had
7,413 driver transmission completions and zero failed completions; the protected
trial ran 120 seconds with 11,991 accepted UDP submissions. Neither is a receiver
acknowledgement. No RD TCP connection was observed. The operator confirmed the
correct dialog: W01, red Server IP placeholders, and the skip button. The setup
AP was restored to open; DL24M remained OFF / CC 0.100 A.

Espressif documents that SmartConfig does not support open networks
([FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/wifi.html#connect-what-are-the-specifications-of-wi-fi-parameters-when-using-smartconfig)).
That limitation alone does not explain the failed protected trial or establish
what RIDEN's special server-address phase requires. The PC-profile
mismatches below have now been corrected and one protected AP-origin pairing
succeeded, including the displayed address, UDP result and TCP identity. This
disproves AP-origin impossibility for this W01 setup, but does not isolate which
profile differences were essential. No evidence currently proves that a second ESP is required for
pairing. A conventional station-origin comparison remains useful if the exact
profile still fails.

A 60-minute AP+STA+DL24M BLE soak and deliberate connection-loss/STOP checks are
required. Espressif marks associated SoftAP plus BLE coexistence as potentially
unstable. A source-only second ESP with a distinct SSID is the configured fallback,
not an automatic in-run failover. The second PSU's future Wi-Fi module cannot be
counted as physically accepted using a simulated connection.

## RIDEN PC software protocol analysis

On 2026-10-20 the RD6018 manufacturer-linked download folder offered
`RidenPowerSupply_V1.0.0.22`. The Windows executable is a managed .NET Framework
WPF application. ILSpy CLI 9.1.0.7988 recovered the relevant methods despite
obfuscated identifiers. Encrypted string resources were decoded offline;
**the vendor executable was not run**. This establishes what the PC sender
constructs, not the receiver's complete acceptance criteria or actual RF timing.
The [handbook](handbook.md#riden-pc-provisioning-wire-profile) contains the usable
protocol specification without depending on private decompiled source.

Provenance (manufacturer binaries and decompiled sources remain ignored):

| Artifact | SHA-256 |
| --- | --- |
| [Current archive, Drive file `1_-TpD8F0-evotz0zv6EXHWDc5Wha8YCS`](https://drive.google.com/file/d/1_-TpD8F0-evotz0zv6EXHWDc5Wha8YCS/view) | `986dfa3259ec5532a3adae09d22bd139a708f06eb80f73c56d675190d3284cc7` |
| Extracted v1.0.0.22 executable, 17,682,432 bytes | `efe21eb1efa5cb7fb7ebf0767a2d316ec7654d37423ee240ffbf8734de8c59ee` |
| [2020 mirror archive, MediaFire key `xeky6spndqk3p3s`](https://www.mediafire.com/folder/xc2v831oqj98g/RD6018) | `2197bb2f8fe0e4e15f17f293f51a54940e13ab30a4f7f24271c54d3c31e1d815` |
| Extracted v1.0.0.8 executable, 17,319,424 bytes | `313250ca31ba49b267b4e193423478ba0fd7c53aaff4cced3a98d4d567250c17` |

Reproduction anchors in `RidenPowerSupply.Window.MainWindow` of the current
executable (metadata token / RVA):

| Responsibility | Method | Token / RVA |
| --- | --- | --- |
| Phase-one fields and repeat loop | `SendIpUdpData` | `0600020A / 0001A418` |
| Phase-two fields and repeat loop | `SendWifiUdpData` | `0600020B / 0001A560` |
| Header, guides, logical payload | `vmPVZJFhmH` | `06000215 / 0001AE18` |
| Destination counter and UDP send | `LsdVKCt76I` | `06000216 / 0001B2D8` |
| CRC/nibble-to-length triple | `MhYVPnfItR` | `06000217 / 0001B3F4` |
| Eleven-byte UDP result | `OnReceiveCompleted`, `dUsyRhg9gh` | `060002B8 / 00028FD8`, `060002B9 / 00029084` |
| TCP listener and acceptance | `F1FV65lApr`, `ClientAccepted` | `06000210 / 0001AA6C`, `06000211 / 0001AAEC` |

The older v1.0.0.8 executable has the same visible two-stage control flow,
39 guide groups, length-triple formula, group-counter range, UDP port 7001 and
one-second inter-burst sleep. Its encrypted string constants have not been
resolved in this comparison; exact equality of its SSID, fixed byte and address
prefix is therefore not claimed. The byte-exact specification and fixtures are
for v1.0.0.22.

### Differences from the previous bridge sender

| Item | LabBridge firmware the development build | RIDEN PC v1.0.0.22 |
| --- | --- | --- |
| Phase-one SSID field | Actual AP SSID | Literal `RD6006` |
| Logical byte 3 | Actual BSSID CRC | Fixed `0xC7` |
| Extra BSSID bytes | Six bytes interleaved | None |
| UDP destination | AP broadcast | Rotating `234.n.n.n:7001` |
| Scheduling | Repeated 2-second guides / 4-second data, requested 8-ms packet gap | 39 guide groups + complete data payload, then 1-second sleep; no explicit packet gap |
| Join result | Display/identity workflow; no UDP result receiver | Eleven-byte UDP result on port 18266 |

CRC-8/MAXIM, nibble packing, length offset 40 and the guide length values agree.
All 256 PC CRC-table entries were checked against the reflected-polynomial
calculation. Synthetic phase-one and phase-two vectors were independently decoded
back to their input bytes with CRC and total-XOR checks. No private credentials
are in those fixtures. This is offline consistency evidence, not a W01 pairing
result. The previous general-profile tests remain valid for that profile but
cannot establish compatibility with RIDEN's PC profile.

The separate PC-profile codec/scheduler was implemented with fixture-based TDD: the old general codec failed the payload-length assertion before the PC codec passed. Generated-client tests also failed on absent burst/join counters before API regeneration passed. The new ESP sender uses multicast UDP in both AP modes. No fixed packet delay is imposed; transient queue backpressure retries the same packet and has a one-second bound. Preserve bounded cancellation, deadlines and
STOP responsiveness: the PC has weak ACK validation, OS-dependent adapter
selection and potentially overlapping workers; those are not requirements to
copy. For hardware acceptance, verify displayed phase-one IP first,
then join ACK, TCP identity, reconnect/STOP behavior and AP+STA+BLE coexistence.
The source output must remain OFF during commissioning. No radio trial or firmware flash was performed as part of the original PC
investigation; subsequent hardware results are recorded in acceptance.md.

The optional `pc_v22_target_ssid` profile changes only the phase-one SSID
field. It is not a commissioned alternative to `pc_v22`. Use a fresh setup
state and session; confirm the displayed server IP and advance before the
management deadline. A retained candidate listing does not prove a new join
or identity exchange.

Target-SSID trials displayed the server IP but did not establish new join/TCP
evidence. A late transition affected one trial; a same-session repeat had the
same outcome. Different transition delays and receiver-state uncertainty
prevent a universal conclusion about the SSID field. Retain the full-PC default.

Discovery-only sockets previously remained idle after one identity request.
A silent peer restart could leave a stale serial claim and block replacements.
Read-only identity probes with a bounded reply deadline address that lifetime
while preserving live-duplicate rejection and configured-device recovery.
A physical source power cycle verified discovery release/reconnection without
restarting the bridge; see acceptance.md. Configured-source STOP/reconnect and
full coexistence acceptance remain open.

## UI password commissioning (LB-020)

Portable access-policy and browser tests cover AP/LAN isolation, password login,
CSRF, expiry, logout, recovery and nonblocking authorization during hashing.
StickS3 commissioning now establishes actual LAN login/scan, a wireless AP scan
through an additional ESP32, and HTTP/USB STOP with confirmed OFF during real
600,000-iteration password jobs. The discovered dual-stack socket classification
failure was reproduced, corrected and covered by a shared transport test.

The original version-one verifier took approximately 114–116 seconds for setup,
login and USB recovery on a 160 MHz StickS3. Version two uses 6,000 rounds under
the operator-approved basic-protection policy. Seventeen API logins measured
0.841–0.955 s and all five real browser entry points stayed below 1.021 s on an
isolated StickS3. Eight sessions retain valid cookies when idle HTTP connections
are reclaimed. The original full flash was restored and device-verified. STOP
acceptance remained below 100 ms in the earlier physical run, with the load
already OFF. The final isolated run used an unconfigured synthetic device ID:
HTTP/USB dispatch responded in 27.7/21.9 ms while hashing, without connecting an
instrument. Shutdown from ON under hashing was not exercised. One earlier real
browser scan wait timed out without a captured cause; an instrumented repeat
passed. Repetition and coexistence soak evidence remains open.

Further hardware work uses only the dedicated test bridge, without laboratory
connections while other tests continue. AP first-
password setup and final live-device verification after NVS restoration remain
open. Original DL24M-stick NVS and the additional test ESP32's overwritten flash
were restored and hash-verified; temporary passwords and raw backups/logs remain
private. Absolute twelve-hour expiry remains covered by deterministic clock tests.
Session capacity, reboot revocation and HTTP old-password replacement now have
isolated hardware evidence; full Bench actuation with cookie login remains
uncommissioned because laboratory connections are paused.

HTTPS/mTLS remains a separate planned story (LB-021); HTTP credentials and
cookies are currently unencrypted. mTLS is intended as optional alternative
authentication for both browser and API clients. Certificate provisioning,
trust anchors, renewal and revocation still need their own acceptance evidence.

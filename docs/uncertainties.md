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
  The new 60/200-ms policy must be measured against the preserved implementation.
- FF55 counter/energy units and numeric counter scaling need calibration against
  host integration. Do not use guessed scaling for battery capacity claims.
- RD6024 CC/CV register 17 is probable; external NTC mapping and Bluetooth-module
  transport remain uncommissioned. USB codec tests are not PSU hardware tests.
- Reference GPIO switching and DS18B20 scratchpad decoding have portable tests;
  actual wiring, power-up behavior, contact feedback and probe identity require
  commissioning. No battery run has been validated on the physical bench.
- USB occasionally required reopening in the original diagnostic tool. Reads
  may be retried by a host after reconnect; uncertain writes must be resolved by
  operation ID/readback, not blind replay.

Manufacturer downloads and private captures are deliberately excluded from this
public repository. They retain their original licensing and may contain private
addresses. Sanitized synthetic wire fixtures are sufficient for regression tests.

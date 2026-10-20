# Acceptance status

## Portable checks

The migration currently has passing behavior tests for resource allocation,
DL24M framing and interleaved notifications, bounded transactions, local guards,
node leases/idempotency, RD6024 CRC/register decoding, interlock transitions,
DS18B20 CRC, typed SDK transport and distributed-acquisition rollback.
The frozen baseline's existing tests remain part of the check command.

## Hardware baseline

Before migration, the StickS3 and DL24M Ver 2.0 were exercised with a 24 V source
limited to 1 A, DL24M CC 0.100 A, bridge limits 25 V / 0.2 A / 5 W. BLE telemetry,
short load pulses, 22 audited reads and reversible setting changes were observed.
All tested settings were restored and the load output confirmed OFF. The final
baseline firmware remains the rollback image outside this public tree.

## Required before a release

- Build the new ESP adapters for each declared supported board.
- Repeat DL24M bench reads and bounded writes with the new asynchronous runtime;
  record latency, retries, error rate, final OFF and restored settings.
- Verify open-AP Wi-Fi scan, configuration, STA reconnect and USB management.
- Exercise disconnect during ON, stale measurements and lease expiry on hardware.
- Commission RD6024 serial binding and verified CC/CV writes on actual sources.
- Commission interlock feedback and external cell temperature before a battery run.

Uncommissioned modules must remain visibly experimental. No full battery hardware
acceptance is implied by simulated tests or by the old baseline evidence.

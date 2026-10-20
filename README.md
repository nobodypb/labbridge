# LabBridge

A modular ESP32/ESP32-S3 Wi-Fi and USB bridge for laboratory instruments.
One firmware source supports a configured set of devices on each board. External
scripts coordinate named nodes; nodes own transport access, readback, local limits
and shutdown. The first workflow profiles **1S Li-ion and LiFePO4** cells. Sources, sinks and
sensors share a typed component API; physical devices may expose several of each.
A feedback interlock and independent meters are optional workflow bindings.

The DL24M Ver 2.0 integration is bench-tested on a StickS3. RD6018/RD6024 (USB/UART and original-module Wi-Fi), reference GPIO
interlocks, DS18B20, INA228 and Yocto-Watt-V2-C have implementation and automated
coverage, but await
physical commissioning. No complete battery run has been validated on hardware.
See [acceptance](docs/acceptance.md) for the exact boundary.

- [Hardware, API and protocol handbook](docs/handbook.md)
- [Architecture and module extension](docs/architecture.md)
- [Development and board builds](docs/development.md)
- [User stories](docs/backlog.md) and [remaining uncertainties](docs/uncertainties.md)
- [OpenAPI contract](api/openapi.json) and [contributor instructions](AGENTS.md)

## Quick start

Python 3.13; install the pinned environment and both local packages:

```sh
uv venv --python /usr/bin/python3.13 .venv
uv pip install --python .venv/bin/python --require-hashes -r tools/requirements.lock
uv pip install --python .venv/bin/python --no-build-isolation --no-deps -e host/generated -e host
. .venv/bin/activate
labbridge --endpoint serial:/dev/ttyACM0 devices
labbridge --endpoint serial:/dev/ttyACM0 operations
labbridge --endpoint serial:/dev/ttyACM0 stop load
```

USB uses the same typed operations as HTTP. `pair` returns the bearer token over
physical USB; keep it private and supply it through an environment variable for
HTTP. `call module.generated_method --args-file arguments.json` invokes generated
operations, waits for confirmation and emits JSON. Exit codes: 0 confirmed,
1 operation/transport failure, 2 invalid usage. Mutations require leases and
idempotency keys; the [SDK example](docs/handbook.md#host-sdk) manages these.

Try a complete workflow without hardware:

```sh
labbridge battery --simulate --profile examples/battery/li-ion.json \
  --bindings examples/battery/bindings.json --output results/li-ion-demo
labbridge battery --simulate --profile examples/battery/lifepo4.json \
  --bindings examples/battery/bindings.json --output results/lifepo4-demo
```

Each run creates `samples.csv` and `run.json` in a new directory. Simulation
exercises generated clients and workflow cleanup; it is not a battery model or
hardware acceptance. For physical runs, configure and commission the complete
circuit described in the handbook, then provide `--nodes examples/nodes.json`
and omit `--simulate`. Example profiles require adjustment to the actual cell.

## Firmware

Use ESP-IDF **5.5.2**, Java 21 and Node 20. With the development environment above:

```sh
tools/generate.sh
tools/check.sh
. "$IDF_PATH/export.sh"
tools/build_firmware.sh sticks3   # or usb-otg, esp32
```

Firmware artifacts are under `.build/<board>/`. Use `idf.py` from the matching
ESP-IDF build to flash. Preserve NVS when updating. The S3 partition layout uses
an 8 MB flash and 4 MB application partition; use the board's generated flash
arguments rather than an unrelated application's partition table.

Connect to the open **LabBridge-Setup** AP and open `http://192.168.4.1/wifi`
for scan, network selection and PSK entry. `/devices` is the separate laboratory
UI. Runtime configuration is available over USB or authenticated HTTP, with
revision checking and board resource validation.

MIT licensed. `reference/` preserves the earlier implementation for regression
checks. No manufacturer binaries, private captures or credentials are published.


## Optional measurement providers

INA228 connects to explicitly approved ESP I²C pins with a configured address,
shunt resistance and current range. Several addresses can share one bus. Native
Yocto-Watt USB on the PC uses the official optional library:

```sh
uv pip install --python .venv/bin/python --no-build-isolation -e 'host[yocto]'
labbridge --endpoint yocto+usb://YWATMK2C-123456 devices
labbridge --endpoint yocto+usb://YWATMK2C-123456 call components.get_sensor_readings \
  --args '{"device_id":"meter"}'
```

The ESP can also read Yocto-Watt through VirtualHub/YoctoHub HTTP; native Yocto USB
on the ESP is not implemented. Both provider paths use the common sensor API.
See [the handbook](docs/handbook.md#device-components-the-common-integration-api)
and the direct INA228/Yocto binding examples for orientation, optional extra
sensors and the distinction between OFF and electrical isolation.

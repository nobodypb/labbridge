# LabBridge

A modular ESP32/ESP32-S3 Wi-Fi and USB bridge for laboratory instruments.
External scripts coordinate one or several nodes; each node owns its configured
devices and enforces local limits. The first application is profiling 1S Li-ion
and LiFePO4 cells through charge, discharge, rest and pulse steps.

**Migration in progress.** The preserved DL24M application is under `reference/`.
The new core, contract and modules are being developed with behavior-first tests.
Hardware support is not inferred from simulator success. Current milestones and
acceptance criteria are in [the backlog](docs/backlog.md).

- [User and protocol handbook](docs/handbook.md)
- [Architecture and extension contracts](docs/architecture.md)
- [Uncertainties and evidence](docs/uncertainties.md)
- [Development contract](AGENTS.md)

License: MIT. Manufacturer downloads and private bench artifacts are not part of
this repository.

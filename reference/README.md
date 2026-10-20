# Frozen migration reference

This is the last locally verified application source before the modular migration.
It contains the known DL24M 9-byte codec, stream resynchronization, guarded setting
writes, persistent OFF/rollback journal and hardware-specific transports.

The source is retained for characterization and comparison, not as the new
architecture or as evidence of a fully validated live automation session.
No manufacturer firmware, calibration backup, credentials or private captures
are included. Unsupported six-byte paths remain here only as historical code;
they must not become an automatic fallback in the new driver.

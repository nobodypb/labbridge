"""Durable USB aliases retain native CDC/JTAG line-control semantics."""

from types import SimpleNamespace
import unittest
from unittest.mock import patch
from labbridge.transport import SerialTransport


class SerialPathTests(unittest.TestCase):
    def check_alias(self, alias, physical, vid, expected_native):
        with (
            patch("serial.Serial") as factory,
            patch(
                "serial.tools.list_ports.comports",
                return_value=[SimpleNamespace(device=physical, vid=vid)],
            ),
            patch(
                "os.path.realpath",
                side_effect=lambda p, **k: physical if p == alias else p,
            ),
            patch("labbridge.transport.time.sleep") as sleep,
        ):
            transport = SerialTransport(alias)
            port = factory.return_value
            self.assertEqual(port.port, alias)
            self.assertEqual(port.dtr, expected_native)
            self.assertFalse(port.rts)
            port.open.assert_called_once()
            if expected_native:
                sleep.assert_not_called()
            else:
                sleep.assert_called_once_with(2)
            transport.close()
            port.close.assert_called_once()

    def test_udev_role_alias_identifies_native_esp(self):
        self.check_alias("/dev/example-node-a", "/dev/ttyACM7", 0x303A, True)

    def test_by_id_alias_identifies_native_esp(self):
        self.check_alias(
            "/dev/serial/by-id/synthetic-esp", "/dev/ttyACM7", 0x303A, True
        )

    def test_uart_alias_preserves_adapter_reset_wait(self):
        self.check_alias("/dev/example-node-b", "/dev/ttyUSB7", 0x1A86, False)

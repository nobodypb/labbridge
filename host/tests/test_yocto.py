import unittest
from labbridge.sdk import Node


class NativeUsbTests(unittest.TestCase):
    def test_native_usb_uses_common_components_and_preserves_signed_si_values(self):
        from labbridge.yocto import YoctoUsbTransport

        class Backend:
            uptime = 1000
            fail = False

            def read(self):
                if self.fail:
                    raise OSError("unplugged")
                return {
                    "serial": "YWATMK2C-123456",
                    "uptime_ms": self.uptime,
                    "values": {"voltage": 4.0, "current": -0.1, "power": -0.4},
                }

            def close(self):
                pass

        backend = Backend()
        transport = YoctoUsbTransport("YWATMK2C-123456", backend=backend)
        with Node("yocto+usb://YWATMK2C-123456", rest=transport) as n:
            info = n.core.get_node()
            sample = n.components.get_component_reading("meter", "current")
            self.assertEqual(
                (sample.value, sample.unit, sample.quality), (-0.1, "A", "valid")
            )
            backend.fail = True
            self.assertEqual(
                n.components.get_component_reading("meter", "current").quality,
                "unavailable",
            )
            backend.fail = False
            backend.uptime = 10
            self.assertNotEqual(n.core.get_node().boot_id, info.boot_id)
            with self.assertRaises(Exception):
                n.components.set_component_current(
                    "meter", "current", "lease", "key", {"current_a": 0.1}
                )

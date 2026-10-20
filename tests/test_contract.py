import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ContractTests(unittest.TestCase):
    def test_modular_typed_operations(self):
        spec = json.loads((ROOT / "api/openapi.json").read_text())
        self.assertEqual(spec["openapi"], "3.0.3")
        ids = []
        for path, item in spec["paths"].items():
            for method, op in item.items():
                if method not in {"get", "put", "post", "delete"}:
                    continue
                ids.append(op["operationId"])
                if method in {"put", "post"}:
                    body = op["requestBody"]["content"]["application/json"]["schema"]
                    self.assertIn("$ref", body, path)
                self.assertIn("default", op["responses"])
        self.assertEqual(len(ids), len(set(ids)))
        self.assertIn("setLoadCurrent", ids)
        self.assertIn("setSourceVoltage", ids)
        self.assertIn("setSwitchPath", ids)
        self.assertIn("getTemperatureChannels", ids)
        self.assertNotIn("/api/v1/devices/load", spec["paths"])
        for driver in ("dl24m", "rd60xx", "gpio_interlock", "ds18b20"):
            self.assertTrue((ROOT / f"api/modules/{driver}.json").is_file())


if __name__ == "__main__":
    unittest.main()

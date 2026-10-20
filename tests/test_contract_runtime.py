import json, pathlib, subprocess, unittest
from jsonschema import Draft4Validator, RefResolver

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RuntimeContractTests(unittest.TestCase):
    def test_actual_driver_responses_conform_to_openapi(self):
        spec = json.loads((ROOT / "api/openapi.json").read_text())
        resolver = RefResolver.from_schema(spec)
        output = subprocess.check_output(
            [ROOT / ".build/host/contract_dump"], text=True
        )
        fixtures = [json.loads(line) for line in output.splitlines()]
        self.assertGreaterEqual(len(fixtures), 10)
        for fixture in fixtures:
            op = next(
                v
                for item in spec["paths"].values()
                for k, v in item.items()
                if k in ("get", "put", "post")
                and v["operationId"] == fixture["operation"]
            )
            schema = op["responses"][str(fixture["status"])]["content"][
                "application/json"
            ]["schema"]
            with self.subTest(operation=fixture["operation"]):
                Draft4Validator(schema, resolver=resolver).validate(fixture["body"])

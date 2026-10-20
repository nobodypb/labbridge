"""Public documentation must not consume the ESP request-validation budget."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
MODULE = importlib.util.spec_from_file_location(
    "assemble_api", ROOT / "tools/assemble_api.py"
)
assembler = importlib.util.module_from_spec(MODULE)
MODULE.loader.exec_module(assembler)


class AnnotationTests(unittest.TestCase):
    def test_public_docs_preserved_without_changing_firmware_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "api/modules").mkdir(parents=True)
            spec = {
                "paths": {
                    "/example": {
                        "put": {
                            "operationId": "example",
                            "description": "Public operation documentation.",
                            "requestBody": {
                                "content": {
                                    "application/json": {
                                        "schema": {
                                            "$ref": "#/components/schemas/Request"
                                        }
                                    }
                                }
                            },
                        }
                    }
                },
                "components": {
                    "schemas": {
                        "Request": {
                            "type": "object",
                            "description": "Public request documentation.",
                            "required": ["description"],
                            "additionalProperties": False,
                            "properties": {
                                "description": {
                                    "type": "string",
                                    "minLength": 3,
                                    "description": "A property can itself be named description.",
                                    "example": "operator note",
                                }
                            },
                        }
                    }
                },
            }
            (root / "api/core.json").write_text(json.dumps(spec))
            with patch.object(assembler, "ROOT", root):
                artifacts = assembler.assemble()
            public = json.loads(artifacts[root / "api/openapi.json"])
            self.assertEqual(public["components"], spec["components"])
            header = artifacts[root / "firmware/generated/routes.hpp"]
            constraints = json.loads(header.split('R"SCHEMA(')[1].split(')SCHEMA"')[0])
            self.assertEqual(
                constraints["schemas"]["Request"],
                {
                    "type": "object",
                    "required": ["description"],
                    "additionalProperties": False,
                    "properties": {"description": {"type": "string", "minLength": 3}},
                },
            )


if __name__ == "__main__":
    unittest.main()

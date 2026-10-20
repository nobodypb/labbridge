import json, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
from labbridge.cli import nodes_from


class CliTests(unittest.TestCase):
    def test_only_selected_node_connections_are_opened(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nodes.json"
            path.write_text(
                json.dumps(
                    {
                        "nodes": {
                            "unused": {"endpoint": "serial:/offline"},
                            "selected": {"endpoint": "http://selected"},
                        }
                    }
                )
            )
            with patch("labbridge.cli.Node") as factory:
                nodes = nodes_from(path, ["selected"])
                self.assertEqual(list(nodes), ["selected"])
                factory.assert_called_once_with("http://selected", "")

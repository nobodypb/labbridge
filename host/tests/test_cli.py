import json, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
from labbridge.cli import nodes_from


class CliTests(unittest.TestCase):
    def test_ui_password_recovers_over_usb_without_echoing_secret(self):
        from unittest.mock import MagicMock
        from labbridge.cli import main

        node = MagicMock()
        node.client.configuration.host = "http://usb"
        from labbridge.transport import SerialTransport

        node.client.rest_client = MagicMock(spec=SerialTransport)
        with patch("labbridge.cli.Node", return_value=node), patch(
            "labbridge.cli.getpass", side_effect=[" synthetic password "] * 2
        ), patch("labbridge.cli.emit") as emit:
            self.assertEqual(
                main(["--endpoint", "serial:/synthetic", "ui-password"]), 0
            )
        request = node.core.change_ui_password.call_args.kwargs["ui_password_change"]
        self.assertEqual(request.password.get_secret_value(), " synthetic password ")
        self.assertEqual(
            request.confirm_password.get_secret_value(), " synthetic password "
        )
        self.assertNotIn("synthetic password", str(emit.call_args))

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

    def test_usb_pair_emits_the_intended_plain_token_from_generated_secret_model(self):
        from unittest.mock import MagicMock
        from labbridge.cli import main
        from labbridge_generated.models.management_token import ManagementToken

        node = MagicMock()
        node.client.configuration.host = "http://usb"
        node.core.get_management_token.return_value = ManagementToken(
            token="synthetic-test-token"
        )
        with patch("labbridge.cli.Node", return_value=node), patch(
            "labbridge.cli.emit"
        ) as emit:
            self.assertEqual(main(["--endpoint", "serial:/synthetic", "pair"]), 0)
        self.assertEqual(emit.call_args.args[0], {"token": "synthetic-test-token"})

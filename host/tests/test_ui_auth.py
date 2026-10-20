import unittest
from unittest.mock import MagicMock, patch
from labbridge.cli import main
from labbridge_generated.api_client import ApiClient
from labbridge_generated.models.ui_password_change import UiPasswordChange


class UiAuthTests(unittest.TestCase):
    def test_reserved_http_host_is_not_a_physical_usb_transport(self):
        with patch("labbridge.cli.getpass") as prompt, patch("sys.stderr"):
            with self.assertRaises(SystemExit):
                main(["--endpoint", "http://usb", "ui-password"])
        prompt.assert_not_called()

    def test_password_models_redact_but_transport_serialization_preserves_value(self):
        secret = " spaces and ä preserved "
        request = UiPasswordChange(password=secret, confirm_password=secret)
        self.assertNotIn(secret, str(request))
        self.assertNotIn(secret, repr(request))
        self.assertNotIn(secret, request.to_str())
        wire = ApiClient().sanitize_for_serialization(request)
        self.assertEqual(wire, {"password": secret, "confirm_password": secret})
        with self.assertRaises(ValueError) as error:
            UiPasswordChange(password="short_secret", confirm_password="x")
        self.assertNotIn("short_secret", str(error.exception))

    def test_cli_requires_usb_before_prompting(self):
        node = MagicMock()
        node.client.configuration.host = "http://node.local"
        with patch("labbridge.cli.Node", return_value=node), patch(
            "labbridge.cli.getpass"
        ) as prompt, patch("sys.stderr"):
            with self.assertRaises(SystemExit) as error:
                main(["--endpoint", "http://node.local", "ui-password"])
        self.assertEqual(error.exception.code, 2)
        prompt.assert_not_called()
        node.core.change_ui_password.assert_not_called()

    def test_cli_mismatch_does_not_write_or_echo_either_password(self):
        node = MagicMock()
        node.client.configuration.host = "http://usb"
        from labbridge.transport import SerialTransport

        node.client.rest_client = MagicMock(spec=SerialTransport)
        with patch("labbridge.cli.Node", return_value=node), patch(
            "labbridge.cli.getpass",
            side_effect=["first synthetic password", "second synthetic password"],
        ), patch("labbridge.cli.emit") as emit:
            self.assertEqual(
                main(["--endpoint", "serial:/synthetic", "ui-password"]), 2
            )
        node.core.change_ui_password.assert_not_called()
        self.assertNotIn("synthetic", str(emit.call_args))

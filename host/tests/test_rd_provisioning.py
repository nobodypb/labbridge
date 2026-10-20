"""Generated provisioning operations use the ordinary CLI and USB transport."""

import json
import unittest
from unittest.mock import patch
from labbridge import cli
from labbridge.sdk import Node


class RecordingRest:
    def __init__(self):
        self.requests = []

    def request(self, method, url, headers=None, body=None, **kwargs):
        self.requests.append((method, url, body))
        value = dict(
            state="server_address",
            session_id="boot-p1",
            deadline_ms=120000,
            ap_ssid="LabBridge-Setup",
            ap_address="192.168.4.1",
            tcp_port=8080,
            listener_running=True,
            ap_clients=1,
            ap_channel=6,
            candidates=[],
            metrics=dict(
                accepted=0,
                disconnects=0,
                rejected=0,
                rx_bytes=0,
                tx_bytes=0,
                provision_packets=0,
                provision_bursts=2,
                provision_join_acks=1,
                provision_tx_completed=4,
                provision_tx_failed=1,
            ),
        )
        from urllib3.response import HTTPResponse

        return HTTPResponse(
            body=json.dumps(value).encode(),
            status=200,
            headers={"Content-Type": "application/json"},
            preload_content=False,
        )


class ProvisioningTests(unittest.TestCase):
    def test_status_preserves_radio_diagnostics_for_automation(self):
        with Node("serial:/synthetic", rest=RecordingRest()) as node:
            status = node.rd60xx.get_rd_provisioning()
            self.assertEqual(status.ap_clients, 1)
            self.assertEqual(status.ap_channel, 6)
            self.assertEqual(status.metrics.provision_tx_completed, 4)
            self.assertEqual(status.metrics.provision_tx_failed, 1)
            self.assertEqual(status.metrics.provision_bursts, 2)
            self.assertEqual(status.metrics.provision_join_acks, 1)

    def test_cli_uses_generated_provisioning_requests_without_operation_polling(self):
        rest = RecordingRest()
        node = Node("serial:/synthetic", rest=rest)
        with patch("labbridge.cli.Node", return_value=node), patch(
            "labbridge.cli.emit"
        ) as emit:
            self.assertEqual(
                cli.main(
                    [
                        "--endpoint",
                        "serial:/synthetic",
                        "call",
                        "rd60xx.start_rd_provisioning",
                        "--args",
                        '{"rd_provision_start":{"confirm_outputs_off":true}}',
                    ]
                ),
                0,
            )
            self.assertEqual(emit.call_args.args[0].state, "server_address")
            self.assertEqual(
                cli.main(
                    [
                        "--endpoint",
                        "serial:/synthetic",
                        "call",
                        "rd60xx.advance_rd_provisioning",
                        "--args",
                        '{"rd_provision_advance":{"session_id":"boot-p1","expected_phase":"server_address"}}',
                    ]
                ),
                0,
            )
        self.assertEqual(len(rest.requests), 2)
        self.assertEqual(rest.requests[0][0], "POST")
        self.assertTrue(rest.requests[0][1].endswith("/api/v1/rd60xx/provisioning"))
        self.assertEqual(
            rest.requests[1][2],
            {"session_id": "boot-p1", "expected_phase": "server_address"},
        )
        node.close()

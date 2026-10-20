"""AP security can be changed/restored without touching station credentials."""

import json
import unittest

from labbridge.sdk import Node
from urllib3.response import HTTPResponse


class Rest:
    def __init__(self):
        self.requests = []

    def request(self, method, url, headers=None, body=None, **kwargs):
        self.requests.append((method, url, body))
        return HTTPResponse(
            body=json.dumps(
                {"state": "connected", "hostname": "fixture.local", "ap_secured": True}
            ).encode(),
            status=200,
            headers={"Content-Type": "application/json"},
            preload_content=False,
        )


class AccessPointTests(unittest.TestCase):
    def test_generated_operation_changes_only_ap_secret_and_can_restore_open(self):
        rest = Rest()
        with Node("serial:/synthetic", rest=rest) as node:
            state = node.core.configure_access_point(
                access_point_request={"psk": "synthetic-fixture"}
            )
            self.assertTrue(state.ap_secured)
            self.assertNotIn("psk", state.to_dict())
            node.core.configure_access_point(access_point_request={"psk": ""})
        self.assertEqual(rest.requests[0][0], "PUT")
        self.assertTrue(rest.requests[0][1].endswith("/api/v1/wifi/ap"))
        self.assertEqual(rest.requests[0][2], {"psk": "synthetic-fixture"})
        self.assertEqual(rest.requests[1][2], {"psk": ""})

#!/usr/bin/env python3
"""Send RIDEN server address and Wi-Fi credentials without intermediate prompts.

Run with PYTHONPATH=host/src:host/generated. Use an exclusive bridge endpoint;
HTTP reads its bearer token from LABBRIDGE_TOKEN. Credentials stay on the bridge.
"""

import argparse
import json
import os

from labbridge import Node
from labbridge.provisioning import pair_rd_wifi


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--confirm-outputs-off", action="store_true", required=True)
    args = parser.parse_args()
    try:
        with Node(args.endpoint, token=os.environ.get("LABBRIDGE_TOKEN", "")) as node:
            status = pair_rd_wifi(node, confirm_outputs_off=args.confirm_outputs_off)
        print(json.dumps({"state": status["state"]}))
        return 0
    except Exception as error:
        print(json.dumps({"state": "failed", "error": type(error).__name__}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

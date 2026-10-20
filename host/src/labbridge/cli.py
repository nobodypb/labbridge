"""JSON-first CLI; exit 0 confirmed, 1 operation/transport error, 2 usage error."""

import argparse
import dataclasses
import json
import os
import sys
from pathlib import Path
from labbridge_generated.models.operation import Operation
from .sdk import Node, Cluster


def encode(value):
    if hasattr(value, "to_dict"):
        return value.to_dict()
    if dataclasses.is_dataclass(value):
        return dataclasses.asdict(value)
    raise TypeError(type(value).__name__)


def emit(value, stream=sys.stdout):
    print(json.dumps(value, default=encode, ensure_ascii=False), file=stream)


def nodes_from(path, selected=None):
    configs = json.loads(Path(path).read_text())["nodes"]
    selected = set(selected if selected is not None else [next(iter(configs))])
    missing = selected - set(configs)
    if missing:
        raise ValueError("Unknown node names: " + ", ".join(sorted(missing)))
    return {
        name: Node(
            c["endpoint"], os.environ.get(c.get("token_env", "LABBRIDGE_TOKEN"), "")
        )
        for name, c in configs.items()
        if name in selected
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--endpoint",
        default="http://labbridge.local",
        help="http://host, serial:/dev/ttyACM0 or yocto+usb://hardware-serial",
    )
    parser.add_argument("--nodes", help="Named-node configuration JSON")
    parser.add_argument("--node", help="Node name from --nodes")
    parser.add_argument("--token-env", default="LABBRIDGE_TOKEN")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("devices")
    sub.add_parser("operations", help="List available generated operations")
    sub.add_parser("pair", help="Read management token over physical USB")
    stop = sub.add_parser("stop")
    stop.add_argument("device")
    call = sub.add_parser(
        "call", help="Call module.generated_method with typed JSON arguments"
    )
    call.add_argument("operation")
    call.add_argument("--args", default="{}")
    call.add_argument("--args-file")
    call.add_argument("--no-wait", action="store_true")
    battery = sub.add_parser("battery")
    battery.add_argument("--profile", required=True)
    battery.add_argument("--bindings", required=True)
    battery.add_argument("--output", required=True)
    battery.add_argument("--simulate", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.command == "battery":
            from .battery import Profile, Runner, binding_nodes

            profile = Profile(**json.loads(Path(args.profile).read_text()))
            profile.validate()
            bindings = json.loads(Path(args.bindings).read_text())
            if args.simulate:
                from .simulator import simulated_cluster

                cluster, clock, _ = simulated_cluster(bindings, profile)
                result = Runner(
                    cluster,
                    bindings,
                    profile,
                    args.output,
                    clock=clock,
                    sleep=clock.sleep,
                ).run()
            else:
                if not args.nodes:
                    parser.error("Physical battery runs require --nodes")
                result = Runner(
                    Cluster(nodes_from(args.nodes, binding_nodes(bindings))),
                    bindings,
                    profile,
                    args.output,
                ).run()
            emit(result)
            return 0
        nodes = (
            nodes_from(args.nodes, [args.node] if args.node else None)
            if args.nodes
            else {"default": Node(args.endpoint, os.environ.get(args.token_env, ""))}
        )
        name = args.node or next(iter(nodes))
        node = nodes[name]
        if args.command == "devices":
            result = node.core.list_devices()
        elif args.command == "stop":
            result = node.stop(args.device)
        elif args.command == "pair":
            if not node.client.configuration.host == "http://usb":
                parser.error("Pairing is only available over serial:")
            # Pairing explicitly returns this credential over physically trusted USB.
            result = {
                "token": node.core.get_management_token().token.get_secret_value()
            }
        elif args.command == "operations":
            result = {
                name: sorted(
                    method
                    for method in dir(api)
                    if not method.startswith("_")
                    and callable(getattr(api, method))
                    and not method.endswith(
                        ("_with_http_info", "_without_preload_content")
                    )
                )
                for name, api in node.apis.items()
            }
        else:
            module, method = args.operation.split(".", 1)
            if method.startswith("_") or method.endswith(
                ("_with_http_info", "_without_preload_content")
            ):
                raise ValueError("Use a public generated operation")
            kwargs = json.loads(
                Path(args.args_file).read_text() if args.args_file else args.args
            )
            result = getattr(node.apis[module], method)(**kwargs, _request_timeout=2.0)
            if isinstance(result, Operation) and not args.no_wait:
                result = node.wait(result)
        emit(result)
        return 0
    except (ValueError, KeyError, AttributeError, TypeError) as exc:
        emit({"error": type(exc).__name__, "detail": str(exc)}, sys.stderr)
        return 2
    except Exception as exc:
        detail = {"error": type(exc).__name__, "detail": str(exc)}
        if hasattr(exc, "operation"):
            detail["operation"] = exc.operation.to_dict()
        if hasattr(exc, "status"):
            detail["status"] = exc.status
        if getattr(exc, "body", None):
            try:
                detail["problem"] = json.loads(exc.body)
            except (ValueError, TypeError):
                pass
        emit(detail, sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

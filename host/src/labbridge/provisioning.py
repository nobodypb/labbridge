"""Bounded host orchestration of the original RIDEN module's two-stage setup."""

import time


_ACTIVE = {"server_address", "wifi", "waiting_connection"}
_SEND_WINDOW_S = 60.0


def pair_rd_wifi(
    node,
    *,
    confirm_outputs_off=False,
    timeout_s=245.0,
    clock=time.monotonic,
    sleep=time.sleep,
):
    """Send both stages without an operator callback; return confirmed status.

    The caller first confirms physical OFF and puts the PSU into Wi-Fi setup.
    Phase one has no receiver acknowledgement: use the sender's 60 s window.
    Credentials follow automatically. A UDP join ACK is not success; completion
    must come from the firmware's session-scoped fresh TCP identity check.
    This routine neither registers a discovered source nor enables an output.
    """
    if not confirm_outputs_off:
        raise ValueError("Confirm physical outputs OFF before starting pairing")
    if timeout_s <= 0:
        raise ValueError("Pairing timeout must be positive")
    api = node.rd60xx
    before = api.get_rd_provisioning(_request_timeout=3.0).to_dict()
    if not before.get("ap_clients", 0):
        raise ValueError("Connect a client to the bridge AP before pairing")
    baseline = before.get("metrics", {})
    status = api.start_rd_provisioning(
        rd_provision_start={"confirm_outputs_off": True, "profile": "pc_v22"},
        _request_timeout=3.0,
    ).to_dict()
    session = status["session_id"]
    started = clock()
    wifi_started = None
    try:
        while True:
            if status.get("session_id") != session:
                raise RuntimeError("Provisioning session changed")
            state = status["state"]
            if state == "complete":
                return status
            if state not in _ACTIVE:
                raise RuntimeError("Provisioning ended: " + state)
            now = clock()
            if now - started >= timeout_s:
                raise TimeoutError("Pairing did not confirm a new TCP identity")
            advance = None
            if state == "server_address" and now - started >= _SEND_WINDOW_S:
                advance = "server_address"
            elif state == "wifi":
                if wifi_started is None:
                    wifi_started = now
                metrics = status.get("metrics", {})
                new_join = metrics.get("provision_join_acks", 0) > baseline.get(
                    "provision_join_acks", 0
                )
                new_socket = metrics.get("accepted", 0) > baseline.get("accepted", 0)
                connected = any(
                    c.get("connected") for c in status.get("candidates", [])
                )
                if (new_join and new_socket and connected) or (
                    now - wifi_started >= _SEND_WINDOW_S
                ):
                    advance = "wifi"
            if advance:
                status = api.advance_rd_provisioning(
                    rd_provision_advance={
                        "session_id": session,
                        "expected_phase": advance,
                    },
                    _request_timeout=3.0,
                ).to_dict()
                if advance == "server_address":
                    wifi_started = clock()
                continue
            sleep(0.5)
            status = api.get_rd_provisioning(_request_timeout=3.0).to_dict()
    except BaseException as error:
        try:
            current = api.get_rd_provisioning(_request_timeout=3.0).to_dict()
            if current.get("session_id") == session and current["state"] in _ACTIVE:
                api.cancel_rd_provisioning(
                    rd_provision_cancel={"session_id": session},
                    _request_timeout=3.0,
                )
        except BaseException as cleanup:
            raise BaseExceptionGroup(
                "Pairing failed; cancellation could not be confirmed", [error, cleanup]
            ) from None
        raise

import unittest
from types import SimpleNamespace

from labbridge.provisioning import pair_rd_wifi


class Clock:
    now = 0.0

    def time(self):
        return self.now

    def sleep(self, duration):
        self.now += duration


class Status:
    def __init__(self, value):
        self.value = value

    def to_dict(self):
        return self.value


class Provisioner:
    def __init__(self, clock, *, fresh_identity=True, interrupt=None):
        self.clock = clock
        self.fresh_identity = fresh_identity
        self.interrupt = interrupt
        self.state = "idle"
        self.session = "boot-p1"
        self.wifi_at = None
        self.calls = []

    def get_rd_provisioning(self, **kwargs):
        if self.state == "server_address" and self.clock.now >= 10:
            if self.interrupt == "cancel":
                self.state = "cancelled"
            if self.interrupt == "foreign":
                self.session = "other-p1"
        connected = self.wifi_at is not None and self.clock.now >= self.wifi_at + 20
        fresh = connected and self.fresh_identity
        return Status(
            {
                "state": self.state,
                "session_id": self.session,
                "ap_clients": 1,
                # A cached, connected candidate must never prove new pairing.
                "candidates": [{"connected": True}],
                "metrics": {
                    "accepted": 7 + int(fresh),
                    "provision_join_acks": int(connected),
                },
            }
        )

    def start_rd_provisioning(self, rd_provision_start, **kwargs):
        self.calls.append(("start", self.clock.now, rd_provision_start))
        self.state = "server_address"
        return self.get_rd_provisioning()

    def advance_rd_provisioning(self, rd_provision_advance, **kwargs):
        self.calls.append(("advance", self.clock.now, rd_provision_advance))
        assert rd_provision_advance["session_id"] == self.session
        assert rd_provision_advance["expected_phase"] == self.state
        if self.state == "server_address":
            self.state = "wifi"
            self.wifi_at = self.clock.now
        else:
            self.state = "complete" if self.fresh_identity else "waiting_connection"
        return self.get_rd_provisioning()

    def cancel_rd_provisioning(self, rd_provision_cancel, **kwargs):
        self.calls.append(("cancel", self.clock.now, rd_provision_cancel))
        self.state = "cancelled"
        return self.get_rd_provisioning()


class AutomaticPairingTests(unittest.TestCase):
    def run_pairing(self, *, confirmed=True, **options):
        clock = Clock()
        api = Provisioner(clock, **options)
        node = SimpleNamespace(rd60xx=api)
        result = pair_rd_wifi(
            node,
            confirm_outputs_off=confirmed,
            clock=clock.time,
            sleep=clock.sleep,
        )
        return result, api.calls

    def test_both_stages_run_without_operator_callback(self):
        result, calls = self.run_pairing()
        self.assertEqual(result["state"], "complete")
        advances = [c for c in calls if c[0] == "advance"]
        self.assertEqual(
            [c[2]["expected_phase"] for c in advances], ["server_address", "wifi"]
        )
        self.assertEqual([c[1] for c in advances], [60, 80])
        self.assertEqual(
            calls[0][2], {"confirm_outputs_off": True, "profile": "pc_v22"}
        )

    def test_initial_physical_off_confirmation_is_required(self):
        with self.assertRaises(ValueError):
            self.run_pairing(confirmed=False)

    def test_join_ack_and_cached_candidate_are_not_success(self):
        clock = Clock()
        api = Provisioner(clock, fresh_identity=False)
        with self.assertRaises(TimeoutError):
            pair_rd_wifi(
                SimpleNamespace(rd60xx=api),
                confirm_outputs_off=True,
                clock=clock.time,
                sleep=clock.sleep,
            )
        advances = [c for c in api.calls if c[0] == "advance"]
        self.assertEqual([c[1] for c in advances], [60, 120])
        self.assertEqual(api.calls[-1][0], "cancel")

    def test_cancel_prevents_sending_credentials(self):
        with self.assertRaisesRegex(RuntimeError, "cancelled"):
            self.run_pairing(interrupt="cancel")

    def test_another_session_is_neither_advanced_nor_cancelled(self):
        clock = Clock()
        api = Provisioner(clock, interrupt="foreign")
        with self.assertRaisesRegex(RuntimeError, "session"):
            pair_rd_wifi(
                SimpleNamespace(rd60xx=api),
                confirm_outputs_off=True,
                clock=clock.time,
                sleep=clock.sleep,
            )
        self.assertEqual([c[0] for c in api.calls], ["start"])

    def test_deadline_cancels_only_the_started_session(self):
        clock = Clock()
        api = Provisioner(clock)
        with self.assertRaises(TimeoutError):
            pair_rd_wifi(
                SimpleNamespace(rd60xx=api),
                confirm_outputs_off=True,
                timeout_s=20,
                clock=clock.time,
                sleep=clock.sleep,
            )
        self.assertEqual([c[0] for c in api.calls], ["start", "cancel"])
        self.assertEqual(api.calls[-1][2], {"session_id": "boot-p1"})

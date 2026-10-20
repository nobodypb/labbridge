import dataclasses, unittest
from labbridge.battery import Profile, Integrator

FIELDS = dict(
    name="test",
    chemistry="li-ion",
    series_count=1,
    maximum_voltage_v=4.2,
    maximum_current_a=0.6,
    charge_voltage_v=4.2,
    charge_current_a=0.5,
    termination_current_a=0.05,
    termination_hold_s=2,
    charge_timeout_s=60,
    discharge_current_a=0.2,
    discharge_cutoff_v=3.0,
    discharge_timeout_s=60,
    minimum_temperature_c=5.0,
    maximum_temperature_c=40.0,
    rest_s=2,
    pulse_current_a=0.3,
    pulse_s=1,
    pulse_rest_s=1,
    pulse_count=2,
    cycles=1,
    max_power_w=3.0,
    sample_interval_s=0.2,
    max_sample_gap_s=0.6,
)


class BatteryTests(unittest.TestCase):
    def test_explicit_profile_and_chemistry_limits(self):
        p = Profile(**FIELDS)
        p.validate()
        with self.assertRaises(ValueError):
            dataclasses.replace(p, series_count=2).validate()
        with self.assertRaises(ValueError):
            dataclasses.replace(p, chemistry="lifepo4", charge_voltage_v=4.2).validate()
        dataclasses.replace(
            p,
            chemistry="lifepo4",
            charge_voltage_v=3.6,
            maximum_voltage_v=3.65,
            discharge_cutoff_v=2.5,
        ).validate()
        with self.assertRaises(TypeError):
            Profile(name="incomplete")

    def test_integration_excludes_stale_gaps_repeats_and_stage_changes(self):
        a = Integrator(max_gap_s=2)
        a.add(0, 4.0, 1.0, True, 1, "discharge")
        a.add(1, 4.0, 1.0, True, 2, "discharge")
        self.assertAlmostEqual(a.ah, 1 / 3600)
        self.assertAlmostEqual(a.wh, 4 / 3600)
        a.add(2, 4.0, 1.0, False, 3, "discharge")
        a.add(3, 4.0, 1.0, True, 4, "discharge")
        a.add(10, 4.0, 1.0, True, 5, "discharge")
        a.add(11, 4.0, 1.0, True, 5, "discharge")
        self.assertAlmostEqual(a.ah, 1 / 3600)
        a.add(12, 4.0, 1.0, True, 6, "charge")
        self.assertAlmostEqual(a.ah, 1 / 3600)


class WorkflowTests(unittest.TestCase):
    def bindings(self):
        return dict(
            source=dict(node="source-node", device="supply"),
            load=dict(node="load-node", device="load"),
            interlock=dict(node="switch-node", device="paths"),
            cell_probe=dict(node="switch-node", device="probe", channel="cell"),
        )

    def test_both_chemistries_complete_cycle_through_generated_api_and_restore(self):
        import tempfile, pathlib, json
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        for chemistry, v, cutoff in [("li-ion", 4.2, 3.0), ("lifepo4", 3.6, 2.5)]:
            p = Profile(
                **{
                    **FIELDS,
                    "chemistry": chemistry,
                    "charge_voltage_v": v,
                    "maximum_voltage_v": 4.2 if chemistry == "li-ion" else 3.65,
                    "discharge_cutoff_v": cutoff,
                }
            )
            cluster, clock, transports = simulated_cluster(self.bindings(), p)
            with tempfile.TemporaryDirectory() as directory:
                output = pathlib.Path(directory) / "run"
                result = Runner(
                    cluster, self.bindings(), p, output, clock=clock, sleep=clock.sleep
                ).run()
                self.assertEqual(result["status"], "succeeded")
                self.assertGreater(result["integration"]["0:discharge"]["ah"], 0)
                self.assertTrue((output / "samples.csv").exists())
                self.assertEqual(
                    json.loads((output / "run.json").read_text())["status"], "succeeded"
                )
            for rest in transports.values():
                self.assertTrue(all(not d["output"] for d in rest.devices.values()))
            self.assertEqual(transports["load-node"].devices["load"]["current"], 0.1)

    def test_exception_stops_every_node_and_writes_failure_metadata(self):
        import tempfile, pathlib, json
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        p = Profile(**FIELDS)
        cluster, clock, transports = simulated_cluster(self.bindings(), p)
        transports["source-node"].fail_next = True
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "run"
            with self.assertRaises(Exception):
                Runner(
                    cluster, self.bindings(), p, output, clock=clock, sleep=clock.sleep
                ).run()
            self.assertEqual(
                json.loads((output / "run.json").read_text())["status"], "failed"
            )
        for rest in transports.values():
            self.assertTrue(all(not d["output"] for d in rest.devices.values()))

    def test_checkpoint_precedes_actuation_and_reboot_aborts_without_integrating(self):
        import tempfile, pathlib, json
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        p = Profile(**FIELDS)
        cluster, clock, transports = simulated_cluster(self.bindings(), p)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "run"
            checked = []

            def reboot_after_first_active_sample(dt):
                if (
                    transports["source-node"].devices["supply"]["output"]
                    and not checked
                ):
                    saved = json.loads((output / "run.json").read_text())
                    self.assertTrue(saved["restore_pending"])
                    self.assertIn("source", saved["original_settings"])
                    self.assertIn("load", saved["original_settings"])
                    checked.append(True)
                    transports["source-node"].boot_id = "sim-rebooted"

            clock.listeners.append(reboot_after_first_active_sample)
            with self.assertRaisesRegex(RuntimeError, "reboot"):
                Runner(
                    cluster, self.bindings(), p, output, clock=clock, sleep=clock.sleep
                ).run()
            self.assertTrue(checked)
            self.assertEqual(
                json.loads((output / "run.json").read_text())["status"], "failed"
            )
        for rest in transports.values():
            self.assertTrue(all(not d["output"] for d in rest.devices.values()))

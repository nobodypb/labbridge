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
        dataclasses.replace(
            p, discharge_cutoff_v=2.95
        ).validate()  # Precision belongs to the selected driver.
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

    def test_source_reconfiguration_keeps_intermediate_power_within_node_limit(self):
        import tempfile, pathlib
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        p = Profile(**{**FIELDS, "max_power_w": 2.2})
        cluster, clock, transports = simulated_cluster(self.bindings(), p)
        supply = transports["source-node"].devices["supply"]
        supply.update(voltage=3.0, current=0.6)
        api = cluster.nodes["source-node"].components
        original = api.set_component_voltage

        def checked_voltage(*args, **kwargs):
            if kwargs["voltage_request"].voltage_v * supply["current"] > p.max_power_w:
                raise ValueError(
                    "Intermediate source settings exceed local power limit"
                )
            return original(*args, **kwargs)

        api.set_component_voltage = checked_voltage
        with tempfile.TemporaryDirectory() as directory:
            result = Runner(
                cluster,
                self.bindings(),
                p,
                pathlib.Path(directory) / "run",
                clock=clock,
                sleep=clock.sleep,
            ).run()
            self.assertEqual(result["status"], "succeeded")
        self.assertEqual((supply["voltage"], supply["current"]), (3.0, 0.6))

    def test_direct_wiring_completes_without_switch_for_both_chemistries(self):
        import tempfile, pathlib
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        bindings = self.bindings()
        bindings.pop("interlock")
        for chemistry, voltage, cutoff in [("li-ion", 4.2, 3.0), ("lifepo4", 3.6, 2.5)]:
            p = Profile(
                **{
                    **FIELDS,
                    "chemistry": chemistry,
                    "charge_voltage_v": voltage,
                    "maximum_voltage_v": 4.2 if chemistry == "li-ion" else 3.65,
                    "discharge_cutoff_v": cutoff,
                }
            )
            cluster, clock, transports = simulated_cluster(bindings, p)
            with tempfile.TemporaryDirectory() as directory:
                result = Runner(
                    cluster,
                    bindings,
                    p,
                    pathlib.Path(directory) / "run",
                    clock=clock,
                    sleep=clock.sleep,
                ).run()
                self.assertEqual(result["status"], "succeeded")
            self.assertTrue(
                all(
                    not d["output"]
                    for t in transports.values()
                    for d in t.devices.values()
                )
            )

    def test_signed_inline_meter_and_additional_temperature_are_recorded(self):
        import tempfile, pathlib, csv
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        bindings = self.bindings()
        bindings.pop("interlock")
        meter = dict(
            node="meter-node", device="inline-meter", current_sign=1, power_sign=1
        )
        bindings["meters"] = {
            "charge": meter,
            "rest": meter,
            "discharge": {**meter, "current_sign": -1},
        }
        bindings["sensors"] = [
            dict(
                node="meter-node",
                device="inline-meter",
                component="temperature-die",
                label="shunt",
            )
        ]
        p = Profile(**FIELDS)
        cluster, clock, transports = simulated_cluster(bindings, p)
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "run"
            result = Runner(
                cluster, bindings, p, output, clock=clock, sleep=clock.sleep
            ).run()
            self.assertGreater(result["integration"]["0:discharge"]["ah"], 0)
            with (output / "samples.csv").open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertTrue(
                any(
                    float(r["current_raw_a"]) < 0
                    and float(r["current_a"]) > 0
                    and r["node"] == "meter-node"
                    for r in rows
                )
            )
            with (output / "sensors.csv").open() as stream:
                extras = list(csv.DictReader(stream))
            self.assertTrue(
                any(r["label"] == "shunt" and r["unit"] == "degC" for r in extras)
            )

    def test_selected_meter_staleness_stops_direct_wiring_run(self):
        import tempfile, pathlib, json
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        bindings = self.bindings()
        bindings.pop("interlock")
        bindings["meters"] = {"charge": dict(node="meter-node", device="inline-meter")}
        p = Profile(**FIELDS)
        cluster, clock, transports = simulated_cluster(bindings, p)
        clock.listeners.append(
            lambda dt: setattr(transports["meter-node"], "stale", True)
        )
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "run"
            with self.assertRaisesRegex(RuntimeError, "Fresh DC meter"):
                Runner(
                    cluster, bindings, p, output, clock=clock, sleep=clock.sleep
                ).run()
            self.assertEqual(
                json.loads((output / "run.json").read_text())["status"], "failed"
            )
        self.assertTrue(
            all(
                not d["output"] for t in transports.values() for d in t.devices.values()
            )
        )

    def test_signed_power_and_two_meters_on_one_node_are_supported(self):
        import tempfile, pathlib
        from labbridge.battery import Runner
        from labbridge.simulator import simulated_cluster

        bindings = self.bindings()
        bindings.pop("interlock")
        bindings["meters"] = {
            "charge": dict(node="meter-node", device="first"),
            "discharge": dict(
                node="meter-node", device="second", current_sign=-1, power_sign=-1
            ),
        }
        p = Profile(**FIELDS)
        cluster, clock, transports = simulated_cluster(bindings, p)
        with tempfile.TemporaryDirectory() as directory:
            result = Runner(
                cluster,
                bindings,
                p,
                pathlib.Path(directory) / "run",
                clock=clock,
                sleep=clock.sleep,
            ).run()
            self.assertEqual(result["status"], "succeeded")

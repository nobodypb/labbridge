import csv
import json
from pathlib import Path
import tempfile
import threading
import sys
import types
import unittest
from unittest import mock
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from labbridge import Client, SerialClient, BridgeError
from labbridge.simulator import Simulator
from labbridge.sweep import sweep


class Clock:
    now = 0.
    def __call__(self): return self.now


class BridgeTests(unittest.TestCase):
    def test_session_exit_attempts_independent_off_after_lost_lease(self):
        s = Simulator()
        run = s.session(["load"])
        with self.assertRaisesRegex(BridgeError, "Shutdown not confirmed"):
            with run:
                run.command("load", output=True)
                s.token = ""
                s.owned = []
                s.stop_outputs = [None]*3
        self.assertFalse(s.data["load"]["output"])
        self.assertTrue(run.stop_result["emergency_stop"]["output_off_confirmed"])
        self.assertFalse(run.stop_result["stop_outputs"][0])

    def test_failed_independent_off_keeps_shutdown_unknown_and_original_error(self):
        s = Simulator()
        run = s.session(["load"])
        with self.assertRaisesRegex(ValueError, "original failure"):
            with run:
                run.command("load", output=True)
                s.disconnected.add("load")
                raise ValueError("original failure")
        self.assertIsNone(run.stop_result["stop_outputs"][0])
        self.assertTrue(run.stop_result["emergency_stop"]["load_stop_pending"])
        self.assertTrue(s.data["load"]["output"])

    def test_valid_command_renews_lease_but_invalid_command_does_not(self):
        clock = Clock()
        sim = Simulator(clock)
        token = sim.request("POST", "/session", {"devices": ["load"]})["lease"]
        clock.now = 9
        sim.request("POST", "/devices/load/command", {"current_a": .1}, token)
        self.assertEqual(sim.deadline, 19)
        clock.now = 18
        with self.assertRaises(BridgeError):
            sim.request("POST", "/devices/load/command", {"current_a": 100}, token)
        self.assertEqual(sim.deadline, 19)
        clock.now = 19.1
        self.assertFalse(sim.request("GET", "/session")["active"])

    def test_serial_api_uses_crlf_lease_and_stop(self):
        model = Simulator()
        class FakePort:
            def __init__(self, **kwargs):
                self.dtr = self.rts = None
                self.port = None
                self.pending = bytearray()
                self.is_open = False
                self.frames = []
                self.input = bytearray()
                self.drop_next = False
            def open(self):
                self.is_open = True
                self.pending.extend(b"boot log\n")
            def close(self): self.is_open = False
            def reset_input_buffer(self): self.pending.clear()
            def flush(self): pass
            @property
            def in_waiting(self): return len(self.pending)
            def write(self, data):
                self.input.extend(data)
                while b"\n" in self.input:
                    raw, _, rest = self.input.partition(b"\n")
                    self.input = bytearray(rest)
                    self.frames.append(raw + b"\n")
                    self.pending.extend(b"I (1) console: command\n")
                    frame = json.loads(raw)
                    try:
                        result = model.request(frame["method"], frame["path"],
                                               frame.get("body"), frame.get("lease", ""))
                        status = 200
                    except BridgeError as error:
                        result, status = {"error": str(error)}, error.status
                    if self.drop_next:
                        self.drop_next = False
                    else:
                        self.pending.extend(b"LABJSON " + json.dumps({"id": frame["id"],
                            "status": status, "result": result}).encode() + b"\n")
                return len(data)
            def read(self, size):
                data = bytes(self.pending[:size]); del self.pending[:size]
                return data
        fake_module = types.SimpleNamespace(Serial=FakePort)
        with mock.patch.dict(sys.modules, {"serial": fake_module}):
            with SerialClient("/dev/fake", timeout=.2) as client:
                self.assertFalse(client.port.dtr)
                self.assertFalse(client.port.rts)
                self.assertEqual(len(client.devices()), 3)
                before = len(client.port.frames)
                client.port.drop_next = True
                self.assertFalse(client.request("GET", "/session")["active"])
                self.assertEqual(len(client.port.frames) - before, 2)
                ids = [json.loads(frame)["id"] for frame in client.port.frames[before:]]
                self.assertNotEqual(ids[0], ids[1])
                before = len(client.port.frames)
                client.port.drop_next = True
                with self.assertRaisesRegex(BridgeError, "USB response timed out"):
                    client.request("POST", "/devices/psu1/command", {"output": True})
                self.assertEqual(len(client.port.frames) - before, 1)
                with self.assertRaises(BridgeError) as failed:
                    client.request("POST", "/devices/psu1/command", {"output": True})
                self.assertEqual(failed.exception.status, 409)
                with client.session(["psu1"]) as run:
                    run.command("psu1", output=True)
                    self.assertTrue(model.data["psu1"]["output"])
                self.assertFalse(model.data["psu1"]["output"])
                self.assertTrue(all(frame.endswith(b"\r\n") for frame in client.port.frames))
            self.assertFalse(client.port.is_open)
            with SerialClient("/dev/ttyACM0", timeout=.2) as native:
                self.assertTrue(native.port.dtr)

    def test_exclusive_lease_expiry_and_stop_order(self):
        clock = Clock()
        s = Simulator(clock)
        token = s.request("POST", "/session", {"devices":["load","psu1","psu2"]})["lease"]
        with self.assertRaises(BridgeError): s.request("POST", "/session", {"devices":["load"]})
        s.request("POST", "/devices/psu1/command", {"output":True}, token)
        clock.now = 10.
        status = s.request("GET", "/session")
        self.assertFalse(status["active"])
        self.assertEqual(status["last_stop_reason"], "lease_expired")
        self.assertEqual([c[0] for c in s.commands[-3:]], ["psu1","psu2","load"])
        with self.assertRaises(BridgeError): s.request("POST", "/session/heartbeat", {}, token)

    def test_limits_and_unknown_device(self):
        s = Simulator()
        with s.session(["load"]) as run:
            with self.assertRaises(BridgeError): run.command("load", current_a=100)
            with self.assertRaises(BridgeError): run.command("load", voltage_v=12)
            with self.assertRaises(ValueError): run.command("psu1", output=True)
            self.assertEqual(s.data["load"]["set_current_a"], 0)

    def test_stale_data_aborts_and_disconnected_output_is_unknown(self):
        clock = Clock();s=Simulator(clock)
        s.request("POST", "/session", {"devices":["load","psu1"]})
        s.frozen.add("load");s.disconnected.add("psu1");clock.now=4
        status=s.request("GET", "/session")
        self.assertEqual(status["last_stop_reason"], "device_unavailable")
        self.assertIsNone(status["stop_outputs"][1])
        self.assertFalse(status["stop_outputs"][0])

    def test_sweep_writes_unique_samples_and_stops(self):
        s=Simulator(sample_period=.01)
        for d in s.data.values():d["limits"]=dict(voltage_v=15.,current_a=.5,power_w=6.)
        with tempfile.TemporaryDirectory() as tmp:
            path=sweep(s,Path(tmp)/"run.csv",[.1,.2],settle=0,samples=2,interval=.02)
            with path.open() as file:rows=list(csv.DictReader(file))
            self.assertEqual(len(rows),8)
            for name in ("load","psu1"):
                seq=[int(r["sequence"]) for r in rows if r["device"]==name]
                self.assertEqual(len(seq),len(set(seq)))
            meta=json.loads(path.with_suffix(".csv.json").read_text())
            self.assertEqual(meta["result"],"completed")
            self.assertTrue(meta["simulation"])
            self.assertFalse(s.data["load"]["output"])
            self.assertFalse(s.data["psu1"]["output"])

    def test_unverified_load_prevents_any_write(self):
        s=Simulator();s.data["load"]["capabilities"]=["measure"]
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(BridgeError):sweep(s,Path(tmp)/"run.csv",[.1])
        self.assertEqual(s.commands,[])

    def test_existing_results_are_not_modified(self):
        s=Simulator()
        for d in s.data.values():d["limits"]=dict(voltage_v=15.,current_a=.5,power_w=6.)
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"existing.csv"
            path.write_text("previous measurement\n")
            with self.assertRaises(FileExistsError):sweep(s,path,[.1])
            self.assertEqual(path.read_text(),"previous measurement\n")
            self.assertFalse(path.with_suffix(".csv.json").exists())
            self.assertEqual(s.commands,[])

    def test_second_supply_simulation(self):
        s=Simulator(sample_period=.01,source="psu2")
        for d in s.data.values():d["limits"]=dict(voltage_v=15.,current_a=.5,power_w=6.)
        with tempfile.TemporaryDirectory() as tmp:
            path=sweep(s,Path(tmp)/"second.csv",[.1],source="psu2",settle=.02,samples=1,interval=.01)
            with path.open() as f:rows=list(csv.DictReader(f))
            self.assertEqual({r["device"] for r in rows},{"load","psu2"})
            self.assertTrue(all(float(r["voltage_v"])==12. for r in rows))

    def test_wifi_loss_never_resumes(self):
        s=Simulator();s.request("POST","/session",{"devices":["load"]})
        s.wifi=False;self.assertFalse(s.request("GET","/session")["active"])
        s.wifi=True;self.assertFalse(s.request("GET","/session")["active"])
        self.assertFalse(s.data["load"]["output"])

    def test_http_auth_and_errors(self):
        model=Simulator()
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_GET(self):self.handle_api()
            def do_POST(self):self.handle_api()
            def handle_api(self):
                code=200
                try:
                    if self.headers.get("Authorization")!="Bearer test":raise BridgeError("invalid_token",401)
                    size=int(self.headers.get("Content-Length","0"))
                    body=json.loads(self.rfile.read(size)) if size else None
                    result=model.request(self.command,self.path.removeprefix("/api/v1"),body,self.headers.get("X-Lab-Lease",""))
                except BridgeError as exc:code=exc.status;result={"error":str(exc)}
                data=json.dumps(result).encode();self.send_response(code);self.send_header("Content-Length",str(len(data)));self.end_headers();self.wfile.write(data)
        server=ThreadingHTTPServer(("127.0.0.1",0),Handler)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            url=f"http://127.0.0.1:{server.server_port}"
            with self.assertRaises(BridgeError) as err:Client(url,"wrong").devices()
            self.assertEqual(err.exception.status,401)
            client=Client(url,"test")
            self.assertEqual(len(client.devices()),3)
            with client.session(["load"]) as run:run.command("load",current_a=.1)
            self.assertEqual(model.data["load"]["set_current_a"],.1)
        finally:server.shutdown();server.server_close();thread.join()


if __name__ == "__main__": unittest.main()

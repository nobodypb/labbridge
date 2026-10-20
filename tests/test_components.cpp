#include "labbridge/node.hpp"
#include <cassert>
using namespace labbridge;
struct Instrument : Driver {
  std::string command;
  Json describe(uint64_t) {
    return Json::parse(
        R"({"id":"combo","driver":"fixture","transport":"test","identity":"fixture","capabilities":["source.cc_cv","load.cc"],"state":"ready","output":"off","limits":{"voltage_v":5,"current_a":1,"power_w":5,"max_age_ms":3000}})");
  }
  Json measurements(uint64_t) {
    return Json::parse(
        R"({"device_id":"combo","sequence":7,"received_ms":10,"age_ms":0,"quality":"valid","voltage_v":4,"current_a":-0.1,"power_w":0.4,"set_current_a":0.1,"set_voltage_v":4.1,"temperature_channels":[{"id":"cell-2","kind":"cell","quality":"valid","value_c":24,"received_ms":10,"age_ms":0}]})");
  }
  bool supports(const std::string &op) const {
    return op == "setSourceCurrent" || op == "setLoadCurrent" ||
           op == "setSourceVoltage" || op == "setOutput";
  }
  void tick(uint64_t) {}
  void stop(uint64_t) { command = "stop"; }
  Json execute(const std::string &op, const Json &, uint64_t) {
    command = op;
    return Json::parse(
        R"({"status":"succeeded","stage":"confirmed","read_attempts":1,"write_attempts":1,"reconnects":0,"duration_ms":0,"final_output":"off","restore_pending":false})");
  }
};
struct Multi : Instrument {
  std::map<std::string, bool> enabled{{"a", false}, {"b", false}};
  Json components(uint64_t) override {
    return Json::parse(
        R"([{"id":"a","kind":"source","actions":["output"]},{"id":"b","kind":"source","actions":["output"]}])");
  }
  std::string component_command(const std::string &id,
                                const std::string &action, uint64_t) override {
    return action == "stop" ? "stopDevice" : id + "." + action;
  }
  bool supports(const std::string &op) const override {
    return op == "a.output" || op == "b.output";
  }
  Json actuator_state(const std::string &id, uint64_t) override {
    Json r;
    r.set("state", "ready")
        .set("output", enabled.at(id) ? "on" : "off")
        .set("settings", Json());
    return r;
  }
  Json execute(const std::string &op, const Json &body, uint64_t now) override {
    enabled.at(op.substr(0, 1)) = body.boolean("enabled");
    return Instrument::execute(op, body, now);
  }
  Json operation(uint64_t now) override {
    return Instrument::execute("status", Json(), now);
  }
  void stop_component(const std::string &id, uint64_t) override {
    enabled.at(id) = false;
  }
  void stop(uint64_t) override {
    for (auto &p : enabled)
      p.second = false;
  }
};
int main() {
  Node n("node", "boot", "test");
  auto d = std::make_shared<Instrument>();
  n.add("combo", d);
  auto get = [&](std::string p) {
    return n.request("GET", "/api/v1/devices/combo" + p, Json(), {}, 10);
  };
  auto list = get("/components");
  assert(list.status == 200);
  auto sample = get("/components/current/measurement");
  assert(sample.status == 200);
  assert(sample.body.number("value") == -.1 &&
         sample.body.text("unit") == "A" &&
         sample.body.text("boot_id") == "boot");
  auto t = get("/components/temperature-cell-2/measurement");
  assert(t.status == 200 && t.body.number("value") == 24);
  assert(get("/components/missing/measurement").status == 404);
  auto lease = n.request(
      "POST", "/api/v1/leases",
      Json::parse(R"({"device_ids":["combo"],"ttl_ms":1000})"), {}, 10);
  auto put = [&](std::string component, std::string key) {
    return n.request(
        "PUT", "/api/v1/devices/combo/components/" + component + "/current",
        Json::parse(R"({"current_a":0.1})"),
        {{"X-Lab-Lease", lease.body.text("id")}, {"Idempotency-Key", key}}, 10);
  };
  assert(put("source", "a").status == 202 && d->command == "setSourceCurrent");
  assert(put("sink", "b").status == 202 && d->command == "setLoadCurrent");
  assert(put("current", "c").status >= 400); // A sensor is never writable.
  auto multi = std::make_shared<Multi>();
  n.add("multi", multi);
  auto l = n.request("POST", "/api/v1/leases",
                     Json::parse(R"({"device_ids":["multi"]})"), {}, 10);
  for (auto id : {"a", "b"})
    assert(n.request("PUT",
                     std::string("/api/v1/devices/multi/components/") + id +
                         "/output",
                     Json::parse(R"({"enabled":true})"),
                     {{"X-Lab-Lease", l.body.text("id")},
                      {"Idempotency-Key", std::string("on-") + id}},
                     10)
               .status == 202);
  auto stopped = n.request("POST", "/api/v1/devices/multi/components/a/stop",
                           Json(), {{"Idempotency-Key", "stop-a"}}, 10);
  assert(stopped.body.text("component_id") == "a" &&
         stopped.body.text("final_output") == "off" &&
         stopped.body.text("status") == "succeeded");
  assert(stopped.status == 202 && !multi->enabled.at("a") &&
         multi->enabled.at("b"));
  assert(n.request("GET", "/api/v1/devices/multi/components/b/state", Json(),
                   {}, 10)
             .body.text("output") == "on");
}

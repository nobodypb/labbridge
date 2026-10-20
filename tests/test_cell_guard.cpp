#include "labbridge/node.hpp"
#include <cassert>
using namespace labbridge;
struct Device : Driver {
  bool on = false, internal = false, fresh = true;
  unsigned stopped = 0;
  std::string id;
  Device(std::string s) : id(std::move(s)) {}
  Json describe(uint64_t) override {
    Json j;
    j.set("output", on ? "on" : "off");
    return j;
  }
  Json measurements(uint64_t now) override {
    auto a = Json::array();
    Json t;
    t.set("id", "probe")
        .set("kind", internal ? "internal" : "cell")
        .set("quality", fresh ? "valid" : "stale")
        .set("value_c", 25.)
        .set("received_ms", double(now))
        .set("age_ms", 0.);
    a.append(t);
    Json j;
    j.set("temperature_channels", a);
    return j;
  }
  bool supports(const std::string &) const override { return true; }
  void tick(uint64_t) override {}
  void stop(uint64_t) override {
    on = false;
    ++stopped;
  }
  Json execute(const std::string &, const Json &, uint64_t) override {
    on = true;
    return Json::parse(R"({"status":"succeeded"})");
  }
};
struct ComponentSensor : Device {
  ComponentSensor() : Device("component-sensor") {}
  Json measurements(uint64_t) override { return Json(); }
  Json components(uint64_t) override {
    return Json::parse(
        R"([{"id":"cell-a","kind":"sensor","quantity":"temperature","unit":"degC","temperature_kind":"external_probe","actions":[]}])");
  }
  Json component_reading(const std::string &, uint64_t now) override {
    Json r;
    r.set("quality", fresh ? "valid" : "stale")
        .set("value", 25.)
        .set("received_ms", double(now))
        .set("age_ms", 0.);
    return r;
  }
};
int main() {
  Node n("n", "boot", "sim");
  auto source = std::make_shared<Device>("source"),
       sensor = std::make_shared<Device>("sensor");
  n.add("source", source);
  n.add("sensor", sensor);
  n.cell_guard("source", {"sensor", "probe", 0, 45, 1000});
  auto l = n.request("POST", "/api/v1/leases",
                     Json::parse(R"({"device_ids":["source"]})"), {}, 0);
  Headers h{{"X-Lab-Lease", l.body.text("id")}, {"Idempotency-Key", "one"}};
  sensor->internal = true;
  assert(n.request("PUT", "/api/v1/devices/source/output",
                   Json::parse(R"({"enabled":true})"), h, 1)
             .status == 422);
  sensor->internal = false;
  assert(n.request("PUT", "/api/v1/devices/source/output",
                   Json::parse(R"({"enabled":true})"), h, 2)
             .status == 202);
  assert(source->on);
  sensor->fresh = false;
  n.tick(3);
  assert(!source->on && source->stopped == 1);
  Node abstract("n", "boot", "sim");
  auto actor = std::make_shared<Device>("actor");
  auto external = std::make_shared<ComponentSensor>();
  abstract.add("actor", actor);
  abstract.add("probe", external);
  abstract.cell_guard("actor", {"probe", "cell-a", 0, 45, 1000});
  auto lease =
      abstract.request("POST", "/api/v1/leases",
                       Json::parse(R"({"device_ids":["actor"]})"), {}, 0);
  auto enabled = abstract.request("PUT", "/api/v1/devices/actor/output",
                                  Json::parse(R"({"enabled":true})"),
                                  {{"X-Lab-Lease", lease.body.text("id")},
                                   {"Idempotency-Key", "abstract-on"}},
                                  1);
  assert(enabled.status == 202 && actor->on);
  external->fresh = false;
  abstract.tick(2);
  assert(!actor->on);
}

#include "labbridge/node.hpp"
#include <cassert>
using namespace labbridge;
struct Fake : Driver {
  bool off = true;
  unsigned starts = 0, stops = 0;
  Json describe(uint64_t) override {
    return Json::parse(
        R"({"id":"load","driver":"fake","identity":"test","transport":"fake","capabilities":["load.cc","output"],"state":"ready","output":"off","limits":{"voltage_v":25,"current_a":0.2,"power_w":5,"max_age_ms":3000}})");
  }
  Json measurements(uint64_t) override {
    return Json::parse(
        R"({"device_id":"load","sequence":1,"received_ms":0,"age_ms":0,"quality":"valid","temperature_channels":[],"voltage_v":24,"current_a":0,"power_w":0})");
  }
  bool supports(const std::string &op) const override {
    return op == "setLoadCurrent";
  }
  void tick(uint64_t) override {}
  void stop(uint64_t) override {
    off = true;
    ++stops;
  }
  Json execute(const std::string &, const Json &, uint64_t) override {
    ++starts;
    return Json::parse(
        R"({"status":"succeeded","stage":"confirmed","read_attempts":1,"write_attempts":1,"reconnects":0,"duration_ms":1,"final_output":"off","restore_pending":false})");
  }
};
int main() {
  Node n("test", "boot", "sim");
  auto d = std::make_shared<Fake>();
  n.add("load", d);
  auto lease =
      n.request("POST", "/api/v1/leases",
                Json::parse(R"({"device_ids":["load"],"ttl_ms":1000})"), {}, 0);
  assert(lease.status == 200);
  auto token = lease.body.text("id");
  Headers h{{"X-Lab-Lease", token}, {"Idempotency-Key", "k1"}};
  auto write = n.request("PUT", "/api/v1/devices/load/load/current",
                         Json::parse(R"({"current_a":0.1})"), h, 10);
  assert(write.status == 202 && d->starts == 1);
  auto duplicate = n.request("PUT", "/api/v1/devices/load/load/current",
                             Json::parse(R"({"current_a":0.1})"), h, 11);
  assert(duplicate.body.text("id") == write.body.text("id") && d->starts == 1);
  assert(n.request("PUT", "/api/v1/devices/load/load/current",
                   Json::parse(R"({"current_a":0.2})"), h, 12)
             .status == 409);
  h["Idempotency-Key"] = "k2";
  assert(n.request("PUT", "/api/v1/devices/load/load/current",
                   Json::parse(R"({"current_a":-1})"), h, 12)
             .status == 400);
  n.tick(1001);
  assert(d->stops >= 1);
  assert(n.request("PUT", "/api/v1/devices/load/load/current",
                   Json::parse(R"({"current_a":0.1})"), h, 1002)
             .status == 423);
  // STOP is available with no lease after expiry.
  assert(n.request("POST", "/api/v1/devices/load/stop", Json::parse("{}"),
                   {{"Idempotency-Key", "stop"}}, 1003)
             .status == 202);
  assert(n.request("GET", "/api/v1/devices/missing", Json(), {}, 1004).status ==
         404);
}

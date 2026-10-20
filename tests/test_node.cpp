#include "labbridge/node.hpp"
#include <cassert>
using namespace labbridge;
struct Fake : Driver {
  bool off = true;
  unsigned starts = 0, stops = 0;
  bool unavailable = false;
  Json describe(uint64_t) override {
    auto j = Json::parse(
        R"({"id":"load","driver":"fake","identity":"test","transport":"fake","capabilities":["load.cc","output"],"state":"ready","output":"off","limits":{"voltage_v":25,"current_a":0.2,"power_w":5,"max_age_ms":3000}})");
    if (unavailable)
      j.set("state", "unavailable");
    return j;
  }
  Json measurements(uint64_t) override {
    return Json::parse(
        R"({"device_id":"load","sequence":1,"received_ms":0,"age_ms":0,"quality":"valid","temperature_channels":[],"voltage_v":24,"current_a":0,"power_w":0})");
  }
  bool supports(const std::string &op) const override {
    return op == "setLoadCurrent" || op == "setDl24mSettings";
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
  Headers order{{"X-Lab-Lease", token}, {"Idempotency-Key", "order"}};
  auto first_order =
      n.request("PUT", "/api/v1/devices/load/dl24m/settings",
                Json::parse(R"({"cutoff_voltage_v":1,"mode":0})"), order, 20);
  auto second_order =
      n.request("PUT", "/api/v1/devices/load/dl24m/settings",
                Json::parse(R"({"mode":0,"cutoff_voltage_v":1.0})"), order, 21);
  assert(first_order.status == 202 && second_order.status == 202 &&
         first_order.body.text("id") == second_order.body.text("id"));
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
  auto again = n.request("POST", "/api/v1/leases",
                         Json::parse(R"({"device_ids":["load"]})"), {}, 1100);
  auto before = d->stops;
  d->unavailable = true;
  n.tick(1101);
  assert(d->stops > before);
  d->unavailable = false;
  auto capacity =
      n.request("POST", "/api/v1/leases",
                Json::parse(R"({"device_ids":["load"]})"), {}, 2000);
  Headers many{{"X-Lab-Lease", capacity.body.text("id")}};
  for (unsigned k = 0; k < 130; ++k) {
    many["Idempotency-Key"] = "fill" + std::to_string(k);
    n.request("PUT", "/api/v1/devices/load/load/current",
              Json::parse(R"({"current_a":0.1})"), many, 2001 + k);
  }
  assert(n.request("POST", "/api/v1/devices/load/stop", Json::parse("{}"),
                   {{"Idempotency-Key", "emergency-after-full"}}, 2200)
             .status == 202);
}

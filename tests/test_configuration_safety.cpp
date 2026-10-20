#include "dl24m_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Offline : Endpoint {
  bool connected() const { return false; }
  uint64_t generation() const { return 1; }
  bool send(const Bytes &) { return false; }
  Bytes receive() { return {}; }
};
int main() {
  Offline w;
  auto save = [](const Json &) {};
  Dl24mDriver unbound("load", "wrong-address", w, {25, .2, 5, 3000, 0}, save,
                      Json());
  unbound.tick(0);
  assert(unbound.safe_to_reconfigure(0));
  Dl24mDriver active("load", "missing", w, {25, .2, 5, 3000, 0}, save,
                     Json::parse(R"({"may_be_on":true,"stop_pending":true})"));
  active.tick(0);
  assert(!active.safe_to_reconfigure(0));
  Node node("node", "boot", "sim");
  node.add("load", std::shared_ptr<Driver>(&active, [](Driver *) {}));
  assert(node.idle(0));
  assert(!node.quiescent(0));
  Dl24mDriver legacy("load", "missing", w, {25, .2, 5, 3000, 0}, save,
                     Json::parse(R"({"stop_pending":true})"));
  assert(!legacy.safe_to_reconfigure(0));
  node.inhibit("Incomplete startup configuration", 0);
  auto denied = node.request(
      "POST", "/api/v1/leases",
      Json::parse(R"({"device_ids":["load"],"ttl_ms":1000})"), {}, 0);
  assert(denied.status == 409);
  auto stop = node.request("POST", "/api/v1/devices/load/stop", Json(),
                           {{"Idempotency-Key", "emergency"}}, 0);
  assert(stop.status == 202);
}

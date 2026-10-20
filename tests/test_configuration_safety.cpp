#include "dl24m_driver.hpp"
#include "labbridge/wifi_security.hpp"
#include <cassert>
using namespace labbridge;
struct Offline : Endpoint {
  bool connected() const { return false; }
  uint64_t generation() const { return 1; }
  bool send(const Bytes &) { return false; }
  Bytes receive() { return {}; }
};
int main() {
  // AP security is independent of station credentials and requires a valid
  // WPA2 passphrase (or an explicit empty string to restore the open default).
  Node ap_node("test", "boot", "sim");
  unsigned applied = 0;
  for (const auto &key :
       {std::string(), std::string("eight!  "), std::string(63, '~')})
    assert(valid_ap_passphrase(key));
  for (const auto &key :
       {std::string("short"), std::string(64, 'a'), std::string("bad\nline"),
        std::string("invalid\xc3\xa4")})
    assert(!valid_ap_passphrase(key));
  ap_node.bind("configureAccessPoint", [&](const Json &body, const Headers &,
                                           uint64_t) {
    if (!valid_ap_passphrase(body.text("psk")))
      return Response{422, Json()};
    ++applied;
    return Response{
        200,
        Json::parse(
            R"({"state":"connected","hostname":"test.local","ap_secured":true})")};
  });
  auto set_ap = [&](const std::string &body) {
    return ap_node.request("PUT", "/api/v1/wifi/ap", Json::parse(body), {}, 0);
  };
  assert(set_ap(R"({"psk":"synthetic-fixture"})").status == 200);
  assert(applied == 1);
  assert(set_ap(R"({"psk":"short"})").status == 422);
  for (auto body :
       {R"({})", R"({"psk":"synthetic-fixture","ssid":"unrelated"})"})
    assert(set_ap(body).status == 400);
  assert(applied == 1);
  assert(set_ap(R"({"psk":""})").status == 200 && applied == 2);
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

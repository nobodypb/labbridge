#include "dl24m_driver.hpp"
#include "labbridge/auth.hpp"
#include "labbridge/node.hpp"
#include <iostream>
using namespace labbridge;
struct Instrument : Endpoint {
  std::map<uint8_t, uint32_t> regs{
      {0x10, 0},    {0x11, 24000}, {0x12, 0},  {0x16, 250}, {0x17, 100},
      {0x18, 70},   {0x19, 0},     {0x21, 0},  {0x31, 1},   {0x32, 57},
      {0x33, 55},   {0x34, 5},     {0x35, 1},  {0x36, 0},   {0x37, 21000},
      {0x38, 2500}, {0x39, 15000}, {0x3a, 90}, {0x3b, 50},  {0x3c, 55}};
  Bytes rx;
  bool connected() const override { return true; }
  uint64_t generation() const override { return 1; }
  bool send(const Bytes &b) override {
    auto c = b[2];
    if (c == 2)
      regs[0x17] = dl24m::be24(b.data() + 3);
    else if (c == 1)
      regs[0x10] = b[3] ? 1 : 0;
    else if (c == 0x31 || dl24m::dl24m_read_only(c)) {
      auto v = regs[c];
      rx = {0xca, 0xcb, c,   uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0,
            0,    0xce, 0xcf};
    }
    return true;
  }
  Bytes receive() override {
    Bytes b;
    b.swap(rx);
    return b;
  }
};
int main() {
  Auth auth(
      "synthetic", Json(), [] { return std::string(64, 'a'); },
      [](const std::string &, const std::string &, unsigned) {
        return std::string(64, 'b');
      },
      [](const Json &) {}, [] { return 15000; });
  auto auth_response = auth.request("GET", "/api/v1/auth/status", Json(), {},
                                    {false, false, "http://node.local"});
  Json auth_fixture;
  auth_fixture.set("operation", "getAuthStatus")
      .set("status", double(auth_response.status))
      .set("body", auth_response.body);
  std::cout << auth_fixture.dump() << '\n';
  Instrument wire;
  Node n("fixture", "boot", "sim");
  auto d = std::make_shared<Dl24mDriver>(
      "load", "synthetic", wire, Limits{25, .2, 5, 3000, 0},
      [](const Json &) {}, Json());
  n.add("load", d);
  for (uint64_t t = 0; t < 15000; t += 5)
    n.tick(t);
  auto emit = [&](const std::string &operation, const std::string &path) {
    auto response = n.request("GET", path, Json(), {}, 15000);
    Json j;
    j.set("operation", operation)
        .set("status", double(response.status))
        .set("body", response.body);
    std::cout << j.dump() << '\n';
  };
  emit("getNode", "/api/v1/node");
  emit("listDevices", "/api/v1/devices");
  emit("getDevice", "/api/v1/devices/load");
  emit("getMeasurements", "/api/v1/devices/load/measurements");
  emit("getDl24mSettings", "/api/v1/devices/load/dl24m/settings");
  emit("getDl24mRegisters", "/api/v1/devices/load/dl24m/registers");
  emit("listComponents", "/api/v1/devices/load/components");
  emit("getSensorReadings", "/api/v1/devices/load/readings");
  emit("getComponentReading",
       "/api/v1/devices/load/components/current/measurement");
  emit("getActuatorState", "/api/v1/devices/load/components/sink/state");
}

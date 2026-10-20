#include "rd60xx_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Rtu : Endpoint {
  Bytes rx;
  std::array<uint16_t, 20> regs{};
  uint64_t gen = 1;
  unsigned enables = 0;
  Rtu() {
    regs[0] = 60241;
    regs[2] = 123;
    regs[8] = 420;
    regs[9] = 100;
  }
  bool connected() const override { return true; }
  uint64_t generation() const override { return gen; }
  bool send(const Bytes &b) override {
    if (b[1] == 3) {
      rx = {1, 3, 40};
      for (auto r : regs) {
        rx.push_back(r >> 8);
        rx.push_back(r & 255);
      }
      auto crc = rd60xx::crc(rx.data(), rx.size());
      rx.push_back(crc & 255);
      rx.push_back(crc >> 8);
    } else {
      auto r = (b[2] << 8) | b[3], v = (b[4] << 8) | b[5];
      regs[r] = v;
      if (r == 18 && v)
        ++enables;
      rx = b;
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
  Rtu wire;
  Rd60xxDriver d(
      "source", 123, wire, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json());
  for (uint64_t t = 0; t < 1000; t += 5)
    d.tick(t);
  assert(d.describe(1000).text("state") == "ready");
  d.execute("setSourceVoltage", Json::parse(R"({"voltage_v":4.1})"), 1000);
  for (uint64_t t = 1000; t < 1500; t += 5)
    d.tick(t);
  assert(wire.regs[8] == 410 &&
         d.operation(1500).text("status") == "succeeded");
  wire.regs[2] = 999;
  bool failed = false;
  d.execute("setOutput", Json::parse(R"({"enabled":true})"), 1500);
  for (uint64_t t = 1500; t < 2000; t += 5)
    d.tick(t);
  failed = d.operation(2000).text("status") == "failed";
  assert(failed && wire.enables == 0);
}

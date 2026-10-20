#include "rd60xx_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Rtu : Endpoint {
  Bytes rx;
  std::array<uint16_t, 20> regs{};
  uint64_t gen = 1;
  unsigned enables = 0;
  bool online = true;
  size_t chunk = 256;
  bool drop_on_echo = false, ignore_on = false;
  Rtu() {
    regs[0] = 60241;
    regs[2] = 123;
    regs[8] = 420;
    regs[9] = 100;
  }
  bool connected() const override { return online; }
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
      if (r == 18 && v && ignore_on)
        regs[18] = 0;
      if (r == 18 && v && drop_on_echo)
        rx.clear();
    }
    return true;
  }
  Bytes receive() override {
    auto n = std::min(chunk, rx.size());
    Bytes b(rx.begin(), rx.begin() + n);
    rx.erase(rx.begin(), rx.begin() + n);
    return b;
  }
};
int main() {
  // LB-019: a lost background read may be recovered before the first ON
  // write. Neither an uncertain ON echo nor a mismatched readback permits
  // a second activation write.
  for (int fault = 0; fault < 3; ++fault) {
    Rtu w;
    w.regs[0] = 60181;
    Rd60xxDriver source(
        "psu", 123, w, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json(), "tcp",
        6018);
    uint64_t now = 0;
    for (; now < 1000; now += 5)
      source.tick(now);
    assert(source.describe(now).text("state") == "ready");
    while (w.rx.empty()) {
      source.tick(now);
      now += 5;
    }
    if (fault == 0)
      w.rx.clear();
    w.drop_on_echo = fault == 1;
    w.ignore_on = fault == 2;
    source.execute("setOutput", Json::parse(R"({"enabled":true})"), now);
    for (auto end = now + 2500; now < end; now += 5)
      source.tick(now);
    assert(w.enables == 1);
    assert(source.operation(now).text("status") ==
           (fault == 0 ? "succeeded" : "failed"));
    if (fault != 0)
      assert(source.describe(now).text("output") == "off");
  }
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
  Rtu wifi;
  wifi.regs[0] = 60181;
  wifi.chunk = 3;
  Rd60xxDriver rd18(
      "psu", 123, wifi, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json(), "tcp",
      6018);
  for (uint64_t t = 0; t < 1000; t += 5)
    rd18.tick(t);
  assert(rd18.describe(1000).text("state") == "ready");
  rd18.execute("setSourceCurrent", Json::parse(R"({"current_a":0.15})"), 1000);
  for (uint64_t t = 1000; t < 1600; t += 5)
    rd18.tick(t);
  assert(wifi.regs[9] == 15 &&
         rd18.operation(1600).text("status") == "succeeded");
  rd18.execute("setOutput", Json::parse(R"({"enabled":true})"), 1600);
  ++wifi.gen;
  rd18.tick(1605);
  assert(rd18.operation(1605).text("status") == "failed");
  for (uint64_t t = 1610; t < 2500; t += 5)
    rd18.tick(t);
  assert(wifi.enables == 0 && rd18.describe(2500).text("output") == "off");
  Rtu second;
  second.regs[0] = 60182;
  second.regs[2] = 456;
  second.chunk = 5;
  Rd60xxDriver rd18b(
      "psu-b", 456, second, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json(),
      "tcp", 6018);
  for (uint64_t t = 2500; t < 3500; t += 5) {
    rd18.tick(t);
    rd18b.tick(t);
  }
  rd18.execute("setSourceCurrent", Json::parse(R"({"current_a":0.20})"), 3500);
  rd18b.execute("setSourceCurrent", Json::parse(R"({"current_a":0.30})"), 3500);
  for (uint64_t t = 3500; t < 4200; t += 5) {
    rd18.tick(t);
    rd18b.tick(t);
  }
  assert(wifi.regs[9] == 20 && second.regs[9] == 30);
  assert(rd18.operation(4200).text("status") == "succeeded" &&
         rd18b.operation(4200).text("status") == "succeeded");
}

#include "dl24m_driver.hpp"
#include "rd60xx_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Link : Endpoint {
  std::map<uint8_t, uint32_t> r{
      {0x10, 0}, {0x11, 4000}, {0x12, 0}, {0x17, 100}, {0x21, 0}};
  Bytes rx;
  bool connected() const { return true; }
  uint64_t generation() const { return 1; }
  bool send(const Bytes &b) {
    auto c = b[2];
    if (c == 1)
      r[0x10] = b[3] ? 1 : 0;
    else if (c == 0x31 || dl24m::dl24m_read_only(c)) {
      auto v = r[c];
      rx = {0xca, 0xcb, c,   uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0,
            0,    0xce, 0xcf};
    }
    return true;
  }
  Bytes receive() {
    Bytes b;
    b.swap(rx);
    return b;
  }
};
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
  Link link;
  Dl24mDriver d(
      "load", "test", link, {5, .2, 1, 3000, 0}, [](const Json &) {}, Json());
  for (uint64_t now = 0; now < 1000; now += 5)
    d.tick(now);
  d.execute("setOutput",
            Json::parse(R"({"enabled":true,"max_duration_ms":500})"), 1000);
  for (uint64_t now = 1000; now < 1400; now += 5)
    d.tick(now);
  assert(link.r[0x10] == 1);
  for (uint64_t now = 1400; now < 1800; now += 5)
    d.tick(now);
  assert(link.r[0x10] == 0);
  Rtu source;
  Rd60xxDriver rd(
      "source", 123, source, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json());
  for (uint64_t now = 0; now < 1000; now += 5)
    rd.tick(now);
  rd.execute("setOutput",
             Json::parse(R"({"enabled":true,"max_duration_ms":500})"), 1000);
  for (uint64_t now = 1000; now < 1400; now += 5)
    rd.tick(now);
  assert(source.regs[18] == 1);
  for (uint64_t now = 1400; now < 1800; now += 5)
    rd.tick(now);
  assert(source.regs[18] == 0);
}

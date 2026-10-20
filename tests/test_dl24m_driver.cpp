#include "dl24m_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Wire : Endpoint {
  std::map<int, uint32_t> regs{{0x10, 0},   {0x11, 24000}, {0x12, 0},
                               {0x16, 250}, {0x17, 100},   {0x21, 0}};
  Bytes rx;
  uint64_t gen = 1;
  unsigned on = 0;
  bool connected() const override { return true; }
  uint64_t generation() const override { return gen; }
  bool send(const Bytes &b) override {
    uint8_t c = b[2];
    auto v = dl24m::be24(b.data() + 3);
    if (c == 1) {
      regs[0x10] = v ? 1 : 0;
      if (v)
        ++on;
    } else if (c == 2)
      regs[0x17] = v;
    else if (c == 6)
      regs[0x21] = v;
    else if (dl24m::dl24m_read_only(c)) {
      v = regs[c];
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
  Wire w;
  bool pending = false;
  auto save = [&](const Json &j) { pending = j.boolean("stop_pending"); };
  Dl24mDriver d("load", "test", w, {25, .2, 5, 3000, 0}, save, Json());
  for (uint64_t t = 0; t < 1000; t += 5)
    d.tick(t);
  assert(d.describe(1000).text("state") == "ready");
  d.execute("setLoadCurrent", Json::parse(R"({"current_a":0.15})"), 1000);
  for (uint64_t t = 1000; t < 1500; t += 5)
    d.tick(t);
  assert(d.operation(1500).text("status") == "succeeded" &&
         w.regs[0x17] == 150);
  d.execute("setOutput", Json::parse(R"({"enabled":true})"), 1500);
  assert(pending);
  for (uint64_t t = 1500; t < 1600; t += 5)
    d.tick(t);
  assert(w.on == 1);
  ++w.gen;
  for (uint64_t t = 1600; t < 2000; t += 5)
    d.tick(t);
  assert(w.on == 1 && w.regs[0x10] == 0 && !pending);
  d.stop(2000);
  for (uint64_t t = 2000; t < 2200; t += 5)
    d.tick(t);
  assert(w.regs[0x10] == 0);
}

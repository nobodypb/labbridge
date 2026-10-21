#include "dl24m_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Wire : Endpoint {
  std::map<int, uint32_t> regs{{0x10, 0},    {0x11, 24000}, {0x12, 0},
                               {0x16, 250},  {0x17, 100},   {0x21, 0},
                               {0x37, 21000}};
  uint32_t targets[4] = {100, 500, 1200, 10};
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
    } else if (c == 2) {
      targets[regs[0x21]] = v;
      regs[0x17] = v;
    } else if (c == 0x47)
      regs[0x37] = v;
    else if (c == 6) {
      regs[0x21] = v;
      regs[0x17] = targets[v];
    } else if (c == 0x31 || dl24m::dl24m_read_only(c)) {
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
  {
    Wire delayed;
    Dl24mDriver checked(
        "fixture", "synthetic", delayed, {25, .2, 5, 150, 0},
        [](const Json &) {}, Json());
    uint64_t now = 0;
    for (; now < 1000; now += 5)
      checked.tick(now);
    checked.execute("setDl24mSettings", Json::parse(R"({"overvoltage_v":19})"),
                    now);
    for (; now < 2500; now += 5) {
      checked.tick(now);
      auto result = checked.operation(now);
      if (result.text("status") == "succeeded") {
        assert(result.text("final_output") == "off");
        break;
      }
    }
    assert(checked.operation(now).text("status") == "succeeded");
  }
  Wire w;
  bool pending = false;
  uint32_t snapshot_wire = 0;
  auto save = [&](const Json &j) {
    pending = j.boolean("stop_pending");
    auto a = j.at("restore");
    if (cJSON_IsArray(a) && a->child) {
      Json r(cJSON_Duplicate(a->child, true));
      snapshot_wire = r.number("wire");
    }
  };
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
  for (uint64_t t = 1500; t < 1750; t += 5) {
    d.tick(t);
    if (d.operation(t).text("status") == "succeeded") {
      assert(d.operation(t).text("final_output") == "on");
      break;
    }
  }
  assert(d.operation(1750).text("status") == "succeeded");
  assert(w.on == 1);
  assert(d.operation(1750).text("final_output") == "on");
  ++w.gen;
  for (uint64_t t = 1750; t < 2000; t += 5)
    d.tick(t);
  assert(w.on == 1 && w.regs[0x10] == 0 && !pending);
  d.stop(2000);
  assert(d.describe(2000).text("state") == "busy");
  for (uint64_t t = 2000; t < 2200; t += 5)
    d.tick(t);
  assert(w.regs[0x10] == 0);
  for (uint64_t t = 2200; t < 7000; t += 5)
    d.tick(t);
  w.regs[0x37] = 20000; // Changed at front panel after the cached read.
  d.execute("setDl24mSettings", Json::parse(R"({"overvoltage_v":19})"), 7000);
  for (uint64_t t = 7000; t < 8000; t += 5)
    d.tick(t);
  assert(snapshot_wire == 20000);
  assert(w.regs[0x37] == 19000);
  auto cc_before_banks = w.regs[0x17];
  d.execute("readDl24mBankTarget", Json::parse(R"({"bank":1})"), 8000);
  for (uint64_t t = 8000; t < 9500; t += 5)
    d.tick(t);
  assert(d.operation(9500).text("status") == "succeeded");
  assert(d.operation(9500).number("confirmed_value") == 500);
  d.execute("setDl24mBankTarget", Json::parse(R"({"bank":1,"target_raw":750})"),
            9500);
  for (uint64_t t = 9500; t < 11000; t += 5)
    d.tick(t);
  assert(d.operation(11000).text("status") == "succeeded" &&
         w.targets[1] == 750);
  assert(w.regs[0x21] == 0 && w.regs[0x17] == cc_before_banks &&
         w.regs[0x10] == 0);
  d.execute("setDl24mMode", Json::parse(R"({"mode":"cr"})"), 11000);
  for (uint64_t t = 11000; t < 13000; t += 5)
    d.tick(t);
  assert(d.operation(13000).text("status") == "succeeded" &&
         w.regs[0x21] == 2 && w.regs[0x10] == 0);
  d.execute("setDl24mTarget",
            Json::parse(R"({"mode":"cr","value":240,"unit":"Ohm"})"), 13000);
  for (uint64_t t = 13000; t < 14000; t += 5)
    d.tick(t);
  assert(d.operation(14000).text("status") == "succeeded" &&
         w.regs[0x17] == 2400);
  auto m = d.measurements(14000);
  assert(m.text("load_mode") == "cr" &&
         m.item("load_target").number("value") == 240);
  d.execute("setOutput",
            Json::parse(R"({"enabled":true,"max_duration_ms":300})"), 14000);
  for (uint64_t t = 14000; t < 16000; t += 5)
    d.tick(t);
  assert(w.on == 2 && w.regs[0x10] == 0);
  d.execute("setDl24mMode", Json::parse(R"({"mode":"cc"})"), 16000);
  for (uint64_t t = 16000; t < 17500; t += 5)
    d.tick(t);
  assert(w.regs[0x21] == 0 && w.regs[0x17] == cc_before_banks);
}

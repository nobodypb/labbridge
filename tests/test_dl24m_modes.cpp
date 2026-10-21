#include "dl24m_driver.hpp"
#include <cassert>
#include <cstdio>
using namespace labbridge;
struct ModeWire : Endpoint {
  uint32_t targets[4]{100, 500, 2400, 10};
  std::map<uint8_t, uint32_t> regs{{0x10, 0},    {0x11, 24000}, {0x12, 0},
                                   {0x18, 1200}, {0x21, 0},     {0x38, 200},
                                   {0x39, 5000}};
  Bytes rx;
  bool online = true, drop = false;
  unsigned reject_mode = 9, on = 0, writes = 0;
  uint64_t gen = 1;
  bool connected() const override { return online; }
  uint64_t generation() const override { return gen; }
  bool send(const Bytes &b) override {
    auto c = b[2];
    auto v = dl24m::be24(b.data() + 3);
    if (c < 0x10 || c >= 0x40) {
      ++writes;
      if (c == 6 && v != reject_mode)
        regs[0x21] = v;
      if (c == 2)
        targets[regs[0x21]] = v;
      if (c == 1) {
        regs[0x10] = v ? 1 : 0;
        if (v)
          ++on;
      }
    } else if (!drop) {
      v = c == 0x17 ? targets[regs[0x21]] : regs[c];
      rx = {0xca, 0xcb, c,   uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0,
            0,    0xce, 0xcf};
    }
    return true;
  }
  Bytes receive() override {
    Bytes result;
    result.swap(rx);
    return result;
  }
};
struct Bench {
  ModeWire wire;
  Json journal;
  uint64_t now = 0;
  Dl24mDriver driver{"test",
                     "synthetic",
                     wire,
                     {25, .2, 5, 3000, 0},
                     [this](const Json &j) { journal = j; },
                     Json()};
  void advance(unsigned ms = 1500) {
    auto end = now + ms;
    for (; now < end; now += 5)
      driver.tick(now);
  }
  void command(const char *op, const char *json) {
    driver.execute(op, Json::parse(json), now);
    advance();
    if (driver.operation(now).text("status") != "succeeded")
      fprintf(stderr, "%s %s: %s\n", op, json,
              driver.operation(now).dump().c_str());
    assert(driver.operation(now).text("status") == "succeeded");
    auto expected = driver.Driver::actuator_settings("sink", now);
    auto settings = driver.execute("getDl24mSettings", Json(), now);
    for (auto key : {"cutoff_voltage_v", "time_limit_s"})
      if (settings.has(key))
        expected.set(key, settings.number(key));
    assert(expected.canonical_dump() ==
           driver.actuator_settings("sink", now).canonical_dump());
  }
  void rejected(const char *op, const char *json) {
    auto before = wire.writes;
    bool rejected = false;
    try {
      driver.execute(op, Json::parse(json), now);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected && wire.writes == before);
  }
};
int main() {
  Bench b;
  b.advance();
  b.command("setDl24mMode", R"({"mode":"cv"})");
  b.command("setDl24mTarget", R"({"mode":"cv","value":23.9,"unit":"V"})");
  assert(b.wire.targets[1] == 2390 &&
         b.driver.measurements(b.now).item("load_target").number("value") ==
             23.9);
  b.rejected("setDl24mTarget", R"({"mode":"cv","value":1.001,"unit":"V"})");
  b.rejected("setDl24mTarget", R"({"mode":"cv","value":1,"unit":"A"})");
  b.rejected(
      "setDl24mTarget",
      R"({"mode":"cp","value":1,"unit":"W"})"); // Mode mismatch must reject
                                                // before any device mutation.
  b.command("setOutput", R"({"enabled":true})");
  b.rejected("setDl24mMode", R"({"mode":"cc"})");
  b.wire.regs[0x12] = 201;
  b.advance();
  assert(b.wire.regs[0x10] == 0); // Local current guard applies in CV.
  b.wire.regs[0x12] = 0;
  b.advance();
  b.command("setDl24mMode", R"({"mode":"cp"})");
  b.command("setDl24mTarget", R"({"mode":"cp","value":2.4,"unit":"W"})");
  assert(b.wire.targets[3] == 24);
  b.wire.regs[0x18] = 700;
  b.advance();
  b.rejected("setOutput", R"({"enabled":true})"); // 2.4 W / 7 V exceeds 0.2 A.
  b.wire.regs[0x18] = 1200;
  b.advance();
  b.driver.execute("setOutput",
                   Json::parse(R"({"enabled":true,"max_duration_ms":300})"),
                   b.now);
  b.advance();
  assert(b.wire.regs[0x10] == 0);
  b.command("setDl24mMode", R"({"mode":"cc"})");
  b.wire.reject_mode = 1;
  b.driver.execute("setDl24mMode", Json::parse(R"({"mode":"cv"})"), b.now);
  b.advance(8000);
  assert(b.driver.operation(b.now).text("status") == "failed");
  assert(b.wire.regs[0x21] == 0 && b.wire.targets[0] == 100 &&
         b.wire.regs[0x10] == 0);
  assert(cJSON_GetArraySize(b.journal.at("restore")) == 0);
  b.wire.reject_mode = 9;
  b.advance();
  b.driver.execute("setDl24mMode", Json::parse(R"({"mode":"cr"})"), b.now);
  for (unsigned n = 0;
       n < 500 && cJSON_GetArraySize(b.journal.at("restore")) == 0; ++n)
    b.advance(5);
  assert(cJSON_GetArraySize(b.journal.at("restore")) == 2);
  auto saved = b.journal;
  // Simulated ESP restart after rollback was persisted, before checked success.
  Dl24mDriver rebooted(
      "test", "synthetic", b.wire, {25, .2, 5, 3000, 0}, [](const Json &) {},
      saved);
  for (unsigned n = 0; n < 2000; ++n)
    rebooted.tick(b.now + n * 5);
  assert(b.wire.regs[0x21] == 0 && b.wire.targets[0] == 100 &&
         b.wire.regs[0x10] == 0);
}

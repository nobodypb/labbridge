#include "dl24m_driver.hpp"
#include <cassert>
using namespace labbridge;
struct PassiveWire : Endpoint {
  unsigned sends = 0;
  Bytes rx;
  std::map<uint8_t, uint32_t> values{
      {0x10, 0},   {0x11, 12000}, {0x12, 0},   {0x13, 0x010203}, {0x14, 123},
      {0x15, 456}, {0x16, 251},   {0x17, 100}, {0x21, 0},        {0x36, 2512}};
  bool connected() const override { return true; }
  uint64_t generation() const override { return 1; }
  bool send(const Bytes &b) override {
    ++sends;
    auto c = b[2];
    if (c == 1)
      values[0x10] = 0;
    else if (dl24m::dl24m_read_only(c) || c == 0x31) {
      auto v = values[c];
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
int main() {
  PassiveWire wire;
  Dl24mDriver driver(
      "test", "synthetic", wire, {25, .2, 5, 3000, 0}, [](const Json &) {},
      Json());
  for (unsigned t = 0; t < 30000; t += 5)
    driver.tick(t);
  assert(driver.components(30000).canonical_dump() ==
         driver.Driver::components(30000).canonical_dump());
  assert(driver.actuator_state("sink", 30000).canonical_dump() ==
         driver.Driver::actuator_state("sink", 30000).canonical_dump());
  auto m = driver.measurements(30000), readings = m.item("native_readings");
  assert(cJSON_IsArray(readings.get()));
  bool elapsed = false, raw = false, fine_temperature = false;
  for (auto p = readings.get()->child; p; p = p->next) {
    Json r(cJSON_Duplicate(p, true));
    if (r.text("id") == "elapsed") {
      assert(r.text("unit") == "s" && r.number("value") == 3723);
      elapsed = true;
    }
    if (r.text("id") == "temperature_magnitude") {
      assert(r.text("unit") == "degC" && r.number("value") == 25.12);
      assert(r.text("origin") == "dl24m.ble.36.magnitude");
      assert(r.text("quality") == "valid");
      fine_temperature = true;
    }
    if (r.text("id") == "capacity") {
      assert(r.text("unit") == "Ah" && r.number("value") == .123);
      raw = true;
    }
  }
  assert(elapsed && raw && fine_temperature &&
         m.number("capacity_ah") == .123 && m.number("energy_wh") == .456);
  auto charge = driver.component_reading("charge", 30000);
  assert(charge.number("value") == .123 && charge.text("unit") == "Ah" &&
         charge.text("quality") == "valid");
  auto aged_charge = driver.component_reading("charge", 40000);
  assert(aged_charge.text("quality") == "stale" &&
         aged_charge.number("received_ms") == charge.number("received_ms"));
  auto stale = driver.measurements(40000);
  assert(!stale.has("capacity_ah") && !stale.has("energy_wh"));
  assert(driver.supports("getDl24mDiagnostics"));
  auto count = wire.sends;
  auto diag = driver.execute("getDl24mDiagnostics", Json(), 30000);
  assert(wire.sends ==
         count); // Read-only access must not create instrument traffic.
  assert(cJSON_GetArraySize(diag.at("entries")) <= 32 &&
         diag.number("dropped") > 0);
  auto entry = Json(cJSON_Duplicate(diag.at("entries")->child, true));
  assert(entry.text("direction") == "tx" || entry.text("direction") == "rx");
  assert(!entry.text("hex").empty());
  auto features = driver.describe(30000).item("features");
  assert(cJSON_IsArray(features.get()));
  unsigned blocked_maintenance = 0;
  for (auto p = features.get()->child; p; p = p->next) {
    Json f(cJSON_Duplicate(p, true));
    if (f.text("id") == "calibration" || f.text("id") == "zero" ||
        f.text("id") == "factory-reset") {
      assert(f.text("status") == "unavailable" && !f.has("operation"));
      assert(f.text("reason").find("native protocol") != std::string::npos);
      assert(f.boolean("requires_off") && f.boolean("requires_lease"));
      ++blocked_maintenance;
    }
  }
  assert(blocked_maintenance == 3);
  unsigned active_modes = 0;
  for (auto p = features.get()->child; p; p = p->next) {
    Json f(cJSON_Duplicate(p, true));
    if (f.text("id") == "cv" || f.text("id") == "cr" || f.text("id") == "cp") {
      assert(f.text("status") == "supported" && !f.text("unit").empty());
      assert(f.text("operation") == "setDl24mMode");
      assert(f.boolean("requires_off") && f.boolean("requires_lease"));
      ++active_modes;
    }
  }
  assert(active_modes == 3);
  assert(driver.stream_details(30000).item("settings").number("mode") == 0);
}

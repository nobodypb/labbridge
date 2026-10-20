#include "ina228.hpp"
#include "meter_driver.hpp"
#include "yocto_watt.hpp"
#include <cassert>
#include <cmath>
using namespace labbridge;
struct Registers : ina228::RegisterIo {
  std::map<uint8_t, uint64_t> r{{0x3e, 0x5449},
                                {0x3f, 0x2281},
                                {0xb, 3},
                                {4, 0x1000},
                                {5, uint64_t(20480) << 4},
                                {6, 3200},
                                {7, uint64_t(0xfffff) << 4},
                                {8, 10000},
                                {9, 100},
                                {10, 0xffffffffff}};
  bool read(uint8_t reg, uint8_t *out, size_t n) {
    auto v = r[reg];
    for (size_t k = 0; k < n; ++k)
      out[n - 1 - k] = v >> (8 * k);
    return true;
  }
  bool write(uint8_t reg, uint16_t value) {
    r[reg] = value;
    return true;
  }
};
int main() {
  assert(yocto_watt::valid_serial("YWATMK2C-123456"));
  assert(!yocto_watt::valid_serial("YWATMK2C-../api"));
  Registers io;
  ina228::Reader reader(io, {.01, 2, 0});
  assert(reader.initialize());
  auto baseline = reader.sample();
  assert(baseline.values.at("energy") == 0 &&
         baseline.values.at("charge") == 0);
  io.r[9] += 100;
  io.r[10] -= 1;
  auto frame = reader.sample();
  assert(frame.status == ina228::Status::valid);
  assert(std::abs(frame.values.at("voltage") - 4) < 1e-9);
  assert(frame.values.at("current") < 0 && frame.values.at("charge") < 0);
  assert(frame.values.at("temperature-die") == 25);
  io.r[0xb] = 1;
  assert(reader.sample().status == ina228::Status::waiting);
  io.r[0xb] = 0x203;
  assert(reader.sample().status == ina228::Status::fault);
  io.r[0x3f] = 0x2291;
  assert(!reader.initialize());
  bool invalid = false;
  try {
    ina228::Calibration{.1, 10, 1}.validate();
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  assert(invalid);
  MeterDriver d(
      "meter", "ina228", "i2c", "64",
      Json::parse(
          R"({"voltage_v":5,"current_a":1,"power_w":5,"max_age_ms":3000})"),
      ina228::channels());
  d.publish(frame.values, 100);
  d.stop(100);
  assert(d.operation(100).text("status") == "succeeded");
  assert(d.component_reading("current", 101).number("value") < 0);
  assert(d.component_reading("voltage", 4001).text("quality") == "stale");
  assert(d.component_reading("temperature-die", 101).text("quantity") ==
         "temperature");
  auto raw = Json::parse(
      R"({"module":{"serialNumber":"YWATMK2C-123456","productId":228,"upTime":100},"voltage1":{"unit":"V DC","currentValue":262144,"sensorState":0},"current1":{"unit":"mA DC","currentValue":-6553600,"sensorState":0},"power":{"unit":"W","currentValue":26214,"sensorState":0}})");
  auto y = yocto_watt::decode(raw, "YWATMK2C-123456");
  assert(y.at("current") == -.1 && y.at("voltage") == 4);
  raw.item("module");
  invalid = false;
  try {
    yocto_watt::decode(raw, "YWATMK2C-wrong");
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  assert(invalid);
}

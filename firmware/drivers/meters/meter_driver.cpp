#include "meter_driver.hpp"
#include <cmath>
namespace labbridge {
MeterDriver::MeterDriver(std::string id, std::string driver,
                         std::string transport, std::string identity,
                         Json limits, std::vector<SensorSpec> channels)
    : id_(std::move(id)), driver_(std::move(driver)),
      transport_(std::move(transport)), identity_(std::move(identity)),
      limits_(std::move(limits)), channels_(std::move(channels)) {}
void MeterDriver::publish(std::map<std::string, double> values, uint64_t now) {
  for (auto &v : values)
    if (!std::isfinite(v.second))
      throw std::invalid_argument("Non-finite measurement");
  std::lock_guard<std::mutex> lock(mutex_);
  values_ = std::move(values);
  received_ = now;
  ++sequence_;
  healthy_ = true;
}
void MeterDriver::advance_epoch() {
  std::lock_guard<std::mutex> lock(mutex_);
  ++epoch_;
  healthy_ = false;
}
void MeterDriver::invalidate() {
  std::lock_guard<std::mutex> lock(mutex_);
  healthy_ = false;
}
Json MeterDriver::components(uint64_t) {
  auto list = Json::array();
  for (auto &c : channels_) {
    Json j;
    j.set("id", c.id)
        .set("kind", "sensor")
        .set("actions", Json::array())
        .set("quantity", c.quantity)
        .set("unit", c.unit);
    if (!c.temperature_kind.empty())
      j.set("temperature_kind", c.temperature_kind);
    list.append(j);
  }
  return list;
}
Json MeterDriver::sensor_readings(uint64_t now) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto list = Json::array();
  for (auto &c : channels_)
    list.append(reading_unlocked(c.id, now));
  return list;
}
Json MeterDriver::component_reading(const std::string &id, uint64_t now) {
  std::lock_guard<std::mutex> lock(mutex_);
  return reading_unlocked(id, now);
}
Json MeterDriver::reading_unlocked(const std::string &id, uint64_t now) {
  for (auto &c : channels_)
    if (c.id == id) {
      auto value = values_.find(id);
      Json j;
      j.set("device_id", id_)
          .set("component_id", id)
          .set("epoch", std::to_string(epoch_))
          .set("quantity", c.quantity)
          .set("unit", c.unit)
          .set("sequence", double(sequence_))
          .set("received_ms", double(received_))
          .set("age_ms", double(now >= received_ ? now - received_ : 0))
          .set("quality",
               !healthy_ || value == values_.end() ? "unavailable"
               : now < received_ ||
                       now - received_ > limits_.number("max_age_ms", 3000)
                   ? "stale"
                   : "valid");
      if (value != values_.end())
        j.set("value", value->second);
      return j;
    }
  throw std::invalid_argument("Unknown sensor component");
}
Json MeterDriver::measurements(uint64_t now) {
  std::lock_guard<std::mutex> lock(mutex_);
  Json m;
  auto temps = Json::array();
  m.set("device_id", id_);
  bool valid = true;
  uint64_t sequence = 0, received = 0, age = 0;
  for (auto &c : channels_) {
    auto reading = reading_unlocked(c.id, now);
    sequence = reading.number("sequence");
    received = reading.number("received_ms");
    age = reading.number("age_ms");
    if (c.id == "voltage" || c.id == "current" || c.id == "power") {
      valid &= reading.text("quality") == "valid";
      if (reading.has("value"))
        m.set(c.id == "voltage"   ? "voltage_v"
              : c.id == "current" ? "current_a"
                                  : "power_w",
              reading.number("value"));
    }
    if (c.id == "energy" && reading.has("value"))
      m.set("energy_wh", reading.number("value"));
    if (c.id == "charge" && reading.has("value"))
      m.set("capacity_ah", reading.number("value"));
    if (c.quantity == "temperature") {
      Json t;
      t.set("id", c.id)
          .set("kind", c.temperature_kind)
          .set("quality", reading.text("quality"))
          .set("received_ms", reading.number("received_ms"))
          .set("age_ms", reading.number("age_ms"));
      if (reading.has("value"))
        t.set("value_c", reading.number("value"));
      temps.append(t);
    }
  }
  m.set("sequence", double(sequence))
      .set("received_ms", double(received))
      .set("age_ms", double(age))
      .set("quality", valid ? "valid" : "unavailable")
      .set("temperature_channels", temps);
  return m;
}
Json MeterDriver::operation(uint64_t) {
  return Json::parse(
      R"({"status":"succeeded","stage":"read_only","read_attempts":0,"write_attempts":0,"reconnects":0,"duration_ms":0,"final_output":"off","restore_pending":false})");
}
Json MeterDriver::describe(uint64_t now) {
  auto m = measurements(now), caps = Json::array();
  for (auto s : {"voltage.measure", "current.measure", "power.measure"})
    caps.append(std::string(s));
  Json d;
  d.set("id", id_)
      .set("driver", driver_)
      .set("transport", transport_)
      .set("identity", identity_)
      .set("capabilities", caps)
      .set("state", m.text("quality") == "valid" ? "ready" : "unavailable")
      .set("output", "off");
  if (limits_.has("voltage_v"))
    d.set("limits", limits_);
  return d;
}
} // namespace labbridge

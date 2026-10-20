#include "labbridge/node.hpp"
#include <algorithm>
#include <tuple>
namespace labbridge {
namespace {
bool contains(const Json &array, const std::string &s) {
  auto a = array.get();
  if (!cJSON_IsArray(a))
    return false;
  for (auto p = a->child; p; p = p->next)
    if (cJSON_IsString(p) && s == p->valuestring)
      return true;
  return false;
}
Json descriptor(const std::string &id, const std::string &kind) {
  Json j;
  j.set("id", id).set("kind", kind).set("actions", Json::array());
  return j;
}
} // namespace
Json Driver::components(uint64_t now) {
  auto result = Json::array(), d = describe(now), m = measurements(now),
       caps = d.item("capabilities");
  auto actor = [&](const char *id, const char *kind, bool present) {
    if (!present)
      return;
    auto c = descriptor(id, kind), actions = Json::array();
    if (supports(std::string(kind) == "source" ? "setSourceCurrent"
                                               : "setLoadCurrent"))
      actions.append(std::string("current"));
    if (std::string(kind) == "source" && supports("setSourceVoltage"))
      actions.append(std::string("voltage"));
    if (supports("setOutput"))
      actions.append(std::string("output"));
    if (std::string(kind) == "sink" && supports("setSinkCutoff"))
      actions.append(std::string("cutoff"));
    if (std::string(kind) == "sink" && supports("setSinkDuration"))
      actions.append(std::string("duration"));
    c.set("actions", actions);
    result.append(c);
  };
  actor("source", "source", contains(caps, "source.cc_cv"));
  actor("sink", "sink", contains(caps, "load.cc") || contains(caps, "sink.cc"));
  for (auto entry : {std::make_tuple("voltage", "V", "voltage_v"),
                     std::make_tuple("current", "A", "current_a"),
                     std::make_tuple("power", "W", "power_w"),
                     std::make_tuple("energy", "Wh", "energy_wh"),
                     std::make_tuple("charge", "Ah", "capacity_ah")}) {
    auto id = std::get<0>(entry);
    if (m.has(std::get<2>(entry)) ||
        contains(caps, std::string(id) + ".measure")) {
      auto c = descriptor(id, "sensor");
      c.set("quantity", id).set("unit", std::get<1>(entry));
      result.append(c);
    }
  }
  auto a = m.at("temperature_channels");
  if (cJSON_IsArray(a))
    for (auto p = a->child; p; p = p->next) {
      Json t(cJSON_Duplicate(p, true));
      auto c = descriptor("temperature-" + t.text("id"), "sensor");
      c.set("quantity", "temperature")
          .set("unit", "degC")
          .set("temperature_kind", t.text("kind"));
      result.append(c);
    }
  return result;
}
Json Driver::sensor_readings(uint64_t now) {
  auto readings = Json::array(), list = components(now);
  for (auto p = list.get()->child; p; p = p->next) {
    Json c(cJSON_Duplicate(p, true));
    if (c.text("kind") == "sensor")
      readings.append(component_reading(c.text("id"), now));
  }
  return readings;
}
Json Driver::component_reading(const std::string &id, uint64_t now) {
  auto m = measurements(now);
  Json source = m;
  std::string quantity, unit, key;
  if (id.rfind("temperature-", 0) == 0) {
    auto a = m.at("temperature_channels");
    bool found = false;
    if (cJSON_IsArray(a))
      for (auto p = a->child; p; p = p->next) {
        Json t(cJSON_Duplicate(p, true));
        if (id == "temperature-" + t.text("id")) {
          source = t;
          found = true;
          break;
        }
      }
    if (!found)
      throw std::invalid_argument("Unknown temperature component");
    quantity = "temperature";
    unit = "degC";
    key = "value_c";
  } else {
    const std::map<std::string, std::pair<std::string, std::string>> fields = {
        {"voltage", {"voltage_v", "V"}},
        {"current", {"current_a", "A"}},
        {"power", {"power_w", "W"}},
        {"energy", {"energy_wh", "Wh"}},
        {"charge", {"capacity_ah", "Ah"}}};
    auto it = fields.find(id);
    if (it == fields.end())
      throw std::invalid_argument("Unknown sensor component");
    quantity = id;
    key = it->second.first;
    unit = it->second.second;
  }
  Json j;
  j.set("component_id", id)
      .set("device_id", m.text("device_id"))
      .set("quantity", quantity)
      .set("unit", unit)
      .set("quality", source.has(key) ? source.text("quality", "unavailable")
                                      : "unavailable")
      .set("sequence", m.number("sequence"))
      .set("received_ms", source.number("received_ms"))
      .set("age_ms", source.number("age_ms"));
  if (source.has(key))
    j.set("value", source.number(key));
  return j;
}
Json Driver::actuator_state(const std::string &id, uint64_t now) {
  auto d = describe(now);
  Json s;
  s.set("state", d.text("state"))
      .set("output", d.text("output"))
      .set("settings", actuator_settings(id, now));
  return s;
}
Json Driver::actuator_settings(const std::string &, uint64_t now) {
  auto m = measurements(now);
  Json s;
  if (m.has("set_current_a"))
    s.set("current_a", m.number("set_current_a"));
  if (m.has("set_voltage_v"))
    s.set("voltage_v", m.number("set_voltage_v"));
  if (m.has("regulation_mode"))
    s.set("regulation_mode", m.text("regulation_mode"));
  return s;
}
std::string Driver::component_command(const std::string &id,
                                      const std::string &action, uint64_t now) {
  auto list = components(now);
  auto a = list.get();
  for (auto p = a->child; p; p = p->next) {
    Json c(cJSON_Duplicate(p, true));
    if (c.text("id") != id)
      continue;
    if (action == "stop")
      return "stopDevice";
    if (!contains(c.item("actions"), action))
      throw std::invalid_argument("Component action unavailable");
    if (action == "output")
      return "setOutput";
    if (action == "current")
      return c.text("kind") == "source" ? "setSourceCurrent" : "setLoadCurrent";
    if (action == "voltage")
      return "setSourceVoltage";
    if (action == "cutoff")
      return "setSinkCutoff";
    if (action == "duration")
      return "setSinkDuration";
  }
  throw std::invalid_argument("Unknown component action");
}
} // namespace labbridge

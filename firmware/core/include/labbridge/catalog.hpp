#pragma once
#include "node.hpp"
#include "resources.hpp"
#include <cmath>
#include <set>
namespace labbridge {
class Catalog {
public:
  using Factory = std::function<std::shared_ptr<Driver>(const Json &)>;
  void register_module(const std::string &id, std::set<std::string> transports,
                       Factory factory) {
    if (id.empty() || !factory || modules_.count(id))
      throw ConfigurationError("Invalid/duplicate module");
    modules_.emplace(id, Module{std::move(transports), std::move(factory)});
  }
  Registry validate(const Json &configuration, BoardProfile board) const {
    Registry registry(std::move(board));
    auto a = configuration.at("devices");
    if (!cJSON_IsArray(a))
      throw ConfigurationError("Devices must be an array");
    std::map<std::string, Json> configs;
    for (auto p = a->child; p; p = p->next) {
      Json j(cJSON_Duplicate(p, true));
      DeviceSpec d{j.text("id"),
                   j.text("driver"),
                   j.text("transport"),
                   j.text("endpoint"),
                   {}};
      auto m = modules_.find(d.driver);
      if (m == modules_.end() || !m->second.transports.count(d.transport))
        throw ConfigurationError(
            "Unknown module or unsupported module transport");
      auto pins = j.at("pins");
      if (cJSON_IsArray(pins))
        for (auto pin = pins->child; pin; pin = pin->next)
          d.pins.push_back(pin->valueint);
      registry.add(d);
      configs.emplace(d.id, j);
    }
    for (auto &p : configs)
      if (p.second.has("cell_temperature")) {
        auto g = p.second.item("cell_temperature");
        auto sensor = configs.find(g.text("sensor_device_id"));
        if (sensor == configs.end() || g.text("channel_id").empty())
          throw ConfigurationError("Unknown cell sensor/channel");
        auto minimum = g.number("minimum_c"), maximum = g.number("maximum_c");
        auto age = g.number("max_age_ms", 3000);
        if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
            minimum >= maximum || !std::isfinite(age) || age < 100 ||
            age > 3000 || std::floor(age) != age)
          throw ConfigurationError("Invalid cell guard limits");
      }
    return registry;
  }
  void instantiate(Node &node, const Json &configuration,
                   BoardProfile board) const {
    auto registry = validate(configuration, std::move(board));
    (void)registry;
    std::vector<std::pair<std::string, Json>> guards;
    auto a = configuration.at("devices");
    for (auto p = a->child; p; p = p->next) {
      Json j(cJSON_Duplicate(p, true));
      auto d = modules_.at(j.text("driver")).factory(j);
      if (!d)
        throw ConfigurationError("Driver factory returned null");
      node.add(j.text("id"), std::move(d));
      if (j.has("cell_temperature"))
        guards.emplace_back(j.text("id"), j.item("cell_temperature"));
    }
    for (auto &p : guards) {
      auto &g = p.second;
      node.cell_guard(p.first,
                      {g.text("sensor_device_id"), g.text("channel_id"),
                       g.number("minimum_c"), g.number("maximum_c"),
                       uint64_t(g.number("max_age_ms", 3000))});
    }
  }

private:
  struct Module {
    std::set<std::string> transports;
    Factory factory;
  };
  std::map<std::string, Module> modules_;
};
} // namespace labbridge

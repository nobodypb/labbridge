#pragma once
#include "labbridge/node.hpp"
#include <mutex>
namespace labbridge {
struct SensorSpec {
  std::string id, quantity, unit, temperature_kind;
};
class MeterDriver : public Driver {
  std::string id_, driver_, transport_, identity_;
  Json limits_;
  std::vector<SensorSpec> channels_;
  std::map<std::string, double> values_;
  std::mutex mutex_;
  uint64_t received_ = 0, sequence_ = 0, epoch_ = 1;
  bool healthy_ = false;

  Json reading_unlocked(const std::string &, uint64_t);

public:
  MeterDriver(std::string id, std::string driver, std::string transport,
              std::string identity, Json limits,
              std::vector<SensorSpec> channels);
  void publish(std::map<std::string, double> values, uint64_t now);
  void invalidate();
  void advance_epoch();
  Json describe(uint64_t) override;
  Json measurements(uint64_t) override;
  Json sensor_readings(uint64_t) override;
  Json components(uint64_t) override;
  Json component_reading(const std::string &, uint64_t) override;
  bool supports(const std::string &) const override { return false; }
  void tick(uint64_t) override{};
  void stop(uint64_t) override{};
  Json execute(const std::string &, const Json &, uint64_t) override {
    throw std::invalid_argument("Read-only meter");
  }
  Json operation(uint64_t) override;
  bool safe_to_reconfigure(uint64_t) override { return true; }
};
} // namespace labbridge

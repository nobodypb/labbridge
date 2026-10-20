#pragma once
#include "labbridge/guard.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace labbridge::rd60xx {
using Bytes = std::vector<uint8_t>;
uint16_t crc(const uint8_t *, size_t);
Bytes read(uint8_t address, uint16_t first, uint16_t count);
Bytes write(uint8_t address, uint16_t reg, uint16_t value);
bool valid(const Bytes &, uint8_t address, uint8_t function);
inline constexpr size_t telemetry_register_count = 42;
inline constexpr size_t telemetry_frame_size = 2 * telemetry_register_count + 5;
struct AuxiliaryMeasurement {
  double input_voltage, battery_voltage, capacity, energy;
  bool battery_detected;
  std::optional<double> external_temperature;
};
struct Measurement {
  uint32_t serial;
  double set_voltage, set_current, voltage, current, power,
      internal_temperature;
  bool output, cc;
  uint16_t protection;
  std::optional<AuxiliaryMeasurement> auxiliary{};
};
struct Profile {
  unsigned model;
  double max_current, max_power;
  void validate(const Limits &) const;
};
Profile profile(unsigned model);
Measurement decode(const std::array<uint16_t, 20> &,
                   unsigned expected_model = 6024);
Measurement decode(const std::array<uint16_t, telemetry_register_count> &,
                   unsigned expected_model = 6024);
} // namespace labbridge::rd60xx

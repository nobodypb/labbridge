#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace labbridge::rd60xx {
using Bytes = std::vector<uint8_t>;
uint16_t crc(const uint8_t *, size_t);
Bytes read(uint8_t address, uint16_t first, uint16_t count);
Bytes write(uint8_t address, uint16_t reg, uint16_t value);
bool valid(const Bytes &, uint8_t address, uint8_t function);
struct Measurement {
  uint32_t serial;
  double set_voltage, set_current, voltage, current, power,
      internal_temperature;
  bool output, cc;
  uint16_t protection;
};
Measurement decode(const std::array<uint16_t, 20> &);
} // namespace labbridge::rd60xx

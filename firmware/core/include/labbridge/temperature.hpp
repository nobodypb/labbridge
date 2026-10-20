#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
namespace labbridge {
enum class TemperatureKind { internal, external_probe, cell };
struct TemperatureChannel {
  std::string id;
  TemperatureKind kind;
  double value_c;
  uint64_t received_ms;
  bool usable_as_cell(uint64_t now, uint64_t max_age) const {
    return kind != TemperatureKind::internal && std::isfinite(value_c) &&
           received_ms <= now && now - received_ms <= max_age;
  }
};
inline std::optional<double> decode_ds18b20(const std::array<uint8_t, 9> &data,
                                            bool conversion_complete) {
  if (!conversion_complete)
    return std::nullopt;
  uint8_t crc = 0;
  for (size_t i = 0; i < 8; ++i) {
    uint8_t b = data[i];
    for (int bit = 0; bit < 8; ++bit) {
      uint8_t mix = (crc ^ b) & 1;
      crc >>= 1;
      if (mix)
        crc ^= 0x8c;
      b >>= 1;
    }
  }
  if (crc != data[8])
    return std::nullopt;
  int16_t raw = static_cast<int16_t>((uint16_t(data[1]) << 8) | data[0]);
  auto resolution = (data[4] >> 5) & 3;
  raw &= static_cast<int16_t>(~((1 << (3 - resolution)) - 1));
  double value = raw / 16.0;
  if (value < -55 || value > 125)
    return std::nullopt;
  return value;
}
} // namespace labbridge

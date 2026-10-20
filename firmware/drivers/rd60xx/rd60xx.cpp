#include "rd60xx.hpp"
#include <cstddef>
#include <stdexcept>
namespace labbridge::rd60xx {
uint16_t crc(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int b = 0; b < 8; ++b)
      c = c & 1 ? (c >> 1) ^ 0xa001 : c >> 1;
  }
  return c;
}
static Bytes frame(uint8_t a, uint8_t f, uint16_t r, uint16_t v) {
  if (a < 1 || a > 247)
    throw std::invalid_argument("Modbus address");
  Bytes b{a, f, uint8_t(r >> 8), uint8_t(r), uint8_t(v >> 8), uint8_t(v)};
  auto c = crc(b.data(), b.size());
  b.push_back(c & 255);
  b.push_back(c >> 8);
  return b;
}
Bytes read(uint8_t a, uint16_t first, uint16_t n) {
  if (!n || n > 125)
    throw std::invalid_argument("Modbus count");
  return frame(a, 3, first, n);
}
Bytes write(uint8_t a, uint16_t r, uint16_t v) { return frame(a, 6, r, v); }
bool valid(const Bytes &b, uint8_t a, uint8_t f) {
  if (b.size() < 5 || b[0] != a || b[1] != f || crc(b.data(), b.size()) != 0)
    return false;
  if (f == 3)
    return b.size() == size_t(b[2]) + 5 && !(b[2] % 2);
  return f == 6 && b.size() == 8;
}
Profile profile(unsigned model) {
  if (model == 6018)
    return {6018, 18, 1080};
  if (model == 6024)
    return {6024, 24, 1440};
  throw std::invalid_argument(
      "Unsupported RD60xx model (P variants are not supported)");
}
void Profile::validate(const Limits &l) const {
  if (!l.valid() || l.voltage > 60 || l.current > max_current ||
      l.power > max_power)
    throw std::invalid_argument("RD60xx limits exceed selected model");
}
Measurement decode(const std::array<uint16_t, 20> &r, unsigned expected_model) {
  auto model = profile(expected_model);
  if (r[0] / 10 != expected_model || (expected_model == 6018 && r[0] > 60184) ||
      r[18] > 1 || r[17] > 1)
    throw std::invalid_argument("Unexpected RD60xx model/register block");
  Measurement m{(uint32_t(r[1]) << 16) | r[2],
                r[8] / 100.,
                r[9] / 100.,
                r[10] / 100.,
                r[11] / 100.,
                ((uint32_t(r[12]) << 16) | r[13]) / 100.,
                (r[4] ? -1. : 1.) * r[5],
                r[18] != 0,
                r[17] != 0,
                r[16]};
  if (m.voltage > 65 || m.current > model.max_current + 2)
    throw std::invalid_argument("RD60xx values out of range");
  return m;
}
} // namespace labbridge::rd60xx

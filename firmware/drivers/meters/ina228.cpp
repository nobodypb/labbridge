#include "ina228.hpp"
#include <cmath>
namespace labbridge::ina228 {
void Calibration::validate() const {
  if (!std::isfinite(shunt_ohms) || !std::isfinite(max_current_a) ||
      shunt_ohms <= 0 || max_current_a <= 0 || adc_range > 1 ||
      shunt_ohms * max_current_a > (adc_range ? .04096 : .16384))
    throw std::invalid_argument(
        "INA228 requires a shunt and current range within ADC full scale");
  double raw =
      13107.2e6 * (max_current_a / 524288.) * shunt_ohms * (adc_range ? 4 : 1);
  if (std::round(raw) < 1 || std::round(raw) > 32767)
    throw std::invalid_argument("INA228 calibration is not representable");
}
uint16_t Calibration::register_value() const {
  validate();
  return std::lround(13107.2e6 * (max_current_a / 524288.) * shunt_ohms *
                     (adc_range ? 4 : 1));
}
double Calibration::current_lsb() const {
  return register_value() / (13107.2e6 * shunt_ohms * (adc_range ? 4 : 1));
}
bool Reader::read(uint8_t reg, size_t size, uint64_t &value) {
  uint8_t bytes[5];
  if (size > 5 || !io_.read(reg, bytes, size))
    return false;
  value = 0;
  for (size_t i = 0; i < size; ++i)
    value = (value << 8) | bytes[i];
  return true;
}
bool Reader::initialize() {
  configured_ = false;
  uint64_t manufacturer = 0, id = 0;
  if (!read(0x3e, 2, manufacturer) || manufacturer != 0x5449 ||
      !read(0x3f, 2, id) || (id >> 4) != 0x228)
    return false;
  // No reset/calibration commands beyond the explicitly configured operating
  // scale.
  if (!io_.write(0, calibration_.adc_range ? 0x10 : 0) ||
      !io_.write(2, calibration_.register_value()) || !io_.write(1, 0xfb6a))
    return false;
  uint64_t c = 0, a = 0, s = 0;
  configured_ = read(0, 2, c) && read(1, 2, a) && read(2, 2, s) &&
                c == (calibration_.adc_range ? 0x10 : 0) && a == 0xfb6a &&
                s == calibration_.register_value();
  return configured_;
}
Sample Reader::sample() {
  Sample result;
  if (!configured_)
    return result;
  uint64_t status = 0, cal = 0, adc = 0, config = 0;
  uint64_t manufacturer = 0, device = 0;
  if (!read(0x3e, 2, manufacturer) || manufacturer != 0x5449 ||
      !read(0x3f, 2, device) || (device >> 4) != 0x228) {
    configured_ = false;
    return result;
  }
  if (!read(2, 2, cal) || !read(1, 2, adc) || !read(0, 2, config) ||
      cal != calibration_.register_value() || adc != 0xfb6a ||
      config != (calibration_.adc_range ? 0x10 : 0)) {
    configured_ = false;
    return result;
  }
  if (!read(0xb, 2, status) || !(status & 1) || (status & 0x200))
    return result;
  if (!(status & 2)) {
    result.status = Status::waiting;
    return result;
  }
  uint64_t bus = 0, shunt = 0, temp = 0, current = 0, power = 0, energy = 0,
           charge = 0;
  if (!read(4, 3, shunt) || !read(5, 3, bus) || !read(6, 2, temp) ||
      !read(7, 3, current) || !read(8, 3, power) || !read(9, 5, energy) ||
      !read(10, 5, charge))
    return result;
  auto signed_bits = [](uint64_t v, unsigned bits) {
    auto sign = uint64_t(1) << (bits - 1);
    return int64_t(v ^ sign) - int64_t(sign);
  };
  if ((shunt >> 4) == 0x7ffff || (shunt >> 4) == 0x80000 ||
      (bus >> 4) * .0001953125 > 85)
    return result;
  auto lsb = calibration_.current_lsb();
  result.values = {{"voltage", (bus >> 4) * .0001953125},
                   {"current", signed_bits(current >> 4, 20) * lsb},
                   {"power", power * 3.2 * lsb},
                   {"temperature-die", signed_bits(temp, 16) * .0078125}};
  if (!(status & 0x800))
    result.values["energy"] = energy * 51.2 * lsb / 3600.;
  if (!(status & 0x400))
    result.values["charge"] = signed_bits(charge, 40) * lsb / 3600.;
  result.status = Status::valid;
  return result;
}
std::vector<SensorSpec> channels() {
  return {{"voltage", "voltage", "V", ""},
          {"current", "current", "A", ""},
          {"power", "power", "W", ""},
          {"energy", "energy", "Wh", ""},
          {"charge", "charge", "Ah", ""},
          {"temperature-die", "temperature", "degC", "internal"}};
}
} // namespace labbridge::ina228

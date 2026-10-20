#include "yocto_watt.hpp"
#include <cmath>
namespace labbridge::yocto_watt {
bool valid_serial(const std::string &s) {
  return s.size() >= 10 && s.size() <= 25 && s.rfind("YWATMK2C-", 0) == 0 &&
         s.substr(9).find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklm"
                                       "nopqrstuvwxyz0123456789") ==
             std::string::npos;
}
std::vector<SensorSpec> channels() {
  return {{"voltage", "voltage", "V", ""},
          {"current", "current", "A", ""},
          {"power", "power", "W", ""},
          {"voltage-ac", "voltage_ac_rms", "V", ""},
          {"current-ac", "current_ac_rms", "A", ""},
          {"power-factor", "power_factor", "1", ""}};
}
std::map<std::string, double> decode(const Json &api,
                                     const std::string &serial) {
  auto module = api.item("module");
  if (!valid_serial(serial) || module.text("serialNumber") != serial ||
      module.number("productId") != 228)
    throw std::invalid_argument("Yocto-Watt identity mismatch");
  std::map<std::string, double> result;
  auto sensor = [&](const char *name, const char *id, const std::string &unit,
                    double scale, bool required) {
    auto f = api.item(name);
    if (f.number("sensorState", -1) != 0 || !f.has("currentValue") ||
        (f.text("unit") != unit &&
         f.text("unit") != unit.substr(0, unit.find(' '))) ||
        (f.has("enabled") && f.number("enabled") != 1)) {
      if (required)
        throw std::invalid_argument(
            "Yocto sensor unavailable or unit mismatch");
      return;
    }
    if (f.has("calibrationParam") && !cJSON_IsString(f.at("calibrationParam")))
      throw std::invalid_argument("Unsupported calibration encoding");
    auto calibration = f.text("calibrationParam");
    if (!calibration.empty() && calibration != "0" && calibration != "0,")
      throw std::invalid_argument("Use native SDK for Yocto user calibration");
    auto value = f.number("currentValue", NAN) / 65536. * scale;
    if (!std::isfinite(value) || std::abs(value) > 1e12)
      throw std::invalid_argument("Invalid Yocto reading");
    result[id] = value;
  };
  sensor("voltage1", "voltage", "V DC", 1, true);
  sensor("current1", "current", "mA DC", .001, true);
  sensor("power", "power", "W", 1, true);
  sensor("voltage2", "voltage-ac", "V AC", 1, false);
  sensor("current2", "current-ac", "mA AC", .001, false);
  auto power = api.item("power");
  if (power.has("powerFactor")) {
    auto pf = power.number("powerFactor") / 65536.;
    if (std::isfinite(pf) && pf >= -1 && pf <= 1)
      result["power-factor"] = pf;
  }
  return result;
}
} // namespace labbridge::yocto_watt

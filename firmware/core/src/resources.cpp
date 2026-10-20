#include "labbridge/resources.hpp"
#include <algorithm>
#include <cctype>
namespace labbridge {
BoardProfile BoardProfile::stick_s3() {
  return {"m5stack-sticks3",
          {{"http", 4},
           {"tcp", 2},
           {"ble", 2},
           {"usb", 0},
           {"spp", 0},
           {"uart", 0},
           {"gpio", 0},
           {"onewire", 0}},
          {}};
}
BoardProfile BoardProfile::s3_usb_otg() {
  return {"esp32-s3-usb-otg",
          {{"http", 4},
           {"tcp", 2},
           {"ble", 2},
           {"usb", 2},
           {"spp", 0},
           {"uart", 0},
           {"gpio", 0},
           {"onewire", 0}},
          {}};
}
BoardProfile BoardProfile::esp32_devkit() {
  return {"esp32-devkit",
          {{"http", 4},
           {"tcp", 2},
           {"ble", 2},
           {"usb", 0},
           {"spp", 0},
           {"uart", 1},
           {"gpio", 0},
           {"onewire", 0}},
          {}};
}
void Registry::add(DeviceSpec spec) {
  if (spec.id.empty() || spec.id.size() > 48 ||
      !std::all_of(spec.id.begin(), spec.id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_';
      }))
    throw ConfigurationError("Invalid device ID");
  if (spec.driver.empty() || spec.endpoint.empty())
    throw ConfigurationError("Driver and endpoint required");
  auto capacity = board_.capacity.find(spec.transport);
  if (capacity == board_.capacity.end() || capacity->second == 0)
    throw ConfigurationError("Transport unavailable on board");
  unsigned used = 0;
  auto canonical = [](std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return value;
  };
  for (const auto &old : devices_) {
    if (old.driver == spec.driver &&
        canonical(old.endpoint) == canonical(spec.endpoint))
      throw ConfigurationError("Instrument identity already assigned");
    if (old.id == spec.id)
      throw ConfigurationError("Duplicate device ID");
    if (old.transport == spec.transport) {
      ++used;
      if (canonical(old.endpoint) == canonical(spec.endpoint))
        throw ConfigurationError("Endpoint already assigned");
    }
    bool shared_i2c = old.transport == "i2c" && spec.transport == "i2c";
    if (shared_i2c && old.pins != spec.pins)
      throw ConfigurationError("I2C0 devices must share the same SDA/SCL pair");
    for (int pin : spec.pins)
      if (!shared_i2c &&
          std::find(old.pins.begin(), old.pins.end(), pin) != old.pins.end())
        throw ConfigurationError("Pin already assigned");
  }
  if (used >= capacity->second)
    throw ConfigurationError("Board transport capacity exceeded");
  for (int pin : spec.pins) {
    if (std::count(spec.pins.begin(), spec.pins.end(), pin) != 1 ||
        std::find(board_.available_pins.begin(), board_.available_pins.end(),
                  pin) == board_.available_pins.end())
      throw ConfigurationError("Pin not approved by board profile");
  }
  devices_.push_back(std::move(spec));
}
} // namespace labbridge

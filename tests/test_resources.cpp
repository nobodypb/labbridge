#include "labbridge/resources.hpp"
#include <cassert>
#include <iostream>
using namespace labbridge;
int main() {
  Registry registry(BoardProfile::stick_s3());
  registry.add({"load-a", "dl24m", "ble", "aa:bb:cc:dd:ee:01", {}});
  registry.add({"load-b", "dl24m", "ble", "aa:bb:cc:dd:ee:02", {}});
  assert(registry.devices().size() == 2);
  bool rejected = false;
  try {
    registry.add({"load-c", "dl24m", "ble", "aa:bb:cc:dd:ee:03", {}});
  } catch (const ConfigurationError &) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    registry.add({"psu", "rd60xx", "usb", "serial:123", {}});
  } catch (const ConfigurationError &) {
    rejected = true;
  }
  assert(rejected);
  Registry wifi(BoardProfile::stick_s3());
  wifi.add({"wifi-a", "rd60xx", "tcp", "123", {}});
  wifi.add({"wifi-b", "rd60xx", "tcp", "456", {}});
  rejected = false;
  try {
    wifi.add({"wifi-c", "rd60xx", "tcp", "789", {}});
  } catch (const ConfigurationError &) {
    rejected = true;
  }
  assert(rejected && wifi.devices().size() == 2);
  Registry otg(BoardProfile::s3_usb_otg());
  otg.add({"psu-a", "rd60xx", "usb", "serial:123", {}});
  otg.add({"psu-b", "rd60xx", "usb", "serial:456", {}});
  rejected = false;
  try {
    otg.add({"duplicate", "rd60xx", "usb", "serial:123", {}});
  } catch (const ConfigurationError &) {
    rejected = true;
  }
  assert(rejected && otg.devices().size() == 2);
  std::cout << "resource isolation and board validation passed\n";
  BoardProfile mixed{"mixed", {{"usb", 1}, {"uart", 1}}, {16, 17}};
  Registry identities(mixed);
  identities.add({"one", "rd60xx", "usb", "123", {}});
  bool duplicate = false;
  try {
    identities.add({"two", "rd60xx", "uart", "123", {16, 17}});
  } catch (const ConfigurationError &) {
    duplicate = true;
  }
  assert(duplicate);
  Registry bus({"i2c-board", {{"i2c", 16}, {"gpio", 2}}, {4, 5, 6}});
  bus.add({"meter-a", "ina228", "i2c", "64", {4, 5}});
  bus.add({"meter-b", "ina228", "i2c", "65", {4, 5}});
  bool crossed = false;
  try {
    bus.add({"bad", "ina228", "i2c", "66", {4, 6}});
  } catch (const ConfigurationError &) {
    crossed = true;
  }
  assert(crossed && bus.devices().size() == 2);
  auto classic = BoardProfile::esp32_devkit();
  assert(classic.capacity.at("ble") == 2 && classic.capacity.at("spp") == 0);
}

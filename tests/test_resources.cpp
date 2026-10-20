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
}

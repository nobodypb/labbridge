#include "labbridge/catalog.hpp"
#include <cassert>
using namespace labbridge;
int main() {
  Catalog catalog;
  catalog.register_module(
      "test", {"ble"}, [](const Json &) { return std::shared_ptr<Driver>{}; });
  auto b = BoardProfile::stick_s3();
  auto c = Json::parse(
      R"({"devices":[{"id":"a","driver":"test","transport":"ble","endpoint":"a","pins":[]},{"id":"b","driver":"test","transport":"ble","endpoint":"b","pins":[]}]})");
  assert(catalog.validate(c, b).devices().size() == 2);
  c = Json::parse(
      R"({"devices":[{"id":"a","driver":"test","transport":"usb","endpoint":"a"}]})");
  bool fail = false;
  try {
    catalog.validate(c, b);
  } catch (const ConfigurationError &) {
    fail = true;
  }
  assert(fail);
  c = Json::parse(
      R"({"devices":[{"id":"load","driver":"test","transport":"ble","endpoint":"a","cell_temperature":{"sensor_device_id":"load","channel_id":"cell","minimum_c":40,"maximum_c":5,"max_age_ms":3000}}]})");
  fail = false;
  try {
    catalog.validate(c, b);
  } catch (const ConfigurationError &) {
    fail = true;
  }
  assert(fail); // Reject an inverted guard before any factory can be allocated.
}

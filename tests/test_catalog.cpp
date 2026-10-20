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
}

#include "labbridge/json.hpp"
#include <cassert>
using namespace labbridge;
int main() {
  bool rejected = false;
  std::string text = R"({"enabled":true})";
  text.push_back('\0');
  text += R"({"enabled":false})";
  try {
    Json::parse(text);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);
  auto one = Json::parse(R"({"b":[{"y":2,"x":1}],"a":1.0})");
  auto two = Json::parse(R"({"a":1,"b":[{"x":1,"y":2}]})");
  assert(one.canonical_dump() == two.canonical_dump());
}

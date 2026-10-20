#include "labbridge/json.hpp"
#include <cassert>
#include <cstdlib>
using namespace labbridge;
static bool exhausted = false;
static int budget = -1;
static void *allocate(size_t n) {
  if (exhausted || budget == 0)
    return nullptr;
  if (budget > 0)
    --budget;
  return std::malloc(n);
}
int main() {
  cJSON_Hooks hooks{allocate, std::free};
  cJSON_InitHooks(&hooks);
  Json value;
  value.set("safe", "original");
  exhausted = true;
  bool rejected = false;
  try {
    value.set("safe", "replacement");
  } catch (const std::bad_alloc &) {
    rejected = true;
  }
  exhausted = false;
  assert(rejected && value.text("safe") == "original");
  exhausted = true;
  rejected = false;
  try {
    Json missing;
  } catch (const std::bad_alloc &) {
    rejected = true;
  }
  exhausted = false;
  assert(rejected);
  Json copy;
  exhausted = true;
  rejected = false;
  try {
    copy.set("body", value);
  } catch (const std::bad_alloc &) {
    rejected = true;
  }
  exhausted = false;
  assert(rejected && !copy.has("body"));
  Json payload;
  payload.set("escaped\"key", "line\n\t\"\\\x01 and UTF-8: Ω")
      .set("tiny", 1e-250)
      .set("large", -1.7976931348623157e308)
      .set("truth", true)
      .set("array", Json::parse("[null,false,0]"));
  // Serialization must not allocate a second cJSON print buffer: it prints
  // directly into its owned string, leaving room for operation confirmations.
  exhausted = true;
  auto encoded = payload.dump();
  exhausted = false;
  assert(cJSON_Compare(Json::parse(encoded).get(), payload.get(), true));
  auto canonical = value.canonical_dump();
  for (int available = 0; available < 16; ++available) {
    budget = available;
    try {
      auto limited = value.canonical_dump();
      assert(limited ==
             canonical); // A partial idempotency signature is never success.
    } catch (const std::exception &) {
    }
    budget = -1;
  }
}

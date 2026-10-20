#pragma once
#include "cJSON.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace labbridge {
class Json {
  cJSON *value_;
  static cJSON *ordered(const cJSON *value) {
    if (cJSON_IsObject(value)) {
      auto result = cJSON_CreateObject();
      std::vector<const cJSON *> fields;
      for (auto p = value->child; p; p = p->next)
        fields.push_back(p);
      std::sort(fields.begin(), fields.end(), [](auto a, auto b) {
        return std::strcmp(a->string, b->string) < 0;
      });
      for (auto p : fields)
        cJSON_AddItemToObject(result, p->string, ordered(p));
      return result;
    }
    if (cJSON_IsArray(value)) {
      auto result = cJSON_CreateArray();
      for (auto p = value->child; p; p = p->next)
        cJSON_AddItemToArray(result, ordered(p));
      return result;
    }
    return cJSON_Duplicate(value, true);
  }

public:
  Json() : value_(cJSON_CreateObject()) {}
  explicit Json(cJSON *owned) : value_(owned) {
    if (!owned)
      throw std::invalid_argument("Invalid JSON");
  }
  ~Json() { cJSON_Delete(value_); }
  Json(const Json &other) : value_(cJSON_Duplicate(other.value_, true)) {}
  Json(Json &&other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
  Json &operator=(Json other) {
    std::swap(value_, other.value_);
    return *this;
  }
  static Json parse(const std::string &s) {
    if (s.find('\0') != std::string::npos)
      throw std::invalid_argument("Embedded NUL in JSON text");
    return Json(cJSON_ParseWithOpts(s.c_str(), nullptr, true));
  }
  static Json array() { return Json(cJSON_CreateArray()); }
  cJSON *get() const { return value_; }
  cJSON *at(const std::string &k) const {
    return cJSON_GetObjectItemCaseSensitive(value_, k.c_str());
  }
  bool has(const std::string &k) const { return at(k) != nullptr; }
  Json item(const std::string &k) const {
    auto p = at(k);
    return p ? Json(cJSON_Duplicate(p, true)) : Json();
  }
  std::string text(const std::string &k,
                   const std::string &fallback = "") const {
    auto p = at(k);
    return cJSON_IsString(p) ? p->valuestring : fallback;
  }
  double number(const std::string &k, double fallback = 0) const {
    auto p = at(k);
    return cJSON_IsNumber(p) ? p->valuedouble : fallback;
  }
  bool boolean(const std::string &k) const { return cJSON_IsTrue(at(k)); }
  Json &set(const std::string &k, const std::string &v) {
    cJSON_DeleteItemFromObjectCaseSensitive(value_, k.c_str());
    cJSON_AddStringToObject(value_, k.c_str(), v.c_str());
    return *this;
  }
  Json &set(const std::string &k, const char *v) {
    return set(k, std::string(v));
  }
  Json &set(const std::string &k, double v) {
    if (!std::isfinite(v))
      throw std::invalid_argument("Non-finite JSON number");
    cJSON_DeleteItemFromObjectCaseSensitive(value_, k.c_str());
    cJSON_AddNumberToObject(value_, k.c_str(), v);
    return *this;
  }
  Json &set(const std::string &k, bool v) {
    cJSON_DeleteItemFromObjectCaseSensitive(value_, k.c_str());
    cJSON_AddBoolToObject(value_, k.c_str(), v);
    return *this;
  }
  Json &set(const std::string &k, const Json &v) {
    cJSON_DeleteItemFromObjectCaseSensitive(value_, k.c_str());
    cJSON_AddItemToObject(value_, k.c_str(), cJSON_Duplicate(v.get(), true));
    return *this;
  }
  void append(const Json &v) {
    cJSON_AddItemToArray(value_, cJSON_Duplicate(v.get(), true));
  }
  void append(const std::string &v) {
    cJSON_AddItemToArray(value_, cJSON_CreateString(v.c_str()));
  }
  std::string canonical_dump() const { return Json(ordered(value_)).dump(); }
  std::string dump() const {
    auto p = cJSON_PrintUnformatted(value_);
    if (!p)
      throw std::bad_alloc();
    std::string s(p);
    cJSON_free(p);
    return s;
  }
};
} // namespace labbridge

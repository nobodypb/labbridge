#pragma once
#include "cJSON.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace labbridge {
class Json {
  cJSON *value_;
  static cJSON *allocated(cJSON *value) {
    if (!value)
      throw std::bad_alloc();
    return value;
  }
  bool linked(const cJSON *item) const {
    for (auto child = value_->child; child; child = child->next)
      if (child == item)
        return true;
    return false;
  }
  Json &assign(const std::string &key, cJSON *owned) {
    allocated(owned);
    auto previous = at(key);
    cJSON_AddItemToObject(value_, key.c_str(), owned);
    if (!linked(owned)) {
      cJSON_Delete(owned);
      throw std::bad_alloc();
    }
    if (previous)
      cJSON_Delete(cJSON_DetachItemViaPointer(value_, previous));
    return *this;
  }
  static size_t quoted_size(const char *text) {
    size_t size = 2;
    for (auto p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
      size += *p == '"' || *p == '\\' || *p == '\b' || *p == '\f' ||
                      *p == '\n' || *p == '\r' || *p == '\t'
                  ? 2
              : *p < 32 ? 6
                        : 1;
    return size;
  }
  static size_t printed_size(const cJSON *value) {
    if (cJSON_IsString(value))
      return quoted_size(value->valuestring);
    if (cJSON_IsRaw(value))
      return std::strlen(value->valuestring);
    if (cJSON_IsArray(value) || cJSON_IsObject(value)) {
      size_t size = 2;
      for (auto child = value->child; child; child = child->next) {
        if (child != value->child)
          ++size;
        if (cJSON_IsObject(value))
          size += quoted_size(child->string) + 1;
        size += printed_size(child);
      }
      return size;
    }
    // Use the same number formatting as the linked cJSON implementation,
    // without allocating another print buffer or assuming a particular float
    // format.
    char scalar[64];
    if (!cJSON_PrintPreallocated(const_cast<cJSON *>(value), scalar,
                                 sizeof(scalar), false))
      throw std::invalid_argument("Invalid JSON scalar");
    return std::strlen(scalar);
  }
  static cJSON *ordered(const cJSON *value) {
    if (cJSON_IsObject(value)) {
      Json result;
      std::vector<const cJSON *> fields;
      for (auto p = value->child; p; p = p->next)
        fields.push_back(p);
      std::sort(fields.begin(), fields.end(), [](auto a, auto b) {
        return std::strcmp(a->string, b->string) < 0;
      });
      for (auto p : fields)
        result.assign(p->string, ordered(p));
      return std::exchange(result.value_, nullptr);
    }
    if (cJSON_IsArray(value)) {
      auto result = Json::array();
      for (auto p = value->child; p; p = p->next)
        result.append(Json(ordered(p)));
      return std::exchange(result.value_, nullptr);
    }
    return allocated(cJSON_Duplicate(value, true));
  }

public:
  Json() : value_(allocated(cJSON_CreateObject())) {}
  explicit Json(cJSON *owned) : value_(owned) {
    if (!owned)
      throw std::invalid_argument("Invalid JSON");
  }
  ~Json() { cJSON_Delete(value_); }
  Json(const Json &other)
      : value_(allocated(cJSON_Duplicate(other.value_, true))) {}
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
  static Json array() { return Json(allocated(cJSON_CreateArray())); }
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
    return assign(k, cJSON_CreateString(v.c_str()));
  }
  Json &set(const std::string &k, const char *v) {
    return set(k, std::string(v));
  }
  Json &set(const std::string &k, double v) {
    if (!std::isfinite(v))
      throw std::invalid_argument("Non-finite JSON number");
    return assign(k, cJSON_CreateNumber(v));
  }
  Json &set(const std::string &k, bool v) {
    return assign(k, cJSON_CreateBool(v));
  }
  Json &set(const std::string &k, const Json &v) {
    return assign(k, cJSON_Duplicate(v.get(), true));
  }
  Json &set(const std::string &k, Json &&v) {
    return assign(k, std::exchange(v.value_, nullptr));
  }
  void append(Json v) {
    cJSON_AddItemToArray(value_, v.value_);
    if (!linked(v.value_))
      throw std::bad_alloc();
    v.value_ = nullptr;
  }
  void append(const std::string &v) {
    auto owned = allocated(cJSON_CreateString(v.c_str()));
    cJSON_AddItemToArray(value_, owned);
    if (!linked(owned)) {
      cJSON_Delete(owned);
      throw std::bad_alloc();
    }
  }
  std::string canonical_dump() const { return Json(ordered(value_)).dump(); }
  std::string dump() const {
    // cJSON's preallocated printer requires five spare bytes. Allocate the
    // result once; its growing temporary buffer otherwise doubles peak ESP RAM.
    auto size = printed_size(value_) + 6;
    if (size > size_t(std::numeric_limits<int>::max()))
      throw std::length_error("JSON output too large");
    std::string result(size, '\0');
    if (!cJSON_PrintPreallocated(value_, result.data(), int(size), false))
      throw std::invalid_argument("JSON serialization failed");
    result.resize(std::strlen(result.c_str()));
    return result;
  }
};
} // namespace labbridge

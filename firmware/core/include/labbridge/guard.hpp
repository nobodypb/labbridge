#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
namespace labbridge {
struct Limits {
  double voltage = 0, current = 0, power = 0;
  uint64_t max_age_ms = 3000, max_duration_ms = 0;
  bool valid() const {
    return std::isfinite(voltage) && std::isfinite(current) &&
           std::isfinite(power) && voltage > 0 && current > 0 && power > 0 &&
           max_age_ms > 0 && max_age_ms <= 3000;
  }
};
struct Sample {
  double voltage, current, power_mw;
  uint64_t received_ms, sequence;
};
class Guard {
public:
  explicit Guard(std::function<void()> stop) : stop_(std::move(stop)) {}
  void configure(Limits limits) { limits_ = limits; }
  bool acquire(const std::string &token, uint64_t now, uint64_t ttl) {
    tick(now);
    if (!token_.empty() || token.empty() || ttl < 1000 || ttl > 10000)
      return false;
    token_ = token;
    ttl_ = ttl;
    deadline_ = now + ttl;
    fault_ = false;
    return true;
  }
  bool heartbeat(const std::string &token, uint64_t now) {
    tick(now);
    if (token_ != token || token.empty() || fault_)
      return false;
    deadline_ = now + ttl_;
    return true;
  }
  bool owns(const std::string &token, uint64_t now) const {
    return !fault_ && !token.empty() && token == token_ && now < deadline_;
  }
  bool may_enable(const std::string &token, uint64_t now) const {
    return owns(token, now) && limits_.valid() && fresh(now) && within();
  }
  void enabled(uint64_t now) {
    on_ = true;
    enabled_at_ = now;
  }
  void confirmed_off() { on_ = false; }
  void observe(Sample sample, uint64_t now) {
    sample_ = sample;
    have_sample_ = true;
    tick(now);
  }
  void tick(uint64_t now) {
    if (!token_.empty() && now >= deadline_) {
      token_.clear();
      trip();
      return;
    }
    if (on_ && (!limits_.valid() || !fresh(now) || !within() ||
                (limits_.max_duration_ms &&
                 now - enabled_at_ >= limits_.max_duration_ms)))
      trip();
  }
  void disconnect() {
    have_sample_ = false;
    trip();
  }
  void release() {
    token_.clear();
    trip();
  }
  bool faulted() const { return fault_; }

private:
  bool fresh(uint64_t now) const {
    return have_sample_ && sample_.received_ms <= now &&
           now - sample_.received_ms <= limits_.max_age_ms;
  }
  bool within() const {
    return std::isfinite(sample_.voltage) && std::isfinite(sample_.current) &&
           std::isfinite(sample_.power_mw) && sample_.voltage >= 0 &&
           sample_.current >= 0 && sample_.power_mw >= 0 &&
           sample_.voltage <= limits_.voltage &&
           sample_.current <= limits_.current &&
           sample_.power_mw <= 1000 * limits_.power;
  }
  void trip() {
    if (!fault_) {
      fault_ = true;
      on_ = false;
      stop_();
    }
  }
  std::function<void()> stop_;
  Limits limits_;
  Sample sample_{};
  bool have_sample_ = false, on_ = false, fault_ = false;
  std::string token_;
  uint64_t deadline_ = 0, ttl_ = 0, enabled_at_ = 0;
};
} // namespace labbridge

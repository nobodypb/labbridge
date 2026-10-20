#pragma once
#include "labbridge/guard.hpp"
#include "labbridge/node.hpp"
#include "labbridge/transaction.hpp"
#include "rd_wifi.hpp"
namespace labbridge {
class Rd60xxDriver : public Driver {
public:
  Rd60xxDriver(std::string id, uint32_t serial, Endpoint &, Limits,
               std::function<void(const Json &)>, Json,
               std::string transport = "usb", unsigned model = 6024);
  Json describe(uint64_t) override;
  Json measurements(uint64_t) override;
  bool supports(const std::string &) const override;
  void tick(uint64_t) override;
  void stop(uint64_t) override;
  Json execute(const std::string &, const Json &, uint64_t) override;
  Json operation(uint64_t) override;
  bool safe_to_reconfigure(uint64_t) override {
    return !journal_.boolean("may_be_on");
  }

private:
  uint64_t active_duration_ms_ = 0;
  enum class Phase { idle, identity, echo, verify, poll, drain };
  std::string id_, transport_;
  uint32_t serial_;
  unsigned model_;
  rd60xx::RtuStream stream_;
  Endpoint &wire_;
  Limits limits_;
  std::function<void(const Json &)> persist_;
  Json journal_, result_;
  rd60xx::Measurement measurement_{};
  bool have_ = false, need_stop_ = true, user_ = false, stopping_ = false;
  bool activation_sent_ = false;
  Phase phase_ = Phase::idle;
  size_t drain_size_ = 45;
  uint64_t generation_ = 0, received_ = 0, sequence_ = 0, due_ = 0,
           started_ = 0, enabled_at_ = 0;
  uint16_t reg_ = 18, value_ = 0;
  unsigned attempts_ = 0;
  Bytes rx_, request_;
  void send_read(Phase, uint64_t);
  void send_write(uint64_t);
  bool consume_read(uint64_t);
  void fail(const std::string &, uint64_t);
  void recovery_stop(uint64_t);
  bool fresh(uint64_t) const;
};
} // namespace labbridge

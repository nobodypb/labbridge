#pragma once
#include "labbridge/guard.hpp"
#include "labbridge/node.hpp"
#include "labbridge/transaction.hpp"
#include <map>
namespace labbridge {
class Dl24mDriver : public Driver {
public:
  using Persist = std::function<void(const Json &)>;
  Dl24mDriver(std::string id, std::string identity, Endpoint &, Limits, Persist,
              Json journal);
  Json describe(uint64_t) override;
  Json measurements(uint64_t) override;
  bool supports(const std::string &) const override;
  void tick(uint64_t) override;
  void stop(uint64_t) override;
  Json execute(const std::string &, const Json &, uint64_t) override;
  Json operation(uint64_t) override;
  bool safe_to_reconfigure(uint64_t) override {
    return !journal_.boolean("may_be_on") &&
           !journal_.boolean("menu_exit_pending") && restore_.empty();
  }

private:
  enum class Work { none, stop, drain, poll, snapshot, user, restore };
  std::string id_, identity_;
  Endpoint &wire_;
  Transaction tx_;
  Limits limits_;
  Persist persist_;
  Json journal_, result_, snapshot_body_;
  bool snapshot_ready_ = false;
  uint64_t operation_started_ = 0;
  unsigned snapshot_reads_ = 0, snapshot_writes_ = 0;
  std::map<uint8_t, std::pair<uint32_t, uint64_t>> values_;
  Work work_ = Work::none, queued_work_ = Work::none;
  std::vector<Step> queued_steps_;
  bool queued_enabling_ = false;
  uint32_t stop_delay_ = 0;
  void foreground(Work, std::vector<Step>, uint64_t, bool = false);
  bool need_stop_ = true, menu_checked_ = false;
  uint64_t generation_ = 0, next_ = 0, sequence_ = 0, enabled_at_ = 0;
  size_t poll_ = 0;
  uint8_t polling_ = 0;
  std::vector<Step> restore_;
  bool bank_operation_ = false, bank_write_ = false, bank_cc_captured_ = false,
       bank_original_captured_ = false;
  uint8_t bank_ = 0;
  uint32_t bank_cc_original_ = 0, bank_original_ = 0, bank_target_ = 0;
  void observe(uint8_t, uint32_t, uint64_t);
  void save_bank_restore(bool);
  void save();
  void begin_stop(uint64_t);
  bool fresh(uint8_t, uint64_t) const;
  int output(uint64_t) const;
  void build_restore();
  void finish_user(uint64_t);
};
} // namespace labbridge

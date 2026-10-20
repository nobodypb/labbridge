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

private:
  enum class Work { none, stop, poll, user, restore };
  std::string id_, identity_;
  Endpoint &wire_;
  Transaction tx_;
  Limits limits_;
  Persist persist_;
  Json journal_, result_;
  std::map<uint8_t, std::pair<uint32_t, uint64_t>> values_;
  Work work_ = Work::none;
  bool need_stop_ = true;
  uint64_t generation_ = 0, next_ = 0, sequence_ = 0, enabled_at_ = 0;
  size_t poll_ = 0;
  uint8_t polling_ = 0;
  std::vector<Step> restore_;
  void save();
  void begin_stop(uint64_t);
  bool fresh(uint8_t, uint64_t) const;
  int output(uint64_t) const;
  void build_restore();
  void finish_user(uint64_t);
};
} // namespace labbridge

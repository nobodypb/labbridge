#pragma once
#include <cstdint>
#include <stdexcept>
namespace labbridge {
enum class Path { isolated, charge, discharge };
class SwitchIo {
public:
  virtual ~SwitchIo() = default;
  virtual void drive(bool, bool) = 0;
  virtual bool charge_closed() const = 0;
  virtual bool discharge_closed() const = 0;
};
class Interlock {
public:
  Interlock(SwitchIo &io, uint32_t break_ms) : io_(io), break_ms_(break_ms) {
    if (break_ms < 20)
      throw std::invalid_argument("Break interval must be at least 20 ms");
    stop();
  }
  bool select(Path path, uint64_t now) {
    if (fault_)
      return false;
    io_.drive(false, false);
    current_ = Path::isolated;
    target_ = path;
    phase_ = Phase::opening;
    ready_at_ = now + break_ms_;
    deadline_ = now + 500;
    return true;
  }
  void stop(uint64_t now = 0) {
    io_.drive(false, false);
    current_ = target_ = Path::isolated;
    phase_ = Phase::opening;
    ready_at_ = now;
    deadline_ = now + 500;
  }
  void tick(uint64_t now) {
    if (fault_)
      return;
    if (phase_ == Phase::opening) {
      if (io_.charge_closed() || io_.discharge_closed()) {
        if (now >= deadline_)
          fail();
        return;
      }
      if (now < ready_at_)
        return;
      if (target_ == Path::isolated) {
        phase_ = Phase::idle;
        return;
      }
      io_.drive(target_ == Path::charge, target_ == Path::discharge);
      phase_ = Phase::closing;
      deadline_ = now + 500;
    }
    if (phase_ == Phase::closing) {
      if (matches(target_)) {
        current_ = target_;
        phase_ = Phase::idle;
      } else if (now >= deadline_)
        fail();
      return;
    }
    if (phase_ == Phase::idle && !matches(current_))
      fail();
  }
  Path path() const { return current_; }
  bool faulted() const { return fault_; }
  bool confirmed() const {
    return !fault_ && phase_ == Phase::idle && matches(current_);
  }

private:
  enum class Phase { idle, opening, closing };
  bool matches(Path p) const {
    return io_.charge_closed() == (p == Path::charge) &&
           io_.discharge_closed() == (p == Path::discharge);
  }
  void fail() {
    stop();
    fault_ = true;
  }
  SwitchIo &io_;
  uint32_t break_ms_;
  uint64_t ready_at_ = 0, deadline_ = 0;
  Path current_ = Path::isolated, target_ = Path::isolated;
  Phase phase_ = Phase::opening;
  bool fault_ = false;
};
} // namespace labbridge

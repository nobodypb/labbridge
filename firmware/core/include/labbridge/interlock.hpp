#pragma once
#include <cstdint>
#include <stdexcept>
namespace labbridge {
enum class Path { isolated, charge, discharge };
class SwitchIo {
public:
  virtual ~SwitchIo() = default;
  virtual void drive(bool charge, bool discharge) = 0;
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
    pending_ = Path::isolated;
    pending_ = path;
    ready_at_ = now + break_ms_;
    closing_ = false;
    return true;
  }
  void tick(uint64_t now) {
    if (fault_)
      return;
    if (closing_) {
      if (io_.charge_closed() == (pending_ == Path::charge) &&
          io_.discharge_closed() == (pending_ == Path::discharge)) {
        current_ = pending_;
        pending_ = Path::isolated;
        closing_ = false;
      } else if (now >= close_deadline_)
        fail();
      return;
    }
    if (pending_ == Path::isolated) {
      if (io_.charge_closed() != (current_ == Path::charge) ||
          io_.discharge_closed() != (current_ == Path::discharge))
        fail();
      return;
    }
    if (now < ready_at_)
      return;
    if (io_.charge_closed() || io_.discharge_closed()) {
      fail();
      return;
    }
    io_.drive(pending_ == Path::charge, pending_ == Path::discharge);
    closing_ = true;
    close_deadline_ = now + 500;
    tick(now);
  }
  void stop() {
    io_.drive(false, false);
    pending_ = current_ = Path::isolated;
    closing_ = false;
  }
  Path path() const { return current_; }
  bool faulted() const { return fault_; }

private:
  void fail() {
    stop();
    fault_ = true;
  }
  SwitchIo &io_;
  uint32_t break_ms_;
  uint64_t ready_at_ = 0, close_deadline_ = 0;
  bool closing_ = false;
  Path current_ = Path::isolated, pending_ = Path::isolated;
  bool fault_ = false;
};
} // namespace labbridge

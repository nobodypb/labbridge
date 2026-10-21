#pragma once
#include "codec.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
namespace labbridge {
using Bytes = dl24m::Bytes;
// Send must enqueue or finish promptly. ATT waits belong in a transport worker,
// never in the node's safety loop. Each endpoint owns its receive stream.
struct WireObservation {
  uint64_t sequence, received_ms, generation;
  bool transmitted, truncated;
  Bytes bytes;
};
class Endpoint {
public:
  virtual ~Endpoint() = default;
  virtual bool trace_supported() const { return false; }
  virtual std::vector<WireObservation> traffic() const { return {}; }
  virtual bool connected() const = 0;
  virtual uint64_t generation() const = 0;
  virtual bool writable() const { return true; }
  virtual void cancel_pending() {}
  // Network transports discard a timed-out byte stream before accepting new
  // work.
  virtual void reset_connection() { cancel_pending(); }
  virtual bool send(const Bytes &) = 0;
  virtual Bytes receive() = 0;
};
enum class TxState { idle, running, succeeded, failed, cancelled };
struct TxResult {
  TxState state = TxState::idle;
  std::string reason;
  unsigned read_attempts = 0, write_attempts = 0;
  uint64_t duration_ms = 0;
  uint32_t value = 0;
};
struct Step {
  uint8_t write = 0;
  uint32_t value = 0;
  uint8_t read = 0;
  std::optional<uint32_t> expected;
  uint32_t before_ms = 0;
  std::function<void(Step &)> prepare{};
};
class Transaction {
public:
  explicit Transaction(
      Endpoint &endpoint,
      std::function<void(uint8_t, uint32_t, uint64_t)> observer = {})
      : endpoint_(endpoint), observer_(std::move(observer)) {}
  void start(std::vector<Step> steps, uint64_t now, bool may_enable) {
    if (result_.state == TxState::running)
      throw std::logic_error("Transaction busy");
    steps_ = std::move(steps);
    result_ = {};
    result_.state = TxState::running;
    start_ = now;
    generation_ = endpoint_.generation();
    index_ = 0;
    writes_ = reads_ = 0;
    waiting_ = false;
    reply_pending_ = false;
    prepared_ = false;
    drain_ = false;
    write_sent_ = false;
    enabling_ = may_enable;
    stream_.clear();
    endpoint_.receive();
    if (steps_.empty()) {
      finish(TxState::succeeded, "", now);
      return;
    }
    due_ = now + steps_[0].before_ms;
  }
  void tick(uint64_t now) {
    if (result_.state != TxState::running)
      return;
    result_.duration_ms = now - start_;
    if (now - start_ >= 10000) {
      finish(TxState::failed, "deadline", now);
      return;
    }
    if (!endpoint_.connected() || generation_ != endpoint_.generation()) {
      finish(TxState::failed, enabling_ ? "connection_changed" : "disconnected",
             now);
      return;
    }
    if (drain_ && !waiting_ && endpoint_.writable()) {
      finish(TxState::cancelled, "cancelled", now);
      return;
    }
    auto bytes = endpoint_.receive();
    auto frames = stream_.feed(bytes.data(), bytes.size());
    auto &step = steps_[index_];
    if (!prepared_) {
      if (step.prepare)
        step.prepare(step);
      prepared_ = true;
    }
    if (waiting_ || reply_pending_) {
      for (auto &frame : frames) {
        uint32_t value = 0;
        if (!dl24m::px3_reply(frame, step.read, value))
          continue;
        result_.value = value;
        if (observer_)
          observer_(step.read, value, now);
        waiting_ = false;
        reply_pending_ = false;
        if (drain_) {
          finish(TxState::cancelled, "cancelled", now);
          return;
        }
        if (!step.expected || value == *step.expected) {
          if (++index_ == steps_.size()) {
            finish(TxState::succeeded, "", now);
            return;
          }
          prepared_ = false;
          writes_ = reads_ = 0;
          write_sent_ = false;
          due_ = now + steps_[index_].before_ms;
          return;
        }
        if (enabling_ && step.write && reads_ < 3) {
          // The write may already have taken effect or hit a protection.
          // Verify by reading; never repeat a sent activation.
          due_ = now + 60;
          return;
        }
        if (!enabling_ && step.write && writes_ < 3) {
          reads_ = 0;
          write_sent_ = false;
          due_ = now + 200;
          return;
        }
        finish(TxState::failed, "readback_mismatch", now);
        return;
      }
      // Backoff still accepts the outstanding step/generation above, before
      // another query or write is sent. Only an active timer expires here.
      if (waiting_) {
        if (now < due_)
          return;
        waiting_ = false;
        if (drain_) {
          finish(TxState::cancelled, "cancelled_timeout", now);
          return;
        }
        if (reads_ >= 3) {
          if (!step.write || writes_ >= 3 || enabling_) {
            finish(TxState::failed, "read_timeout", now);
            return;
          }
          reads_ = 0;
          write_sent_ = false;
          due_ = now + 200;
          return;
        }
        due_ = now + 60;
        return;
      }
    }
    if (now < due_ || !endpoint_.writable())
      return;
    // Before a query, discard old bytes; a reply cannot be carried into another
    // operation. Protocol has no request IDs: see handbook residual ambiguity.
    stream_.clear();
    if (step.write && reads_ == 0 && !write_sent_) {
      reply_pending_ = false;
      ++writes_;
      ++result_.write_attempts;
      if (!endpoint_.send(dl24m::px3_command(step.write, step.value))) {
        finish(TxState::failed, "send_failed", now);
        return;
      }
      write_sent_ = true;
      due_ = now;
      return;
    }
    if (!step.read) {
      write_sent_ = false;
      if (++index_ == steps_.size()) {
        finish(TxState::succeeded, "", now);
        return;
      }
      prepared_ = false;
      writes_ = reads_ = 0;
      write_sent_ = false;
      due_ = now + steps_[index_].before_ms;
      return;
    }
    ++reads_;
    ++result_.read_attempts;
    if (!endpoint_.send(dl24m::px3_command(step.read))) {
      finish(TxState::failed, "send_failed", now);
      return;
    }
    waiting_ = true;
    reply_pending_ = true;
    due_ = now + 160;
  }
  void cancel_after_io() {
    if (result_.state == TxState::running)
      drain_ = true;
  }
  void cancel() {
    if (result_.state == TxState::running) {
      result_.state = TxState::cancelled;
      result_.reason = "cancelled";
    }
    write_sent_ = false;
  }
  const TxResult &result() const { return result_; }

private:
  void finish(TxState s, const std::string &reason, uint64_t now) {
    reply_pending_ = false;
    result_.state = s;
    result_.reason = reason;
    result_.duration_ms = now - start_;
    write_sent_ = false;
  }
  Endpoint &endpoint_;
  std::function<void(uint8_t, uint32_t, uint64_t)> observer_;
  dl24m::AtorchStream stream_;
  std::vector<Step> steps_;
  TxResult result_;
  size_t index_ = 0;
  unsigned writes_ = 0, reads_ = 0;
  uint64_t start_ = 0, generation_ = 0, due_ = 0;
  bool waiting_ = false, reply_pending_ = false, enabling_ = false,
       write_sent_ = false, prepared_ = false, drain_ = false;
};
} // namespace labbridge

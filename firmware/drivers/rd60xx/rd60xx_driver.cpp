#include "rd60xx_driver.hpp"
#include <algorithm>
#include <cmath>
namespace labbridge {
namespace {
Json progress() {
  Json j;
  j.set("status", "running")
      .set("stage", "identity")
      .set("read_attempts", 0.)
      .set("write_attempts", 0.)
      .set("reconnects", 0.)
      .set("duration_ms", 0.)
      .set("final_output", "unknown")
      .set("restore_pending", false);
  return j;
}
} // namespace
Rd60xxDriver::Rd60xxDriver(std::string id, uint32_t serial, Endpoint &wire,
                           Limits lim, std::function<void(const Json &)> save,
                           Json journal, std::string transport, unsigned model)
    : id_(std::move(id)), transport_(std::move(transport)), serial_(serial),
      model_(model), wire_(wire), limits_(lim), persist_(std::move(save)),
      journal_(std::move(journal)), result_(progress()) {
  if (!serial_)
    throw std::invalid_argument("RD60xx requires a nonzero serial");
  rd60xx::profile(model_).validate(limits_);
  active_duration_ms_ = limits_.max_duration_ms;
  if (!journal_.has("may_be_on"))
    journal_.set("may_be_on", journal_.boolean("stop_pending"));
}
bool Rd60xxDriver::fresh(uint64_t now) const {
  return have_ && wire_.connected() && received_ <= now &&
         now - received_ <= limits_.max_age_ms;
}
bool Rd60xxDriver::supports(const std::string &op) const {
  return op == "setSourceVoltage" || op == "setSourceCurrent" ||
         op == "setOutput";
}
Json Rd60xxDriver::describe(uint64_t now) {
  Json j, l;
  auto a = Json::array();
  for (auto c : {"source.cc_cv", "output", "voltage.measure", "current.measure",
                 "power.measure"})
    a.append(std::string(c));
  l.set("voltage_v", limits_.voltage)
      .set("current_a", limits_.current)
      .set("power_w", limits_.power)
      .set("max_age_ms", double(limits_.max_age_ms));
  j.set("id", id_)
      .set("driver", "rd60xx")
      .set("transport", transport_)
      .set("identity", std::to_string(serial_))
      .set("capabilities", a)
      .set("state", !fresh(now) ? "unavailable"
                    : (need_stop_ && result_.text("status") == "failed") ||
                            measurement_.protection
                        ? "fault"
                    : (user_ || stopping_) ? "busy"
                    : need_stop_           ? "fault"
                                           : "ready")
      .set("output",
           fresh(now) ? measurement_.output ? "on" : "off" : "unknown")
      .set("limits", l);
  return j;
}
Json Rd60xxDriver::measurements(uint64_t now) {
  Json j;
  auto temps = Json::array();
  j.set("device_id", id_)
      .set("sequence", double(sequence_))
      .set("received_ms", double(received_))
      .set("age_ms", double(now - received_))
      .set("quality", fresh(now) ? "valid" : "stale");
  if (have_) {
    j.set("set_voltage_v", measurement_.set_voltage)
        .set("set_current_a", measurement_.set_current)
        .set("regulation_mode", measurement_.cc ? "cc" : "cv");
    j.set("voltage_v", measurement_.voltage)
        .set("current_a", measurement_.current)
        .set("protection_code", double(measurement_.protection))
        .set("power_w", measurement_.power);
    Json t;
    t.set("id", "internal")
        .set("kind", "internal")
        .set("quality", fresh(now) ? "valid" : "stale")
        .set("value_c", measurement_.internal_temperature)
        .set("received_ms", double(received_))
        .set("age_ms", double(now - received_));
    temps.append(t);
  }
  j.set("temperature_channels", temps);
  return j;
}
Json Rd60xxDriver::operation(uint64_t now) {
  auto j = result_;
  j.set("final_output",
        fresh(now) ? measurement_.output ? "on" : "off" : "unknown");
  return j;
}
void Rd60xxDriver::fail(const std::string &reason, uint64_t now) {
  result_.set("status", "failed")
      .set("stage", reason)
      .set("duration_ms", double(now - started_));
  phase_ = Phase::idle;
  user_ = false;
  have_ = false;
  need_stop_ = true;
  stopping_ = false;
  stream_.clear();
  if (transport_ == "tcp")
    wire_.reset_connection();
  due_ = now + 1000;
}
void Rd60xxDriver::send_read(Phase phase, uint64_t now) {
  wire_.receive();
  rx_.clear();
  stream_.clear();
  request_ = rd60xx::read(1, 0, 20);
  if (!wire_.send(request_)) {
    fail("send_failed", now);
    return;
  }
  phase_ = phase;
  due_ = now + 500;
  if (user_ || stopping_)
    result_.set("read_attempts", result_.number("read_attempts") + 1);
}
void Rd60xxDriver::send_write(uint64_t now) {
  wire_.receive();
  rx_.clear();
  stream_.clear();
  request_ = rd60xx::write(1, reg_, value_);
  if (!wire_.send(request_)) {
    fail("send_failed", now);
    return;
  }
  phase_ = Phase::echo;
  due_ = now + 500;
  ++attempts_;
  result_.set("write_attempts", result_.number("write_attempts") + 1);
}
bool Rd60xxDriver::consume_read(uint64_t now) {
  if (!rd60xx::valid(rx_, 1, 3) || rx_.size() != 45)
    return false;
  std::array<uint16_t, 20> r{};
  for (size_t i = 0; i < 20; ++i)
    r[i] = (uint16_t(rx_[3 + 2 * i]) << 8) | rx_[4 + 2 * i];
  try {
    auto m = rd60xx::decode(r, model_);
    if (m.serial != serial_) {
      fail("identity_changed", now);
      return false;
    }
    if (m.output && !journal_.boolean("may_be_on")) {
      journal_.set("may_be_on", true).set("stop_pending", true);
      persist_(journal_);
    }
    measurement_ = m;
    have_ = true;
    received_ = now;
    ++sequence_;
    return true;
  } catch (const std::invalid_argument &) {
    fail("invalid_registers", now);
    return false;
  }
}
void Rd60xxDriver::stop(uint64_t now) {
  wire_.cancel_pending();
  bool active = phase_ != Phase::idle;
  if (active && phase_ != Phase::drain)
    drain_size_ = phase_ == Phase::echo ? 8 : 45;
  user_ = false;
  stopping_ = true;
  need_stop_ = true;
  if (active) {
    phase_ = Phase::drain;
    due_ = std::min<uint64_t>(due_, now + 500);
  } else {
    phase_ = Phase::idle;
    due_ = now;
  }
  reg_ = 18;
  value_ = 0;
  attempts_ = 0;
  started_ = now;
  result_ = progress();
  journal_.set("stop_pending", true);
  persist_(journal_);
}
Json Rd60xxDriver::execute(const std::string &op, const Json &body,
                           uint64_t now) {
  if (!supports(op))
    throw std::invalid_argument("Unsupported source operation");
  if (need_stop_ || !fresh(now) || user_)
    throw std::invalid_argument("Source not ready");
  double v = measurement_.set_voltage, a = measurement_.set_current;
  if (op == "setOutput") {
    reg_ = 18;
    value_ = body.boolean("enabled") ? 1 : 0;
  } else if (op == "setSourceVoltage") {
    reg_ = 8;
    v = body.number("voltage_v", -1);
    value_ = std::lround(v * 100);
  } else {
    reg_ = 9;
    a = body.number("current_a", -1);
    value_ = std::lround(a * 100);
  }
  if (!std::isfinite(v) || !std::isfinite(a) ||
      std::abs(v * 100 - std::round(v * 100)) > 1e-6 ||
      std::abs(a * 100 - std::round(a * 100)) > 1e-6)
    throw std::invalid_argument("RD60xx setpoints require 0.01 V/A steps");
  if (v < 0 || a < 0 || v > limits_.voltage || a > limits_.current ||
      v * a > limits_.power)
    throw std::invalid_argument("Source setpoint exceeds limits");
  if (reg_ == 18 && value_) {
    if (measurement_.output || measurement_.protection)
      throw std::invalid_argument(
          "Source requires confirmed OFF without active protection");
    auto duration = body.number("max_duration_ms", limits_.max_duration_ms);
    if (body.has("max_duration_ms") &&
        (duration < 100 || duration > 86400000 ||
         std::floor(duration) != duration ||
         (limits_.max_duration_ms && duration > limits_.max_duration_ms)))
      throw std::invalid_argument("Duration outside configured limit");
    active_duration_ms_ = duration;
    journal_.set("stop_pending", true).set("may_be_on", true);
    persist_(journal_);
    enabled_at_ = now;
  }
  bool pending_poll = phase_ == Phase::poll;
  user_ = true;
  stopping_ = false;
  if (!pending_poll)
    phase_ = Phase::idle;
  attempts_ = 0;
  started_ = now;
  if (!pending_poll)
    due_ = now;
  result_ = progress();
  return operation(now);
}
void Rd60xxDriver::recovery_stop(uint64_t now) {
  auto previous = result_;
  stop(now);
  if (previous.text("status") == "failed")
    result_ = previous;
}
void Rd60xxDriver::tick(uint64_t now) {
  if (generation_ != wire_.generation()) {
    bool interrupted = user_ || stopping_;
    generation_ = wire_.generation();
    have_ = false;
    phase_ = Phase::idle;
    stream_.clear();
    rx_.clear();
    if (interrupted)
      result_.set("status", "failed").set("stage", "connection_changed");
    recovery_stop(now);
  }
  if (have_ && measurement_.output && !need_stop_ &&
      (!fresh(now) || measurement_.voltage > limits_.voltage ||
       measurement_.current > limits_.current ||
       measurement_.power > limits_.power ||
       (active_duration_ms_ && now - enabled_at_ >= active_duration_ms_))) {
    stop(now);
    result_.set("status", "failed").set("stage", "local_guard");
  }
  if ((user_ || stopping_) && now - started_ >= 10000) {
    fail("deadline", now);
    return;
  }
  if (!wire_.connected())
    return;
  if (phase_ == Phase::idle) {
    if (now < due_ || !wire_.writable())
      return;
    if (need_stop_ && !stopping_)
      recovery_stop(now);
    send_read(user_ || stopping_ ? Phase::identity : Phase::poll, now);
    return;
  }
  auto bytes = wire_.receive();
  if (phase_ == Phase::drain) {
    rx_.insert(rx_.end(), bytes.begin(), bytes.end());
    bool complete =
        rx_.size() >= drain_size_ || (rx_.size() >= 5 && (rx_[1] & 0x80) &&
                                      rd60xx::crc(rx_.data(), rx_.size()) == 0);
    if (complete || now >= due_) {
      rx_.clear();
      stream_.clear();
      phase_ = Phase::idle;
      due_ = now + (complete ? 20 : 250);
    }
    return;
  }
  auto frames = stream_.feed(bytes);
  for (auto &frame : frames) {
    if (frame[0] != 1)
      continue;
    if (frame[1] & 0x80) {
      fail("modbus_exception", now);
      return;
    }
    if ((phase_ == Phase::echo && frame[1] == 6) ||
        (phase_ != Phase::echo && frame[1] == 3 && frame.size() == 45)) {
      rx_ = std::move(frame);
      break;
    }
  }
  if (!wire_.writable())
    return;
  if (phase_ == Phase::echo) {
    if (rx_.size() >= 8) {
      if (rx_ != request_ || !rd60xx::valid(rx_, 1, 6)) {
        fail("write_echo", now);
        return;
      }
      send_read(Phase::verify, now);
      return;
    }
  } else if (rx_.size() >= 45) {
    auto phase = phase_;
    if (!consume_read(now))
      return;
    if (phase == Phase::identity) {
      double v = reg_ == 8 ? value_ / 100. : measurement_.set_voltage,
             a = reg_ == 9 ? value_ / 100. : measurement_.set_current;
      if (!(reg_ == 18 && value_ == 0) &&
          (v > limits_.voltage || a > limits_.current ||
           v * a > limits_.power)) {
        fail("fresh_setpoint_limits", now);
        return;
      }
      send_write(now);
      return;
    }
    if (phase == Phase::verify) {
      double expected = reg_ == 8   ? measurement_.set_voltage
                        : reg_ == 9 ? measurement_.set_current
                                    : double(measurement_.output);
      double requested = reg_ == 18 ? value_ : value_ / 100.;
      if (std::abs(expected - requested) > .001) {
        if (attempts_ < 3) {
          send_read(Phase::identity, now);
          return;
        }
        fail("readback_mismatch", now);
        return;
      }
      if (reg_ == 18 && value_ == 0) {
        need_stop_ = false;
        journal_.set("stop_pending", false).set("may_be_on", false);
        persist_(journal_);
      }
      if (result_.text("status") != "failed")
        result_.set("status", "succeeded")
            .set("stage", "confirmed")
            .set("duration_ms", double(now - started_));
      user_ = false;
      stopping_ = false;
    }
    phase_ = Phase::idle;
    due_ = now + 100;
    return;
  }
  if (now >= due_) {
    if ((user_ || stopping_) && attempts_ < 3 && !(reg_ == 18 && value_ == 1)) {
      ++attempts_;
      send_read(phase_ == Phase::poll ? Phase::poll : Phase::identity, now);
    } else
      fail("response_timeout", now);
  }
}
} // namespace labbridge

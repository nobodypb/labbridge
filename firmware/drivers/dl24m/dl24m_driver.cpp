#include "dl24m_driver.hpp"
#include <algorithm>
#include <cmath>
namespace labbridge {
namespace {
struct Setting {
  const char *name;
  uint8_t write, read;
  double write_scale, read_scale;
  bool menu;
};
const Setting settings[] = {
    {"cutoff_voltage_v", 3, 0x18, 10, 100, false},
    {"time_limit_s", 4, 0x19, 1, 1, false},
    {"working_brightness", 0x42, 0x32, 1, 1, true},
    {"standby_brightness", 0x43, 0x33, 1, 1, true},
    {"standby_time", 0x44, 0x34, 1, 1, true},
    {"resolution", 0x45, 0x35, 1, 1, true},
    {"overvoltage_v", 0x47, 0x37, 1000, 1000, true},
    {"overcurrent_a", 0x48, 0x38, 1000, 1000, true},
    {"overpower_w", 0x49, 0x39, 100, 1000, true},
    {"overtemperature_c", 0x4d, 0x3a, 1, 1, true},
    {"fan_temperature_c", 0x4e, 0x3b, 1, 1, true},
    {"battery_overtemperature_c", 0x50, 0x3c, 1, 1, true},
    {"mode", 6, 0x21, 1, 1, false}};
uint32_t packed_time(uint32_t s) {
  return ((s / 3600) << 16) | (((s / 60) % 60) << 8);
}
uint32_t seconds(uint32_t packed) {
  return ((packed >> 16) & 255) * 3600 + ((packed >> 8) & 255) * 60;
}
Json progress() {
  Json j;
  j.set("status", "running")
      .set("stage", "queued")
      .set("read_attempts", 0.)
      .set("write_attempts", 0.)
      .set("reconnects", 0.)
      .set("duration_ms", 0.)
      .set("final_output", "unknown")
      .set("restore_pending", false);
  return j;
}
} // namespace
Dl24mDriver::Dl24mDriver(std::string id, std::string identity, Endpoint &wire,
                         Limits limits, Persist persist, Json journal)
    : id_(std::move(id)), identity_(std::move(identity)), wire_(wire),
      tx_(wire), limits_(limits), persist_(std::move(persist)),
      journal_(std::move(journal)), result_(progress()) {
  if (!limits_.valid())
    throw std::invalid_argument("DL24M requires positive configured limits");
  build_restore();
}
void Dl24mDriver::save() { persist_(journal_); }
void Dl24mDriver::build_restore() {
  restore_.clear();
  auto a = journal_.at("restore");
  if (cJSON_IsArray(a))
    for (auto p = a->child; p; p = p->next) {
      Json j(cJSON_Duplicate(p, true));
      restore_.push_back({uint8_t(j.number("write")),
                          uint32_t(j.number("wire")), uint8_t(j.number("read")),
                          uint32_t(j.number("expected")), 200});
    }
}
bool Dl24mDriver::fresh(uint8_t c, uint64_t now) const {
  auto it = values_.find(c);
  return it != values_.end() && it->second.second <= now &&
         now - it->second.second <= limits_.max_age_ms;
}
int Dl24mDriver::output(uint64_t now) const {
  auto i = values_.find(0x10);
  return fresh(0x10, now) && i->second.first <= 1 ? int(i->second.first) : -1;
}
bool Dl24mDriver::supports(const std::string &op) const {
  return op == "setOutput" || op == "setLoadCurrent" ||
         op == "getDl24mSettings" || op == "setDl24mSettings" ||
         op == "resetDl24mCounters";
}
Json Dl24mDriver::describe(uint64_t now) {
  Json j, lim;
  auto caps = Json::array();
  for (auto c : {"load.cc", "output", "voltage.measure", "current.measure",
                 "power.measure", "counters", "load.cutoff", "dl24m.settings"})
    caps.append(std::string(c));
  lim.set("voltage_v", limits_.voltage)
      .set("current_a", limits_.current)
      .set("power_w", limits_.power)
      .set("max_age_ms", double(limits_.max_age_ms));
  auto out = output(now);
  j.set("id", id_)
      .set("driver", "dl24m")
      .set("identity", identity_)
      .set("transport", "ble")
      .set("capabilities", caps)
      .set("state", !wire_.connected()                     ? "unavailable"
                    : need_stop_ || !restore_.empty()      ? "fault"
                    : work_ == Work::user                  ? "busy"
                    : fresh(0x11, now) && fresh(0x12, now) ? "ready"
                                                           : "unavailable")
      .set("output", out == 0   ? "off"
                     : out == 1 ? "on"
                                : "unknown")
      .set("limits", lim);
  return j;
}
Json Dl24mDriver::measurements(uint64_t now) {
  Json j;
  auto temps = Json::array();
  bool valid = fresh(0x11, now) && fresh(0x12, now);
  uint64_t received =
      valid ? std::min(values_[0x11].second, values_[0x12].second) : 0;
  j.set("device_id", id_)
      .set("sequence", double(sequence_))
      .set("received_ms", double(received))
      .set("age_ms", double(now - received))
      .set("quality", valid ? "valid" : "stale");
  if (values_.count(0x11) && values_.count(0x12)) {
    double v = values_[0x11].first / 1000., a = values_[0x12].first / 1000.;
    j.set("voltage_v", v).set("current_a", a).set("power_w", v * a);
  }
  if (values_.count(0x16)) {
    auto v = values_[0x16];
    Json t;
    t.set("id", "internal")
        .set("kind", "internal")
        .set("quality", fresh(0x16, now) ? "valid" : "stale")
        .set("value_c", v.first / 10.)
        .set("received_ms", double(v.second))
        .set("age_ms", double(now - v.second));
    temps.append(t);
  }
  // Counter scaling is not independently calibrated. Host integrates V/I
  // itself.
  j.set("temperature_channels", temps);
  return j;
}
void Dl24mDriver::begin_stop(uint64_t now) {
  tx_.cancel();
  journal_.set("stop_pending", true);
  save();
  need_stop_ = true;
  work_ = Work::stop;
  tx_.start({{1, 0, 0x10, 0, 0}}, now, false);
  next_ = now;
}
void Dl24mDriver::stop(uint64_t now) {
  result_ = progress();
  result_.set("stage", "stopping");
  begin_stop(now);
}
void Dl24mDriver::finish_user(uint64_t now) {
  auto r = tx_.result();
  result_.set("status", r.state == TxState::succeeded ? "succeeded" : "failed")
      .set("stage", r.state == TxState::succeeded ? "confirmed" : r.reason)
      .set("read_attempts", double(r.read_attempts))
      .set("write_attempts", double(r.write_attempts))
      .set("duration_ms", double(r.duration_ms));
  if (r.state == TxState::succeeded) {
    restore_.clear();
    journal_.set("restore", Json::array());
    save();
  } else {
    need_stop_ = true;
    values_.erase(0x10);
    next_ = now;
  }
}
Json Dl24mDriver::operation(uint64_t now) {
  auto j = result_;
  auto out = output(now);
  j.set("final_output", out == 0   ? "off"
                        : out == 1 ? "on"
                                   : "unknown")
      .set("restore_pending", !restore_.empty());
  return j;
}
void Dl24mDriver::tick(uint64_t now) {
  if (generation_ != wire_.generation()) {
    generation_ = wire_.generation();
    values_.clear();
    tx_.cancel();
    if (work_ == Work::user) {
      result_.set("status", "failed").set("stage", "connection_changed");
    }
    work_ = Work::none;
    need_stop_ = true;
    journal_.set("stop_pending", true);
    save();
    next_ = now;
  }
  if (!wire_.connected())
    return;
  if (output(now) != 0 && !need_stop_) {
    bool stale = !fresh(0x11, now) || !fresh(0x12, now) || !fresh(0x10, now);
    bool exceed = false;
    if (!stale) {
      double v = values_[0x11].first / 1000., a = values_[0x12].first / 1000.;
      exceed =
          v > limits_.voltage || a > limits_.current || v * a > limits_.power;
    }
    if (stale || exceed ||
        (limits_.max_duration_ms &&
         now - enabled_at_ >= limits_.max_duration_ms)) {
      result_.set("status", "failed").set("stage", "local_guard");
      begin_stop(now);
    }
  }
  if (work_ != Work::none) {
    tx_.tick(now);
    auto state = tx_.result().state;
    if (state == TxState::running)
      return;
    if (work_ == Work::poll) {
      if (state == TxState::succeeded) {
        values_[polling_] = {tx_.result().value, now};
        ++sequence_;
      }
      next_ = now;
    } else if (work_ == Work::stop) {
      if (state == TxState::succeeded) {
        values_[0x10] = {0, now};
        need_stop_ = false;
        journal_.set("stop_pending", false);
        save();
        if (result_.text("status") == "running") {
          result_.set("status", "succeeded").set("stage", "confirmed");
        }
        next_ = now;
      } else {
        values_.erase(0x10);
        next_ = now + 1000;
        result_.set("status", "failed").set("stage", "stop_unconfirmed");
      }
    } else if (work_ == Work::restore) {
      if (state == TxState::succeeded) {
        restore_.clear();
        journal_.set("restore", Json::array());
        save();
      }
      next_ = now + 1000;
    } else
      finish_user(now);
    work_ = Work::none;
  }
  if (now < next_)
    return;
  if (need_stop_) {
    begin_stop(now);
    return;
  }
  if (!restore_.empty()) {
    auto steps = restore_;
    steps.insert(steps.begin(), {0x31, 0, 0, {}, 0});
    steps.push_back({0x4c, 0, 0, {}, 60});
    work_ = Work::restore;
    tx_.start(std::move(steps), now, false);
    return;
  }
  // Fast safety cycle between audited settings reads; settings never starve
  // V/I.
  static constexpr uint8_t fast[] = {0x10, 0x11, 0x12, 0x16, 0x17, 0x21};
  if (poll_ % 7 < 6)
    polling_ = fast[poll_ % 7];
  else
    polling_ =
        dl24m::dl24m_reads[(poll_ / 7) % dl24m::dl24m_reads.size()].command;
  ++poll_;
  work_ = Work::poll;
  tx_.start({{0, 0, polling_, {}, 0}}, now, false);
}
Json Dl24mDriver::execute(const std::string &op, const Json &body,
                          uint64_t now) {
  if (op == "getDl24mSettings") {
    Json j;
    for (auto &s : settings)
      if (values_.count(s.read)) {
        double v = values_[s.read].first / s.read_scale;
        if (s.write == 4)
          v = seconds(values_[s.read].first);
        if (s.write == 0x42 || s.write == 0x43)
          v -= 48;
        j.set(s.name, v);
      }
    return j;
  }
  if (need_stop_ || !restore_.empty() || !wire_.connected())
    throw std::invalid_argument("Device shutdown/restore not complete");
  if (work_ == Work::user)
    throw std::invalid_argument("Device busy");
  std::vector<Step> steps;
  bool enabling = false;
  if (op == "setOutput") {
    enabling = body.boolean("enabled");
    if (!enabling) {
      stop(now);
      return operation(now);
    }
    if (output(now) != 0 || !fresh(0x11, now) || !fresh(0x12, now) ||
        !fresh(0x17, now) || !fresh(0x21, now) || values_[0x21].first != 0)
      throw std::invalid_argument(
          "Fresh OFF state, CC mode and measurements required");
    double v = values_[0x11].first / 1000., a = values_[0x17].first / 1000.;
    if (v > limits_.voltage || a > limits_.current || v * a > limits_.power)
      throw std::invalid_argument("Setpoint exceeds limits");
    journal_.set("stop_pending", true);
    save();
    enabled_at_ = now;
    steps.push_back({1, 0x010000, 0x10, 1, 0});
  } else if (op == "setLoadCurrent") {
    double a = body.number("current_a", -1),
           v = values_.count(0x11) ? values_[0x11].first / 1000.
                                   : limits_.voltage;
    if (a < 0 || a > limits_.current || a * v > limits_.power ||
        !fresh(0x21, now) || values_[0x21].first != 0)
      throw std::invalid_argument("CC mode and in-range current required");
    auto value = uint32_t(std::lround(a * 1000));
    steps.push_back({2, value, 0x17, value, 60});
  } else if (op == "resetDl24mCounters") {
    if (output(now) != 0)
      throw std::invalid_argument("Reset requires OFF");
    steps = {{5, 0, 0x13, 0, 0}, {0, 0, 0x14, 0, 0}, {0, 0, 0x15, 0, 0}};
  } else if (op == "setDl24mSettings") {
    if (output(now) != 0)
      throw std::invalid_argument("Settings require confirmed OFF");
    auto records = Json::array();
    bool menu = false;
    for (auto &s : settings)
      if (body.has(s.name)) {
        if (!values_.count(s.read))
          throw std::invalid_argument("Original setting not read yet");
        double requested = body.number(s.name);
        uint32_t wire = std::lround(requested * s.write_scale),
                 expected = std::lround(requested * s.read_scale),
                 old = values_[s.read].first,
                 oldwire = std::lround(old * s.write_scale / s.read_scale);
        if (s.write == 4) {
          wire = uint32_t(requested);
          expected = packed_time(wire);
          oldwire = seconds(old);
        }
        if (s.write == 0x42 || s.write == 0x43) {
          wire += 48;
          expected += 48;
        }
        if (s.menu && !menu) {
          steps.push_back({0x31, 0, 0, {}, 0});
          menu = true;
        }
        steps.push_back({s.write, wire, s.read, expected, s.menu ? 200u : 60u});
        Json r;
        r.set("write", double(s.write))
            .set("wire", double(oldwire))
            .set("read", double(s.read))
            .set("expected", double(old));
        records.append(r);
      }
    if (body.has("language"))
      throw std::invalid_argument("Language write needs a separately captured "
                                  "menu snapshot; unsupported in this build");
    if (steps.empty())
      throw std::invalid_argument("No supported settings supplied");
    if (menu)
      steps.push_back({0x4c, 0, 0, {}, 60});
    journal_.set("restore", records);
    save();
    build_restore();
  } else
    throw std::invalid_argument("Unsupported DL24M operation");
  tx_.cancel();
  work_ = Work::user;
  result_ = progress();
  tx_.start(std::move(steps), now, enabling);
  return operation(now);
}
} // namespace labbridge

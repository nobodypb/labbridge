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
    {"current_resolution", 0x45, 0x35, 1, 1, true},
    {"overvoltage_v", 0x47, 0x37, 1000, 1000, true},
    {"overcurrent_a", 0x48, 0x38, 1000, 1000, true},
    {"overpower_w", 0x49, 0x39, 100, 1000, true},
    {"overtemperature_c", 0x4d, 0x3a, 1, 1, true},
    {"fan_temperature_c", 0x4e, 0x3b, 1, 1, true},
    {"battery_overtemperature_c", 0x50, 0x3c, 1, 1, true},
    {"language", 0x41, 0x31, 1, 1, true},
    {"mode", 6, 0x21, 1, 1, false}};
uint32_t packed_time(uint32_t s) {
  return ((s / 3600) << 16) | (((s / 60) % 60) << 8);
}
uint32_t seconds(uint32_t packed) {
  return ((packed >> 16) & 255) * 3600 + ((packed >> 8) & 255) * 60;
}
const char *mode_names[] = {"cc", "cv", "cr", "cp"};
const char *mode_units[] = {"A", "V", "Ohm", "W"};
const double mode_scales[] = {1000, 100, 10, 10};
unsigned mode_number(const std::string &mode) {
  for (unsigned n = 0; n < 4; ++n)
    if (mode == mode_names[n])
      return n;
  throw std::invalid_argument("Unsupported load mode");
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
      trace_(wire), tx_(trace_, [this](uint8_t c, uint32_t v,
                                       uint64_t now) { observe(c, v, now); }),
      limits_(limits), persist_(std::move(persist)),
      journal_(std::move(journal)), result_(progress()) {
  if (!limits_.valid())
    throw std::invalid_argument("DL24M requires positive configured limits");
  active_duration_ms_ = limits_.max_duration_ms;
  if (!journal_.has("may_be_on"))
    journal_.set("may_be_on", journal_.boolean("stop_pending"));
  build_restore();
}
void Dl24mDriver::save_bank_restore(bool full) {
  auto records = Json::array();
  auto record = [&](uint8_t write, uint32_t value, uint8_t read,
                    uint32_t expected) {
    Json j;
    j.set("write", double(write))
        .set("wire", double(value))
        .set("read", double(read))
        .set("expected", double(expected));
    records.append(j);
  };
  if (full) {
    record(6, bank_, 0x21, bank_);
    record(2, bank_original_, 0x17, bank_original_);
  }
  record(6, 0, 0x21, 0);
  record(2, bank_cc_original_, 0x17, bank_cc_original_);
  journal_.set("restore", records).set("requires_menu", false);
  save();
  build_restore();
}
void Dl24mDriver::observe(uint8_t c, uint32_t value, uint64_t now) {
  if (c == 0x10 && value == 1 && !journal_.boolean("may_be_on")) {
    journal_.set("may_be_on", true).set("stop_pending", true);
    save();
  }
  values_[c] = {value, now};
  ++sequence_;
  if (!bank_operation_ || work_ != Work::user || c != 0x17 ||
      !values_.count(0x21))
    return;
  auto mode = values_[0x21].first;
  if (mode == 0 && !bank_cc_captured_) {
    bank_cc_original_ = value;
    bank_cc_captured_ = true;
    save_bank_restore(false);
  } else if (mode == bank_ && !bank_original_captured_) {
    bank_original_ = value;
    bank_original_captured_ = true;
    save_bank_restore(true);
  }
}
void Dl24mDriver::foreground(Work work, std::vector<Step> steps, uint64_t now,
                             bool enabling) {
  if (work_ == Work::poll && tx_.result().state == TxState::running) {
    queued_work_ = work;
    queued_steps_ = std::move(steps);
    queued_enabling_ = enabling;
    return;
  }
  tx_.cancel();
  work_ = work;
  tx_.start(std::move(steps), now, enabling);
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
const char *Dl24mDriver::state(uint64_t now) const {
  return !wire_.connected()                                 ? "unavailable"
         : need_stop_ && result_.text("status") == "failed" ? "fault"
         : (journal_.boolean("menu_exit_pending") ||
            queued_work_ != Work::none || work_ == Work::user ||
            work_ == Work::snapshot || work_ == Work::stop ||
            work_ == Work::drain || work_ == Work::restore)
             ? "busy"
         : need_stop_ || !restore_.empty()      ? "fault"
         : fresh(0x11, now) && fresh(0x12, now) ? "ready"
                                                : "unavailable";
}
Json Dl24mDriver::components(uint64_t) {
  auto list = Json::array();
  list.append(Json::parse(
      R"({"id":"sink","kind":"sink","actions":["current","output","cutoff","duration"]})"));
  for (auto field :
       {std::make_pair("voltage", "V"), std::make_pair("current", "A"),
        std::make_pair("power", "W"), std::make_pair("energy", "Wh"),
        std::make_pair("charge", "Ah")})
    list.append(Json()
                    .set("id", field.first)
                    .set("kind", "sensor")
                    .set("actions", Json::array())
                    .set("quantity", field.first)
                    .set("unit", field.second));
  if (values_.count(0x16))
    list.append(Json::parse(
        R"({"id":"temperature-internal","kind":"sensor","actions":[],"quantity":"temperature","unit":"degC","temperature_kind":"internal"})"));
  return list;
}
Json Dl24mDriver::component_reading(const std::string &id, uint64_t now) {
  if (id != "charge" && id != "energy")
    return Driver::component_reading(id, now);
  uint8_t command = id == "charge" ? 0x14 : 0x15;
  auto found = values_.find(command);
  Json result;
  result.set("component_id", id)
      .set("device_id", id_)
      .set("quantity", id)
      .set("unit", id == "charge" ? "Ah" : "Wh")
      .set("quality", found == values_.end() ? "unavailable"
                      : fresh(command, now)  ? "valid"
                                             : "stale")
      .set("sequence", double(sequence_))
      .set("received_ms",
           found == values_.end() ? 0. : double(found->second.second))
      .set("age_ms",
           found == values_.end() ? 0. : double(now - found->second.second));
  if (found != values_.end())
    result.set("value", found->second.first / 1000.);
  return result;
}
Json Dl24mDriver::actuator_state(const std::string &id, uint64_t now) {
  auto out = output(now);
  return Json()
      .set("state", state(now))
      .set("output", out == 0   ? "off"
                     : out == 1 ? "on"
                                : "unknown")
      .set("settings", actuator_settings(id, now));
}
bool Dl24mDriver::supports(const std::string &op) const {
  return op == "setSinkCutoff" || op == "setSinkDuration" ||
         op == "setOutput" || op == "setLoadCurrent" ||
         op == "getDl24mSettings" || op == "getDl24mRegisters" ||
         op == "getDl24mDiagnostics" || op == "setDl24mBankTarget" ||
         op == "readDl24mBankTarget" || op == "setDl24mSettings" ||
         op == "setDl24mMode" || op == "setDl24mTarget" ||
         op == "resetDl24mCounters";
}
Json Dl24mDriver::describe(uint64_t now) {
  Json j, lim;
  auto caps = Json::array();
  for (auto c : {"load.cc", "output", "voltage.measure", "current.measure",
                 "power.measure", "energy.measure", "charge.measure",
                 "counters", "load.cutoff", "dl24m.settings"})
    caps.append(std::string(c));
  lim.set("voltage_v", limits_.voltage)
      .set("current_a", limits_.current)
      .set("power_w", limits_.power)
      .set("max_age_ms", double(limits_.max_age_ms));
  auto features = Json::array();
  auto feature = [&](const char *id, const char *status, const char *reason,
                     const char *unit, const char *operation, bool off,
                     bool lease) {
    Json value;
    value.set("id", id).set("status", status).set("reason", reason);
    if (*unit)
      value.set("unit", unit);
    if (*operation)
      value.set("operation", operation);
    if (off)
      value.set("requires_off", true);
    if (lease)
      value.set("requires_lease", true);
    features.append(std::move(value));
  };
  feature("cc", "supported",
          "CC 0.001 A steps; configured current and power limits apply", "A",
          "setComponentCurrent", false, true);
  feature("settings", "supported",
          "Audited settings with original-value journal and checked readback",
          "", "setDl24mSettings", true, true);
  feature("counter-reset", "supported",
          "Command 05, OFF required; original counters cannot be restored", "",
          "resetDl24mCounters", true, true);
  feature(
      "diagnostics", "supported",
      "Passive bounded BLE byte chunks and cached registers; no extra commands",
      "raw", "getDl24mDiagnostics", false, false);
  feature("elapsed", "supported",
          "Read 13: packed HH MM SS; individual freshness applies", "s",
          "getMeasurements", false, false);
  feature(
      "temperature-magnitude", "supported",
      "Register 36 encodes the same instrument temperature as 16 at 0.01 "
      "degC resolution. Numeric replies omit the sign; this is a magnitude, "
      "not an external/cell probe or calibrated accuracy guarantee",
      "degC", "getMeasurements", false, false);
  feature("capacity", "supported",
          "Read 14 counts mAh; Ah scale checked against a bounded CC integral. "
          "Absolute accuracy unverified",
          "Ah", "getMeasurements", false, false);
  feature("energy", "supported",
          "Read 15 counts mWh; Wh scale checked against a bounded CC integral. "
          "Absolute accuracy unverified",
          "Wh", "getMeasurements", false, false);
  feature("cv", "supported",
          "0.01 V steps; ON requires fresh OCP/OPP within local limits. "
          "Verified on a 24 V / 0.15 A bench source",
          "V", "setDl24mMode", true, true);
  feature("cr", "supported",
          "0.1 Ohm steps; current/power checked at configured maximum voltage. "
          "Verified at 240 Ohm on a 24 V / 0.15 A source",
          "Ohm", "setDl24mMode", true, true);
  feature(
      "cp", "supported",
      "0.1 W steps; requires fresh nonzero cutoff and current/power bounds. "
      "Verified at 2.4 W on a 24 V / 0.15 A source",
      "W", "setDl24mMode", true, true);
  feature("external-temperature", "unavailable",
          "No audited BLE external probe register on this protocol revision; "
          "internal temperature is not cell temperature",
          "degC", "", false, false);
  feature("fan-rpm", "unavailable",
          "No audited RPM telemetry in this protocol revision", "rpm", "",
          false, false);
  feature("calibration", "unavailable",
          "This native protocol exposes no voltage/current/external-probe gain "
          "read, write or backup equivalent. Command 46 overwrites live internal "
          "temperature, not a persistent gain; it cannot implement calibration",
          "", "", true, true);
  feature("zero", "unavailable",
          "The native protocol has no calibration-offset backup or restore. "
          "Command 4F samples current and rewrites flash calibration; its "
          "effect cannot be independently read back or reversed",
          "", "", true, true);
  feature("factory-reset", "unavailable",
          "Command 4B in this native protocol also overwrites calibration. "
          "There is no coefficient backup/restore query, so the original "
          "calibration-preserving reset cannot be implemented",
          "", "", true, true);
  auto out = output(now);
  j.set("id", id_)
      .set("driver", "dl24m")
      .set("identity", identity_)
      .set("transport", "ble")
      .set("capabilities", caps)
      .set("features", std::move(features))
      .set("state", state(now))
      .set("output", out == 0   ? "off"
                     : out == 1 ? "on"
                                : "unknown")
      .set("limits", lim);
  return j;
}
Json Dl24mDriver::actuator_settings(const std::string &, uint64_t) {
  Json s;
  if (values_.count(0x21) && values_[0x21].first <= 3 && values_.count(0x17)) {
    auto mode = values_[0x21].first;
    s.set("regulation_mode", mode_names[mode]);
    if (mode == 0)
      s.set("current_a", values_[0x17].first / 1000.);
  }
  if (values_.count(0x18))
    s.set("cutoff_voltage_v", values_[0x18].first / 100.);
  if (values_.count(0x19))
    s.set("time_limit_s", double(seconds(values_[0x19].first)));
  return s;
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
  if (values_.count(0x17) && values_.count(0x21) && values_[0x21].first == 0)
    j.set("set_current_a", values_[0x17].first / 1000.)
        .set("regulation_mode", "cc");
  if (values_.count(0x21) && values_[0x21].first <= 3 && values_.count(0x17)) {
    auto mode = values_[0x21].first;
    j.set("regulation_mode", mode_names[mode])
        .set("load_mode", mode_names[mode])
        .set("load_target",
             Json()
                 .set("mode", mode_names[mode])
                 .set("value", values_[0x17].first / mode_scales[mode])
                 .set("unit", mode_units[mode]));
  }
  auto readings = Json::array();
  for (auto entry : {std::make_pair(uint8_t(0x13), "elapsed"),
                     std::make_pair(uint8_t(0x14), "capacity"),
                     std::make_pair(uint8_t(0x15), "energy"),
                     std::make_pair(uint8_t(0x36), "temperature_magnitude")}) {
    Json reading;
    auto c = entry.first;
    reading.set("id", entry.second)
        .set("unit", c == 0x13   ? "s"
                     : c == 0x14 ? "Ah"
                     : c == 0x36 ? "degC"
                                 : "Wh")
        .set("origin", c == 0x13   ? "dl24m.ble.13"
                       : c == 0x14 ? "dl24m.ble.14"
                       : c == 0x36 ? "dl24m.ble.36.magnitude"
                                   : "dl24m.ble.15");
    auto found = values_.find(c);
    bool valid_value = found != values_.end();
    if (valid_value && c == 0x13)
      valid_value = ((found->second.first >> 8) & 255) < 60 &&
                    (found->second.first & 255) < 60;
    reading.set("quality", !valid_value    ? "unavailable"
                           : fresh(c, now) ? "valid"
                                           : "stale");
    if (valid_value) {
      auto value = found->second.first;
      auto scaled = c == 0x13   ? double(seconds(value) + (value & 255))
                    : c == 0x36 ? value / 100.
                                : value / 1000.;
      reading.set("value", scaled)
          .set("received_ms", double(found->second.second))
          .set("age_ms", double(now - found->second.second));
      if ((c == 0x14 || c == 0x15) && fresh(c, now))
        j.set(c == 0x14 ? "capacity_ah" : "energy_wh", scaled);
    }
    readings.append(reading);
  }
  j.set("native_readings", std::move(readings));
  j.set("temperature_channels", temps);
  return j;
}
void Dl24mDriver::begin_stop(uint64_t now) {
  bank_operation_ = false;
  queued_work_ = Work::none;
  queued_steps_.clear();
  journal_.set("stop_pending", true);
  save();
  need_stop_ = true;
  if (tx_.result().state == TxState::running) {
    tx_.cancel_after_io();
    wire_.cancel_pending();
    work_ = Work::drain;
    return;
  }
  work_ = Work::stop;
  tx_.start({{1, 0, 0x10, 0, stop_delay_}}, now, false);
  stop_delay_ = 0;
  next_ = now;
}
void Dl24mDriver::stop(uint64_t now) {
  snapshot_ready_ = false;
  operation_started_ = now;
  result_ = progress();
  result_.set("stage", "stopping");
  begin_stop(now);
}
void Dl24mDriver::finish_user(uint64_t now) {
  auto r = tx_.result();
  result_.set("status", r.state == TxState::succeeded ? "succeeded" : "failed")
      .set("stage", r.state == TxState::succeeded ? "confirmed" : r.reason)
      .set("read_attempts", double(r.read_attempts + snapshot_reads_))
      .set("write_attempts", double(r.write_attempts + snapshot_writes_))
      .set("duration_ms", double(now - operation_started_));
  if (bank_operation_ && r.state == TxState::succeeded)
    result_.set("confirmed_value",
                double(bank_write_ ? bank_target_ : bank_original_));
  bank_operation_ = false;
  if (r.state == TxState::succeeded) {
    restore_.clear();
    journal_.set("restore", Json::array()).set("menu_exit_pending", false);
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
      .set("restore_pending",
           !restore_.empty() || journal_.boolean("menu_exit_pending"));
  return j;
}
void Dl24mDriver::tick(uint64_t now) {
  trace_.clock(now);
  if (generation_ != wire_.generation()) {
    generation_ = wire_.generation();
    menu_checked_ = false;
    values_.clear();
    tx_.cancel();
    if (work_ == Work::user || work_ == Work::snapshot ||
        queued_work_ != Work::none || work_ == Work::drain ||
        work_ == Work::stop) {
      result_.set("status", "failed").set("stage", "connection_changed");
    }
    queued_work_ = Work::none;
    queued_steps_.clear();
    bank_operation_ = false;
    work_ = Work::none;
    need_stop_ = true;
    journal_.set("stop_pending", true);
    save();
    next_ = now;
  }
  if ((work_ == Work::user || work_ == Work::snapshot || work_ == Work::stop ||
       work_ == Work::drain || queued_work_ != Work::none) &&
      result_.text("status") == "running" &&
      now - operation_started_ >= 10000) {
    tx_.cancel();
    queued_work_ = Work::none;
    queued_steps_.clear();
    bank_operation_ = false;
    result_.set("status", "failed")
        .set("stage", "deadline")
        .set("duration_ms", double(now - operation_started_));
    need_stop_ = true;
    work_ = Work::none;
    next_ = now;
  }
  if (!wire_.connected())
    return;
  if ((!values_.count(0x10) || values_[0x10].first != 0) && !need_stop_) {
    bool stale = !fresh(0x11, now) || !fresh(0x12, now) || !fresh(0x10, now);
    bool exceed = false;
    if (!stale) {
      double v = values_[0x11].first / 1000., a = values_[0x12].first / 1000.;
      exceed =
          v > limits_.voltage || a > limits_.current || v * a > limits_.power;
    }
    if (stale || exceed ||
        (active_duration_ms_ && now - enabled_at_ >= active_duration_ms_)) {
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
        if (polling_ == 0x31) {
          journal_.set("menu_exit_pending", false);
          save();
        }
        values_[polling_] = {tx_.result().value, now};
        ++sequence_;
      }
      next_ = state == TxState::succeeded ? now : now + 250;
    } else if (work_ == Work::drain) {
      if (tx_.result().reason == "cancelled_timeout")
        stop_delay_ = 250;
      work_ = Work::none;
      begin_stop(now);
      return;
    } else if (work_ == Work::stop) {
      if (state == TxState::succeeded) {
        values_[0x10] = {0, now};
        need_stop_ = false;
        journal_.set("stop_pending", false).set("may_be_on", false);
        save();
        if (result_.text("status") == "running") {
          result_.set("status", "succeeded")
              .set("stage", "confirmed")
              .set("read_attempts", double(tx_.result().read_attempts))
              .set("write_attempts", double(tx_.result().write_attempts))
              .set("duration_ms", double(tx_.result().duration_ms));
        }
        next_ = now;
      } else {
        values_.erase(0x10);
        next_ = now + 1000;
        result_.set("status", "failed").set("stage", "stop_unconfirmed");
      }
    } else if (work_ == Work::snapshot) {
      if (state == TxState::succeeded) {
        snapshot_reads_ = tx_.result().read_attempts;
        snapshot_writes_ = tx_.result().write_attempts;
        if (snapshot_body_.has("language")) {
          journal_.set("menu_exit_pending", false);
          save();
        }
        snapshot_ready_ = true;
        work_ = Work::none;
        try {
          execute(snapshot_operation_, snapshot_body_, now);
        } catch (const std::exception &e) {
          snapshot_ready_ = false;
          result_.set("status", "failed").set("stage", e.what());
          need_stop_ = true;
        }
        return;
      }
      finish_user(now);
    } else if (work_ == Work::restore) {
      if (state == TxState::succeeded) {
        restore_.clear();
        journal_.set("restore", Json::array()).set("menu_exit_pending", false);
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
  if (queued_work_ != Work::none) {
    auto work = queued_work_;
    auto steps = std::move(queued_steps_);
    auto enable = queued_enabling_;
    queued_work_ = Work::none;
    foreground(work, std::move(steps), now, enable);
    return;
  }
  if (!restore_.empty()) {
    auto steps = restore_;
    if (journal_.boolean("requires_menu")) {
      steps.insert(steps.begin(), {0x31, 0, 0, {}, 0});
      steps.push_back({0x4c, 0, 0, {}, 60});
    }
    work_ = Work::restore;
    tx_.start(std::move(steps), now, false);
    return;
  }
  if (journal_.boolean("menu_exit_pending")) {
    work_ = Work::restore;
    tx_.start({{0x4c, 0, 0, {}, 60}}, now, false);
    return;
  }
  if (!menu_checked_ && output(now) == 0 && poll_ > 12) {
    menu_checked_ = true;
    journal_.set("menu_exit_pending", true);
    save();
    polling_ = 0x31;
    work_ = Work::poll;
    tx_.start({{0, 0, 0x31, {}, 0}, {0x4c, 0, 0, {}, 60}}, now, false);
    return;
  }
  // Fast safety cycle between audited settings reads; settings never starve
  // V/I.
  static constexpr uint8_t fast[] = {0x10, 0x11, 0x12, 0x16, 0x17, 0x21, 0x18,
                                     0x38, 0x39, 0x13, 0x14, 0x15, 0x36};
  constexpr auto fast_count = sizeof(fast) / sizeof(fast[0]);
  if (poll_ % (fast_count + 1) < fast_count)
    polling_ = fast[poll_ % (fast_count + 1)];
  else
    polling_ = dl24m::dl24m_reads[(poll_ / (fast_count + 1)) %
                                  dl24m::dl24m_reads.size()]
                   .command;
  ++poll_;
  work_ = Work::poll;
  tx_.start({{0, 0, polling_, {}, 0}}, now, false);
}
Json Dl24mDriver::execute(const std::string &op, const Json &body,
                          uint64_t now) {
  trace_.clock(now);
  if (op == "getDl24mDiagnostics") {
    auto result = trace_.diagnostics();
    result.set("registers",
               execute("getDl24mRegisters", Json(), now).item("registers"));
    return result;
  }
  if (op == "setSinkCutoff" || op == "setSinkDuration")
    return execute("setDl24mSettings", body, now);

  if (op == "getDl24mRegisters") {
    auto a = Json::array();
    for (auto &r : dl24m::dl24m_reads) {
      Json entry;
      entry.set("name", r.name).set("command", double(r.command));
      auto it = values_.find(r.command);
      if (it == values_.end())
        entry.set("quality", "unavailable");
      else
        entry.set("value", double(it->second.first))
            .set("received_ms", double(it->second.second))
            .set("age_ms", double(now - it->second.second))
            .set("quality",
                 now - it->second.second <= 30000 ? "valid" : "stale");
      a.append(entry);
    }
    Json j;
    j.set("registers", a);
    return j;
  }
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
  if (need_stop_ || !restore_.empty() ||
      journal_.boolean("menu_exit_pending") || !wire_.connected())
    throw std::invalid_argument("Device shutdown/restore not complete");
  if (work_ == Work::user || work_ == Work::snapshot ||
      queued_work_ != Work::none)
    throw std::invalid_argument("Device busy");
  if (op == "setDl24mSettings")
    for (auto &setting : settings)
      if (body.has(setting.name)) {
        auto scaled = body.number(setting.name) * setting.write_scale;
        if (std::abs(scaled - std::round(scaled)) > 1e-6)
          throw std::invalid_argument(
              std::string(setting.name) +
              " is not representable at device resolution");
      }
  if (op == "setDl24mBankTarget" || op == "readDl24mBankTarget") {
    if (output(now) != 0 || !fresh(0x21, now) || values_[0x21].first != 0)
      throw std::invalid_argument("Bank access requires CC and confirmed OFF");
    auto bank = body.number("bank");
    auto target = body.number("target_raw", 0);
    auto maximum = bank == 1 ? 1000 : bank == 2 ? 2400 : 100;
    if (bank < 1 || bank > 3 || std::floor(bank) != bank || target < 0 ||
        target > maximum || std::floor(target) != target)
      throw std::invalid_argument(
          "Bank/target outside conservative supported range");
    bank_operation_ = true;
    bank_write_ = op == "setDl24mBankTarget";
    bank_ = bank;
    bank_target_ = target;
    bank_cc_captured_ = bank_original_captured_ = false;
    operation_started_ = now;
    snapshot_reads_ = snapshot_writes_ = 0;
    std::vector<Step> sequence{{0, 0, 0x10, 0, 0},
                               {0, 0, 0x21, 0, 0},
                               {0, 0, 0x17, {}, 0},
                               {6, bank_, 0x21, bank_, 60},
                               {0, 0, 0x17, {}, 0}};
    if (bank_write_)
      sequence.push_back({2, bank_target_, 0x17, bank_target_, 60});
    sequence.push_back({6, 0, 0x21, 0, 60});
    Step cc{2, 0, 0x17, 0, 60};
    cc.prepare = [this](Step &step) {
      step.value = bank_cc_original_;
      step.expected = bank_cc_original_;
    };
    sequence.push_back(std::move(cc));
    sequence.push_back({0, 0, 0x11, {}, 0});
    sequence.push_back({0, 0, 0x12, {}, 0});
    sequence.push_back({0, 0, 0x10, 0, 0});
    result_ = progress();
    result_.set("stage", "bank_snapshot");
    foreground(Work::user, std::move(sequence), now);
    return operation(now);
  }
  if (op == "setDl24mMode" || op == "setDl24mTarget") {
    auto mode = mode_number(body.text("mode"));
    if (output(now) != 0)
      throw std::invalid_argument("Mode/target changes require confirmed OFF");
    if (op == "setDl24mTarget") {
      if (!fresh(0x21, now) || values_[0x21].first != mode)
        throw std::invalid_argument(
            "Fresh matching selected mode required before target changes");
      double value = body.number("value", -1);
      if (body.text("unit") != mode_units[mode] || !std::isfinite(value) ||
          value < 0 ||
          std::abs(value * mode_scales[mode] -
                   std::round(value * mode_scales[mode])) > 1e-6 ||
          (mode == 0 && value > limits_.current) ||
          (mode == 1 && (value < .5 || value > limits_.voltage)) ||
          (mode == 2 && (value < .1 || value > 240)) ||
          (mode == 3 && (value < .1 || value > limits_.power)))
        throw std::invalid_argument(
            "Target unit, range or native resolution invalid");
    }
    if (!snapshot_ready_) {
      bank_operation_ = false;
      operation_started_ = now;
      snapshot_reads_ = snapshot_writes_ = 0;
      snapshot_body_ = body;
      snapshot_operation_ = op;
      result_ = progress();
      std::vector<Step> reads{{0, 0, 0x10, 0, 0},
                              {0, 0, 0x21, {}, 0},
                              {0, 0, 0x17, {}, 0},
                              {0, 0, 0x10, 0, 0}};
      if (op == "setDl24mTarget")
        reads[1].expected = mode;
      foreground(Work::snapshot, std::move(reads), now);
      return operation(now);
    }
    snapshot_ready_ = false;
    unsigned original_mode = values_[0x21].first;
    if (original_mode > 3 || (op == "setDl24mTarget" && original_mode != mode))
      throw std::invalid_argument(
          "Selected mode changed before target snapshot");
    auto original_target = values_[0x17].first;
    auto records = Json::array();
    records.append(Json()
                       .set("write", 6.)
                       .set("wire", double(original_mode))
                       .set("read", 33.)
                       .set("expected", double(original_mode)));
    records.append(Json()
                       .set("write", 2.)
                       .set("wire", double(original_target))
                       .set("read", 23.)
                       .set("expected", double(original_target)));
    journal_.set("restore", records).set("requires_menu", false);
    save();
    build_restore();
    std::vector<Step> steps{{0, 0, 0x10, 0, 0}};
    if (op == "setDl24mMode") {
      steps.push_back({6, mode, 0x21, mode, 60});
      steps.push_back({0, 0, 0x17, {}, 0});
    } else {
      auto target =
          uint32_t(std::lround(body.number("value") * mode_scales[mode]));
      steps.push_back({0, 0, 0x21, mode, 0});
      steps.push_back({2, target, 0x17, target, 60});
    }
    steps.push_back({0, 0, 0x10, 0, 0});
    steps.push_back({0, 0, 0x11, {}, 0});
    steps.push_back({0, 0, 0x12, {}, 0});
    result_ = progress();
    result_.set("stage", "mode_target_readback");
    foreground(Work::user, std::move(steps), now);
    return operation(now);
  }
  bank_operation_ = false;
  if (!snapshot_ready_) {
    operation_started_ = now;
    snapshot_reads_ = snapshot_writes_ = 0;
  }
  if (op == "setDl24mSettings" && !snapshot_ready_) {
    if (output(now) != 0)
      throw std::invalid_argument("Settings require confirmed OFF");
    std::vector<Step> reads{{0, 0, 0x10, 0, 0}};
    bool menu = false;
    for (auto &setting : settings)
      if (body.has(setting.name)) {
        reads.push_back({0, 0, setting.read, {}, 0});
        if (setting.read == 0x31)
          menu = true;
      }
    if (reads.size() == 1)
      throw std::invalid_argument("No supported setting supplied");
    if (menu) {
      journal_.set("menu_exit_pending", true);
      save();
      reads.push_back({0x4c, 0, 0, {}, 60});
    }
    reads.push_back({0, 0, 0x10, 0, 0});
    snapshot_operation_ = op;
    snapshot_body_ = body;
    result_ = progress();
    result_.set("stage", "snapshot");
    foreground(Work::snapshot, std::move(reads), now);
    return operation(now);
  }
  snapshot_ready_ = false;
  std::vector<Step> steps;
  bool enabling = false;
  if (op == "setOutput") {
    enabling = body.boolean("enabled");
    if (!enabling) {
      stop(now);
      return operation(now);
    }
    if (output(now) != 0 || !fresh(0x11, now) || !fresh(0x12, now) ||
        !fresh(0x17, now) || !fresh(0x21, now) || values_[0x21].first > 3)
      throw std::invalid_argument(
          "Fresh OFF, known mode/target and measurements required");
    auto mode = values_[0x21].first;
    double v = values_[0x11].first / 1000.,
           target = values_[0x17].first / mode_scales[mode];
    double predicted_a = 0, predicted_w = 0;
    if (mode == 0) {
      predicted_a = target;
      predicted_w = v * target;
    }
    if (mode == 1) {
      if (target < .5 || target > limits_.voltage || !fresh(0x38, now) ||
          !fresh(0x39, now) || values_[0x38].first == 0 ||
          values_[0x39].first == 0)
        throw std::invalid_argument("CV requires a valid voltage target and "
                                    "fresh device OCP/OPP ceilings");
      predicted_a = values_[0x38].first / 1000.;
      predicted_w = values_[0x39].first / 1000.;
    }
    if (mode == 2) {
      if (target <= 0)
        throw std::invalid_argument("CR requires positive resistance");
      predicted_a = limits_.voltage / target;
      predicted_w = limits_.voltage * predicted_a;
    }
    if (mode == 3) {
      if (target <= 0 || !fresh(0x18, now) || values_[0x18].first == 0 ||
          v < values_[0x18].first / 100.)
        throw std::invalid_argument("CP requires a positive target and fresh "
                                    "nonzero cutoff below present voltage");
      predicted_a = target / (values_[0x18].first / 100.);
      predicted_w = target;
    }
    if (v > limits_.voltage || predicted_a > limits_.current + 1e-9 ||
        predicted_w > limits_.power + 1e-9)
      throw std::invalid_argument(
          "Mode target/protection ceilings exceed configured current, voltage "
          "or power limits");
    auto duration = body.number("max_duration_ms", limits_.max_duration_ms);
    if (body.has("max_duration_ms") &&
        (duration < 100 || duration > 86400000 ||
         std::floor(duration) != duration ||
         (limits_.max_duration_ms && duration > limits_.max_duration_ms)))
      throw std::invalid_argument("Duration outside configured limit");
    active_duration_ms_ = duration;
    journal_.set("stop_pending", true).set("may_be_on", true);
    save();
    enabled_at_ = now;
    steps = {{0, 0, 0x21, mode, 0},     {0, 0, 0x17, values_[0x17].first, 0},
             {1, 0x010000, 0x10, 1, 0}, {0, 0, 0x11, {}, 0},
             {0, 0, 0x12, {}, 0},       {0, 0, 0x10, 1, 60}};
  } else if (op == "setLoadCurrent") {
    double a = body.number("current_a", -1),
           v = values_.count(0x11) ? values_[0x11].first / 1000.
                                   : limits_.voltage;
    if (a < 0 || a > limits_.current || a * v > limits_.power ||
        !fresh(0x21, now) || values_[0x21].first != 0)
      throw std::invalid_argument("CC mode and in-range current required");
    if (std::abs(a * 1000 - std::round(a * 1000)) > 1e-6)
      throw std::invalid_argument("CC current requires 0.001 A steps");
    auto value = uint32_t(std::lround(a * 1000));
    steps = {{0, 0, 0x21, 0, 0}, {2, value, 0x17, value, 60}};
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
    if (steps.empty())
      throw std::invalid_argument("No supported settings supplied");
    if (menu)
      steps.push_back({0x4c, 0, 0, {}, 60});
    if (body.has("mode"))
      steps.push_back({0, 0, 0x17, {}, 0});
    journal_.set("restore", records)
        .set("requires_menu", menu)
        .set("menu_exit_pending", menu);
    save();
    build_restore();
  } else
    throw std::invalid_argument("Unsupported DL24M operation");
  bool refresh = op == "setDl24mSettings" || !fresh(0x11, now) ||
                 !fresh(0x12, now) || now - values_[0x11].second >= 500 ||
                 now - values_[0x12].second >= 500;
  if (!enabling && refresh) {
    steps.push_back({0, 0, 0x11, {}, 0});
    steps.push_back({0, 0, 0x12, {}, 0});
  }
  // Long menu/settings batches can outlive the cached OFF freshness window.
  // Complete them only with a final OFF read, independently of earlier
  // snapshots.
  if (op == "setDl24mSettings" || op == "resetDl24mCounters")
    steps.push_back({0, 0, 0x10, 0, 0});
  result_ = progress();
  foreground(Work::user, std::move(steps), now, enabling);
  return operation(now);
}
} // namespace labbridge

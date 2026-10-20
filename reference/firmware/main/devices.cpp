#include "app.hpp"
#include <algorithm>
#include <cstdio>
#include <limits>

namespace lab {
struct Device {
  std::shared_ptr<Link> link;
  std::string serial, model;
  Reading reading;
  double set_voltage = 0, set_current = 0;
  int output = -1, protection = 0;
  uint64_t received = 0, sequence = 0;
  bool connected = false;
};
static Device load, psus[2];
static AtorchStream load_stream;
static bool load_desynced = false;
static Bytes last_load_report;
static Bytes last_load_reply;
static Bytes probe_rx;
static bool collecting_probe = false;
static bool numeric_ble_profile() {
#if CONFIG_IDF_TARGET_ESP32
  return false;
#else
  return config.load_profile == "dl24m-ble";
#endif
}
static bool strict_load_profile() {
#if CONFIG_IDF_TARGET_ESP32
  return true;
#else
  return numeric_ble_profile();
#endif
}
static Lease lease;
static std::string stop_reason;
static std::array<int, 3> stopped_outputs{-1, -1, -1};
struct SettingCheck {
  const char *name;
  uint8_t write, read;
  uint32_t trial_wire, trial_expected, read_factor;
  uint8_t mode = 0; // Nonzero only for testing that mode's separate setpoint.
};
// Deliberately excludes calibration, factory reset and unknown special tests.
// Values are wire/readback units; time uses packed HH MM 00 on readback.
static const SettingCheck setting_checks[] = {
    {"cutoff_voltage", 3, 0x18, 8, 80, 10},
    {"discharge_time", 4, 0x19, 60, 0x000100, 0},
    {"working_brightness", 0x42, 0x32, 56, 56, 1},
    {"standby_brightness", 0x43, 0x33, 54, 54, 1},
    {"standby_time", 0x44, 0x34, 5, 5, 1},
    {"current_resolution_1", 0x45, 0x35, 1, 1, 1},
    {"current_resolution_2", 0x45, 0x35, 2, 2, 1},
    {"overvoltage", 0x47, 0x37, 3000, 3000, 1},
    {"overcurrent", 0x48, 0x38, 100, 100, 1},
    {"overpower", 0x49, 0x39, 100, 1000, 10},
    {"overtemperature", 0x4d, 0x3a, 90, 90, 1},
    {"fan_temperature", 0x4e, 0x3b, 50, 50, 1},
    {"battery_overtemperature", 0x50, 0x3c, 55, 55, 1},
    {"mode_1", 6, 0x21, 1, 1, 1},
    {"mode_2", 6, 0x21, 2, 2, 1},
    {"mode_3", 6, 0x21, 3, 3, 1},
    {"setpoint_mode_1", 2, 0x17, 1000, 1000, 1, 1},
    {"setpoint_mode_2", 2, 0x17, 2400, 2400, 1, 2},
    {"setpoint_mode_3", 2, 0x17, 10, 10, 1, 3},
    {"language", 0x41, 0x31, 1, 1, 1},
};
static const SettingCheck *setting_check(const std::string &name) {
  for (const auto &s : setting_checks)
    if (name == s.name)
      return &s;
  return nullptr;
}
static bool settings_pending() { return !config.load_restore_setting.empty(); }
static bool restore_load_setting();
static const char *ids[] = {"load", "psu1", "psu2"};
static bool fresh(const Device &d) {
  return d.connected && d.received && now_ms() - d.received <= 3000;
}
static Device *device(int id) {
  if (id == 0)
    return &load;
  Device *found = nullptr;
  for (auto &p : psus)
    if (p.connected && !config.serials[id - 1].empty() &&
        p.serial == config.serials[id - 1]) {
      if (found)
        return nullptr;
      found = &p;
    }
  return found;
}
static cJSON *error(int &status, int code, const char *message) {
  status = code;
  auto j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "error", message);
  return j;
}
static cJSON *success() {
  auto j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "status", "confirmed");
  return j;
}
static bool rd_read(Device &d, std::array<uint16_t, 20> &regs) {
  if (!d.link || !d.link->connected())
    return false;
  d.link->receive();
  if (!d.link->send(modbus_read(0, 20)))
    return false;
  Bytes response;
  auto end = now_ms() + 500;
  while (now_ms() < end && d.link->connected()) {
    auto b = d.link->receive();
    response.insert(response.end(), b.begin(), b.end());
    if (response.size() >= 45)
      break;
    if (response.size() >= 5 && (response[1] & 0x80))
      break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!modbus_reply(response, 3, 40))
    return false;
  for (int i = 0; i < 20; i++)
    regs[i] = be16(&response[3 + 2 * i]);
  return regs[0] / 10 == 6024 && regs[18] <= 1;
}
static bool rd_poll(Device &d) {
  std::array<uint16_t, 20> r{};
  if (!rd_read(d, r)) {
    d.connected = false;
    d.output = -1;
    return false;
  }
  d.serial = std::to_string((uint32_t(r[1]) << 16) | r[2]);
  d.model = "RD6024";
  d.set_voltage = r[8] / 100.;
  d.set_current = r[9] / 100.;
  d.reading.voltage = r[10] / 100.;
  d.reading.current = r[11] / 100.;
  d.reading.power = ((uint32_t(r[12]) << 16) | r[13]) / 100.;
  d.reading.temperature = (r[4] ? -1 : 1) * int(r[5]);
  if (d.reading.voltage > 65 || d.reading.current > 26) {
    d.connected = false;
    return false;
  }
  d.output = r[18];
  d.protection = r[16];
  d.received = now_ms();
  d.sequence++;
  d.connected = true;
  return true;
}
static bool rd_write(Device &d, uint16_t reg, uint16_t value) {
  auto original = d.serial;
  // Re-read identity before a write, including after rapid
  // disconnect/reconnect.
  if (!rd_poll(d) || d.serial != original)
    return false;
  // Validate against the just-read setpoints, not the previous telemetry cache.
  if (!(reg == 18 && value == 0)) {
    int id = -1;
    for (int i = 1; i < 3; i++)
      if (device(i) == &d)
        id = i;
    if (id < 0)
      return false;
    const auto &limits = config.limits[id];
    double v = reg == 8 ? value / 100. : d.set_voltage;
    double a = reg == 9 ? value / 100. : d.set_current;
    if (!limits.valid())
      return false;
    if (d.output != 0 || reg == 18) {
      if (!limits.contains(v, a))
        return false;
    } else if ((reg == 8 && v > limits.voltage) ||
               (reg == 9 && a > limits.current))
      return false;
  }
  auto request = modbus_write(reg, value);
  d.link->receive();
  if (!d.link->send(request))
    return false;
  Bytes response;
  auto end = now_ms() + 500;
  while (now_ms() < end) {
    auto b = d.link->receive();
    response.insert(response.end(), b.begin(), b.end());
    if (response.size() >= 8)
      break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (response != request || !modbus_reply(response, 6, 0)) {
    d.output = -1;
    return false;
  }
  if (!rd_poll(d) || d.serial != original)
    return false;
  if (reg == 8)
    return std::abs(d.set_voltage - value / 100.) < 0.001;
  if (reg == 9)
    return std::abs(d.set_current - value / 100.) < 0.001;
  return reg == 18 && d.output == value;
}
static void report_frame(const Bytes &f) {
  Reading r;
  if (atorch_report(f, config.ignore_checksum, r)) {
    load.reading = r;
    load.received = now_ms();
    load.sequence++;
  }
}
static std::vector<Bytes> load_frames() {
  auto bytes = load.link->receive();
  if (collecting_probe && probe_rx.size() + bytes.size() <= 512)
    probe_rx.insert(probe_rx.end(), bytes.begin(), bytes.end());
  auto frames = load_stream.feed(bytes.data(), bytes.size());
  for (auto &f : frames) {
    if (f.size() == 8 && f[0] == 0xff && f[1] == 0x55 && f[2] == 0x02)
      last_load_reply = f;
    if (f.size() == 36) {
      last_load_report = f;
      report_frame(f);
    }
  }
  return frames;
}
static bool px(const Bytes &request, bool query, uint32_t &result,
               uint32_t timeout_ms = 500) {
  if (load_desynced || !load.link || !load.link->connected())
    return false;
  load_frames(); // discard old replies, preserving unsolicited telemetry
  if (!load.link->send(request))
    return false;
  auto end = now_ms() + timeout_ms;
  while (now_ms() < end && load.link->connected()) {
    for (auto &f : load_frames()) {
      if (query && f.size() == 7 && f[0] == 0xca) {
        result = be24(&f[2]);
        return true;
      }
      if (!query && f.size() == 1 && f[0] == 0x6f)
        return true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  // PX100 replies carry no request ID. A timeout invalidates the stream until
  // reconnect, so a late answer cannot be mistaken for another register.
  load_desynced = true;
  load.output = -1;
  return false;
}
static bool probe_px3(uint8_t command, uint32_t &value);
static bool load_query(uint8_t command, uint32_t &value) {
#if CONFIG_IDF_TARGET_ESP32
  if (load_desynced || !load.link || !load.link->connected())
    return false;
  load_frames();
  if (!load.link->send(spp_command(command)))
    return false;
  const auto deadline = now_ms() + 1500;
  while (now_ms() < deadline && load.link->connected()) {
    for (auto &frame : load_frames())
      if (spp_reply(frame, command, value))
        return true;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  load_desynced = true;
  load.output = -1;
  select_spp(config.spp_address);
  return false;
#else
  if (numeric_ble_profile()) {
    load_frames();
    return probe_px3(command, value);
  }
  return px(px_command(command), true, value);
#endif
}
static bool probe_px3(uint8_t command, uint32_t &value) {
  if (load_desynced || !load.link || !load.link->connected() ||
      !load.link->send(px3_command(command)))
    return false;
  auto end = now_ms() + 2000;
  while (now_ms() < end && load.link->connected()) {
    for (const auto &frame : load_frames()) {
      if (px3_reply(frame, command, value))
        return true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  load_desynced = true;
  load.output = -1;
  return false;
}
static void pending_load_stop(bool pending) {
  if (config.load_stop_pending != pending) {
    config.load_stop_pending = pending;
    save_config();
  }
}
static bool numeric_ble_send(uint8_t command, uint32_t value) {
  if (command == 1)
    pending_load_stop(true); // Persist before ON or any unconfirmed OFF.
  vTaskDelay(pdMS_TO_TICKS(500));
  bool sent = load.link && load.link->connected() &&
              load.link->send(px3_command(command, value));
  if (sent)
    vTaskDelay(pdMS_TO_TICKS(500)); // Device processing, separate from ATT ACK.
  return sent;
}
// Independent of commissioning, freshness, CC mode and lease ownership.
// A pending diagnostic in monitor mode uses this same known numeric protocol.
static bool numeric_ble_off() {
  pending_load_stop(true);
  bool sent = numeric_ble_send(1, 0);
  if (!sent || load_desynced)
    return false;
  auto query = [](uint8_t command, uint32_t &value) {
    vTaskDelay(pdMS_TO_TICKS(500));
    load_frames();
    return probe_px3(command, value);
  };
  uint32_t state = 2, current = 0;
  if (!query(0x10, state) || state != 0 ||
      !query(0x12, current) || current > 5) {
    load.output = -1;
    return false;
  }
  load.output = 0;
  // Do not refresh the timestamp for the whole reading: voltage is unchanged.
  load.reading.current = current / 1000.;
  load.reading.power = load.reading.voltage * load.reading.current;
  pending_load_stop(false);
  return true;
}
static bool numeric_ble_sample(bool temperature = false) {
  uint32_t voltage = 0, current = 0, temp = 0;
  if (!load_query(0x11, voltage) || !load_query(0x12, current) ||
      voltage > 210000 || current > 45000 ||
      (temperature && (!load_query(0x16, temp) || temp > 1500)))
    return false;
  load.reading.voltage = voltage / 1000.;
  load.reading.current = current / 1000.;
  load.reading.power = load.reading.voltage * load.reading.current;
  if (temperature)
    load.reading.temperature = temp / 10.;
  load.received = now_ms();
  ++load.sequence;
  return true;
}
static bool load_output(bool value) {
  if (numeric_ble_profile()) {
    if (!value)
      return numeric_ble_off();
    if (load_desynced) {
      load.output = -1;
      return false;
    }
    uint32_t state = 0, mode = 255, target = 0, voltage = 0;
    if (value) {
      if (!config.load_verified || !fresh(load) ||
          !load_query(0x21, mode) || mode != 0 ||
          !load_query(0x17, target) || !load_query(0x11, voltage) ||
          !config.limits[0].contains(voltage / 1000., target / 1000.))
        return false;
      load.set_current = target / 1000.;
    }
    bool sent = numeric_ble_send(1, value ? 0x010000 : 0);
    if (!sent || !load_query(0x10, state) || state != (value ? 1u : 0u)) {
      if (value)
        numeric_ble_send(1, 0);
      load.output = -1;
      return false;
    }
    load.output = value;
    return numeric_ble_sample();
  }
#if CONFIG_IDF_TARGET_ESP32
  if (!load.link || !load.link->connected())
    return false;
  if (load_desynced) {
    if (!value)
      load.link->send(spp_command(0x01, 0));
    load.output = -1;
    return false;
  }
  uint32_t readback = 0;
  if (value) {
    if (!config.load_verified || !fresh(load) ||
        !load_query(0x17, readback))
      return false;
    load.set_current = readback / 1000.;
    if (!config.limits[0].contains(load.reading.voltage, load.set_current))
      return false;
  }
  if (!load.link->send(spp_command(0x01, value ? 1 : 0)) ||
      !load_query(0x10, readback) || readback != (value ? 1u : 0u)) {
    if (value && load.link->connected())
      load.link->send(spp_command(0x01, 0));
    load.output = -1;
    return false;
  }
  load.output = value;
  return true;
#else
  uint32_t r = 0;
  if (load_desynced) {
    // stop() calls this after handling sources; a late untagged answer cannot
    // confirm this best-effort OFF command.
    if (!value && load.link && load.link->connected())
      load.link->send(px_command(1, 0));
    select_ble(config.ble_address);
    load.output = -1;
    return false;
  }
  if (value) {
    if (!config.load_verified || !load_query(0x17, r))
      return false;
    load.set_current =
        r / (config.load_profile == "px100-milli-read" ? 1000. : 100.);
    if (!config.limits[0].contains(load.reading.voltage, load.set_current))
      return false;
  }
  if (!px(px_command(1, value ? 1 : 0), false, r) || !load_query(0x10, r) ||
      r != (value ? 1u : 0u)) {
    load.output = -1;
    return false;
  }
  load.output = value;
  return true;
#endif
}
static bool load_current(double value) {
  if (numeric_ble_profile()) {
    if (!std::isfinite(value) || value < 0 || value > 40 ||
        std::abs(value * 1000 - std::round(value * 1000)) > 0.0001 ||
        load_desynced)
      return false;
    uint32_t mode = 255, state = 2, voltage = 0, readback = 0;
    if (!load_query(0x21, mode) || mode != 0 ||
        !load_query(0x10, state) || state > 1 ||
        !config.limits[0].valid() || value > config.limits[0].current)
      return false;
    if (state && (!config.load_verified || !load_query(0x11, voltage) ||
                  !config.limits[0].contains(voltage / 1000., value)))
      return false;
    uint32_t raw = std::lround(value * 1000);
    bool confirmed = false;
    for (unsigned attempt = 0; attempt < (state == 0 ? 2u : 1u); ++attempt) {
      // A retry is safe only with the output still OFF and in CC. Never
      // resend an unconfirmed current change into a live load.
      if (attempt && (!load_query(0x10, state) || state != 0 ||
                      !load_query(0x21, mode) || mode != 0))
        break;
      if (!numeric_ble_send(2, raw) || !load_query(0x17, readback))
        break;
      if (readback == raw) {
        confirmed = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      if (!load_query(0x17, readback))
        break;
      if (readback == raw) {
        confirmed = true;
        break;
      }
    }
    if (!confirmed) {
      load.set_current = 0;
      return false;
    }
    load.output = state;
    load.set_current = readback / 1000.;
    return numeric_ble_sample();
  }
#if CONFIG_IDF_TARGET_ESP32
  if (!std::isfinite(value) || value < 0 || value > 40 ||
      std::abs(value * 1000 - std::round(value * 1000)) > 0.0001 ||
      load_desynced || !load.link || !load.link->connected() ||
      load.output < 0 || (load.output == 1 && !config.load_verified))
    return false;
  const auto raw = uint32_t(std::lround(value * 1000));
  uint32_t readback = 0;
  if (!load.link->send(spp_command(0x02, raw)) ||
      !load_query(0x17, readback) || readback != raw) {
    load.set_current = 0;
    return false;
  }
  load.set_current = readback / 1000.;
  return true;
#else
  uint32_t r = 0;
  auto packet = px_current(value);
  if (packet.empty() || !px(packet, false, r) || !load_query(0x17, r))
    return false;
  double scale = config.load_profile == "px100-milli-read" ? 1000. : 100.;
  load.set_current = r / scale;
  return std::abs(load.set_current - value) < 0.001;
#endif
}
static bool output(int id, bool on) {
  auto d = device(id);
  return d && d->link && d->link->connected() &&
         (id == 0 ? load_output(on) : rd_write(*d, 18, on ? 1 : 0));
}
static void stop(const std::string &reason) {
  lease.stopping = true;
  stop_reason = reason;
  stopped_outputs = {-1, -1, -1};
  if (numeric_ble_profile() && lease.devices[0])
    pending_load_stop(true);
  for (int id : {1, 2, 0})
    if (lease.devices[id])
      stopped_outputs[id] = output(id, false) ? 0 : -1;
  // A lost SPP connection or contradictory readback requires a new physical
  // commissioning check; a later reconnect must not silently restore writes.
  if (strict_load_profile() && lease.devices[0] &&
      (reason == "command_unconfirmed" || reason == "device_unavailable" ||
       stopped_outputs[0] < 0)) {
    config.load_verified = false;
    save_config();
  }
  lease.clear();
}
static bool writable(int id) {
#if CONFIG_IDF_TARGET_ESP32
  return id == 0 && config.load_verified;
#else
  return id != 0 || config.load_verified;
#endif
}
static void poll_once(uint64_t &last) {
  Guard g;
  const bool was_connected = load.connected;
  load.connected = load.link->connected();
  if (load.connected)
    load_frames();
  else {
    if (strict_load_profile() && was_connected && config.load_verified) {
      config.load_verified = false;
      save_config();
    }
    load.output = -1;
    load.received = 0;
    load_stream.clear();
    load_desynced = false;
  }
  if (now_ms() - last >= 1000) {
    last = now_ms();
    if (settings_pending() && !lease.active()) {
      if (config.load_verified) {
        config.load_verified = false;
        save_config();
      }
      if (load.connected)
        restore_load_setting();
      if (load_desynced)
        select_ble(config.ble_address);
      last = now_ms();
    }
    if (config.load_stop_pending && (!lease.active() || !lease.devices[0])) {
      if (config.load_verified) {
        config.load_verified = false;
        save_config();
      }
      if (load.connected && numeric_ble_off())
        stopped_outputs[0] = 0;
      if (load_desynced)
        select_ble(config.ble_address);
      last = now_ms();
      // An unrelated source lease still needs its watchdog below.
    }
    for (auto &p : psus)
      if (p.link && p.link->connected())
        rd_poll(p);
      else {
        p.connected = false;
        p.output = -1;
      }
    if (load.connected && config.load_verified && !load_desynced) {
      uint32_t v, mode = 255;
      bool mode_ok = !numeric_ble_profile() ||
                     (load_query(0x21, mode) && mode == 0);
      if (mode_ok && load_query(0x10, v) && v <= 1)
        load.output = v;
      else {
        load.output = -1;
        if (strict_load_profile()) {
          config.load_verified = false;
          save_config();
        }
      }
      if (numeric_ble_profile() && config.load_verified) {
        if (!numeric_ble_sample(true)) {
          load.output = -1;
          config.load_verified = false;
          save_config();
        }
        // Do not start the next multi-query poll immediately just because
        // this cycle itself took longer than one second.
        last = now_ms();
      }
    }
  }
  if (lease.active()) {
    std::string fault;
    if (lease.expired(now_ms()))
      fault = "lease_expired";
    else if (lease.require_wifi && !wifi_connected())
      fault = "wifi_disconnected";
    for (int i = 0; i < 3; i++)
      if (lease.devices[i]) {
        auto d = device(i);
        if (!d || !fresh(*d) || d->output < 0)
          fault = "device_unavailable";
        else if (d->protection || !config.limits[i].contains(
                                      d->reading.voltage, d->reading.current))
          fault = "measurement_limit_or_protection";
      }
    if (!fault.empty())
      stop(fault);
  }
  if (!lease.active() && load_desynced)
#if CONFIG_IDF_TARGET_ESP32
    select_spp(config.spp_address);
#else
    select_ble(config.ble_address);
#endif
}
static void poll_task(void *) {
  uint64_t last = 0;
  for (;;) {
    poll_once(last);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void start_devices() {
#if CONFIG_IDF_TARGET_ESP32
  load.link = spp_link();
#else
  load.link = ble_link();
#endif
  load.model = "DL24M-A";
  auto links = usb_links();
  for (int i = 0; i < 2; i++)
    psus[i].link = links[i];
  xTaskCreate(poll_task, "measurements", 6144, nullptr, 4, nullptr);
}
static cJSON *limits_json(const Limits &l) {
  auto j = cJSON_CreateObject();
  cJSON_AddNumberToObject(j, "voltage_v", l.voltage);
  cJSON_AddNumberToObject(j, "current_a", l.current);
  cJSON_AddNumberToObject(j, "power_w", l.power);
  return j;
}
cJSON *configuration_json() {
  auto j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "ble_address", config.ble_address.c_str());
  cJSON_AddStringToObject(j, "spp_address", config.spp_address.c_str());
  cJSON_AddStringToObject(j, "spp_pin", config.spp_pin.c_str());
  cJSON_AddStringToObject(j, "load_transport",
#if CONFIG_IDF_TARGET_ESP32
                          "spp"
#else
                          "ble"
#endif
  );
  cJSON_AddStringToObject(j, "load_profile", config.load_profile.c_str());
  cJSON_AddBoolToObject(j, "ignore_checksum", config.ignore_checksum);
  cJSON_AddBoolToObject(j, "load_verified", config.load_verified);
  cJSON_AddBoolToObject(j, "load_stop_pending", config.load_stop_pending);
  cJSON_AddStringToObject(j, "load_restore_setting", config.load_restore_setting.c_str());
  cJSON_AddNumberToObject(j, "load_restore_wire", config.load_restore_wire);
  cJSON_AddNumberToObject(j, "load_restore_expected", config.load_restore_expected);
  cJSON_AddNumberToObject(j, "load_restore_cc", config.load_restore_cc);
  cJSON_AddStringToObject(j, "psu1_serial", config.serials[0].c_str());
  cJSON_AddStringToObject(j, "psu2_serial", config.serials[1].c_str());
  auto a = cJSON_AddArrayToObject(j, "limits");
  for (auto &l : config.limits)
    cJSON_AddItemToArray(a, limits_json(l));
  return j;
}
static cJSON *device_json(int id) {
  auto j = cJSON_CreateObject();
  auto d = device(id);
  cJSON_AddStringToObject(j, "id", ids[id]);
  cJSON_AddStringToObject(j, "model", id ? "RD6024" : "DL24M-A");
  cJSON_AddStringToObject(j, "transport", id ? "usb" :
#if CONFIG_IDF_TARGET_ESP32
                                                 "spp"
#else
                                                 "ble"
#endif
  );
  cJSON_AddStringToObject(j, "identity",
                          id ? config.serials[id - 1].c_str()
                             :
#if CONFIG_IDF_TARGET_ESP32
                             config.spp_address.c_str()
#else
                             config.ble_address.c_str()
#endif
  );
  cJSON_AddBoolToObject(j, "connected", d && d->connected);
  cJSON_AddBoolToObject(j, "fresh", d && fresh(*d));
  cJSON_AddBoolToObject(j, "writable", writable(id));
  cJSON_AddItemToObject(j, "limits", limits_json(config.limits[id]));
  auto capabilities = cJSON_AddArrayToObject(j, "capabilities");
  cJSON_AddItemToArray(capabilities, cJSON_CreateString("measure"));
  if (!id) {
    cJSON_AddBoolToObject(j, "load_stop_pending", config.load_stop_pending);
    if (numeric_ble_profile() || config.load_stop_pending)
      cJSON_AddItemToArray(capabilities, cJSON_CreateString("emergency_stop"));
  }
  if (writable(id)) {
    cJSON_AddItemToArray(capabilities, cJSON_CreateString("output"));
    cJSON_AddItemToArray(capabilities, cJSON_CreateString("current_a"));
    if (id)
      cJSON_AddItemToArray(capabilities, cJSON_CreateString("voltage_v"));
  }
  if (d) {
    cJSON_AddNumberToObject(j, "sequence", d->sequence);
    cJSON_AddNumberToObject(j, "received_ms", d->received);
    cJSON_AddNumberToObject(
        j, "age_ms", d->received ? double(now_ms() - d->received) : -1.0);
    if (d->output < 0)
      cJSON_AddNullToObject(j, "output");
    else
      cJSON_AddBoolToObject(j, "output", d->output);
    cJSON_AddNumberToObject(j, "voltage_v", d->reading.voltage);
    cJSON_AddNumberToObject(j, "current_a", d->reading.current);
    cJSON_AddNumberToObject(j, "power_w", d->reading.power);
    cJSON_AddNumberToObject(j, "temperature_c", d->reading.temperature);
    cJSON_AddNumberToObject(j, "set_voltage_v", d->set_voltage);
    cJSON_AddNumberToObject(j, "set_current_a", d->set_current);
    cJSON_AddNumberToObject(j, "protection", d->protection);
    if (!id) {
      cJSON_AddNumberToObject(j, "capacity_ah", d->reading.capacity_ah);
      cJSON_AddNumberToObject(j, "energy_wh", d->reading.energy_wh);
    }
  }
  return j;
}
cJSON *devices_json() {
  auto j = cJSON_CreateObject();
  auto a = cJSON_AddArrayToObject(j, "devices");
  for (int i = 0; i < 3; i++)
    cJSON_AddItemToArray(a, device_json(i));
  return j;
}
cJSON *discovery_json() {
  auto j = cJSON_CreateObject();
  auto a = cJSON_AddArrayToObject(j, "ble");
  auto found = discovered_ble();
  for (auto &s : found)
    cJSON_AddItemToArray(a, cJSON_CreateString(s.c_str()));
  // The selected device stops advertising while connected, and some DL24M
  // advertisements omit the name. Keep the connected device selectable.
  bool listed = std::any_of(found.begin(), found.end(), [](const auto &s) {
    return s.size() >= 17 && s.compare(0, 17, config.ble_address) == 0;
  });
  if (load.connected && !config.ble_address.empty() && !listed)
    cJSON_AddItemToArray(
        a, cJSON_CreateString((config.ble_address + " DL24M-A (verbunden)").c_str()));
  a = cJSON_AddArrayToObject(j, "spp");
  auto spp_found = discovered_spp();
  for (auto &s : spp_found)
    cJSON_AddItemToArray(a, cJSON_CreateString(s.c_str()));
  bool spp_listed = std::any_of(spp_found.begin(), spp_found.end(),
                                [](const auto &s) {
                                  return s.size() >= 17 &&
                                         s.compare(0, 17, config.spp_address) == 0;
                                });
  if (load.connected && !config.spp_address.empty() && !spp_listed)
    cJSON_AddItemToArray(
        a, cJSON_CreateString((config.spp_address + " DL24M_SPP (verbunden)").c_str()));
  a = cJSON_AddArrayToObject(j, "usb");
  for (auto &p : psus)
    if (p.connected) {
      auto v = cJSON_CreateObject();
      cJSON_AddStringToObject(v, "model", p.model.c_str());
      cJSON_AddStringToObject(v, "serial", p.serial.c_str());
      cJSON_AddItemToArray(a, v);
    }
  return j;
}
static cJSON *session_json() {
  auto j = cJSON_CreateObject();
  cJSON_AddBoolToObject(j, "active", lease.active());
  cJSON_AddStringToObject(j, "last_stop_reason", stop_reason.c_str());
  cJSON_AddBoolToObject(j, "load_stop_pending", config.load_stop_pending);
  auto a = cJSON_AddArrayToObject(j, "stop_outputs");
  for (int v : stopped_outputs)
    cJSON_AddItemToArray(a,
                         v < 0 ? cJSON_CreateNull() : cJSON_CreateBool(false));
  return j;
}
static bool numeric_field(const cJSON *j, const char *k) {
  auto v = cJSON_GetObjectItemCaseSensitive(j, k);
  return !v || (cJSON_IsNumber(v) && std::isfinite(v->valuedouble));
}
cJSON *load_bench_pulse(const cJSON *confirmation, int &status) {
#if CONFIG_IDF_TARGET_ESP32
  (void)confirmation;
  return error(status, 422, "ble_toggle_not_supported_on_spp_board");
#else
  // Local, attended commissioning only. Never expose a general toggle API:
  // this command cannot establish an absolute output state or set a current.
  status = 200;
  if (!flag(confirmation, "cc_0_10a_output_off_confirmed") ||
      !flag(confirmation, "source_24v_1a_confirmed"))
    return error(status, 400, "bench_conditions_required");
  if (settings_pending() || config.load_stop_pending || lease.active() || config.load_profile != "monitor" ||
      config.load_verified || load_desynced || !fresh(load))
    return error(status, 409, "idle_fresh_monitor_required");
  auto baseline_ok = [] {
    return fresh(load) && load.reading.voltage >= 22 &&
           load.reading.voltage <= 25 && load.reading.current <= 0.005 &&
           load.reading.temperature < 40;
  };
  if (!baseline_ok())
    return error(status, 409, "bench_baseline_outside_limits");
  auto result = cJSON_CreateObject();
  auto samples = cJSON_AddArrayToObject(result, "samples");
  const auto started = now_ms();
  uint64_t recorded_sequence = 0;
  auto sample = [&](const char *phase) {
    load.connected = load.link->connected();
    load_frames();
    if (!fresh(load) || load.sequence == recorded_sequence)
      return false;
    recorded_sequence = load.sequence;
    auto s = cJSON_CreateObject();
    cJSON_AddStringToObject(s, "phase", phase);
    cJSON_AddNumberToObject(s, "elapsed_ms", now_ms() - started);
    cJSON_AddNumberToObject(s, "sequence", load.sequence);
    cJSON_AddNumberToObject(s, "voltage_v", load.reading.voltage);
    cJSON_AddNumberToObject(s, "current_a", load.reading.current);
    cJSON_AddNumberToObject(s, "power_w", load.reading.power);
    cJSON_AddNumberToObject(s, "temperature_c", load.reading.temperature);
    cJSON_AddItemToArray(samples, s);
    return true;
  };
  unsigned baseline_samples = 0;
  while (now_ms() - started < 3500 && baseline_samples < 3) {
    if (sample("before"))
      ++baseline_samples;
    if (!baseline_ok())
      break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if (baseline_samples < 3 || !baseline_ok()) {
    cJSON_Delete(result);
    return error(status, 409, "stable_zero_current_baseline_required");
  }
  // ATORCH DC remote OK/ON-OFF key, checksum (0x11+0x02+0x32)^0x44.
  const Bytes toggle{0xff, 0x55, 0x11, 0x02, 0x32, 0, 0, 0, 0, 0x01};
  const bool app_write = flag(confirmation, "app_write_no_response", true);
  cJSON_AddStringToObject(result, "ble_write_mode",
                          app_write ? "without_response" : "request");
  auto send_toggle = [&] {
    return app_write ? load.link->send_without_response(toggle)
                     : load.link->send(toggle);
  };
  auto reply_hex = [] {
    std::string hex;
    for (uint8_t byte : last_load_reply) {
      char b[3];
      snprintf(b, sizeof(b), "%02x", byte);
      hex += b;
    }
    return hex;
  };
  load_frames(); // Keep a late reply from being attributed to this keypress.
  last_load_reply.clear();
  const auto enabled_at = now_ms();
  bool first_sent = send_toggle();
  bool activation = false, stop_sent = false, zero_after_stop = false;
  unsigned matching_samples = 0;
  std::string reason = first_sent ? "activation_not_observed" : "write_failed";
  while (first_sent && now_ms() - enabled_at < 4000) {
    if (sample("active")) {
      if (load.reading.current > 0.02)
        activation = true;
      if (load.reading.current > 0.15 || load.reading.voltage < 22 ||
          load.reading.voltage > 25 || load.reading.temperature >= 45) {
        reason = "measurement_outside_bench_limits";
        break;
      }
      if (std::abs(load.reading.current - 0.1) <= 0.02)
        ++matching_samples;
    }
    if (!load.connected || now_ms() - load.received > 2000) {
      reason = "telemetry_lost";
      break;
    }
    if (now_ms() - enabled_at >= 3000 && activation) {
      reason = matching_samples >= 2 ? "measured_pulse_completed"
                                     : "current_did_not_match_preset";
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  cJSON_AddStringToObject(result, "reply_after_start_hex", reply_hex().c_str());
  cJSON_AddItemToObject(result, "ble_after_start", ble_diagnostics());
  // A second toggle is sent only with fresh evidence of current flow.
  // Never retry a toggle, and never interpret zero current as an absolute OFF.
  if (activation && load.link->connected() &&
      now_ms() - load.received <= 2000 && load.reading.current > 0.02) {
    load_frames();
    last_load_reply.clear();
    stop_sent = send_toggle();
    auto stop_at = now_ms();
    unsigned zero_samples = 0;
    while (now_ms() - stop_at < 4000) {
      if (sample("after")) {
        zero_samples = baseline_ok() ? zero_samples + 1 : 0;
        if (zero_samples >= 2) {
          zero_after_stop = true;
          break;
        }
      }
      if (!load.connected)
        break;
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  cJSON_AddStringToObject(result, "reply_after_stop_hex",
                          stop_sent ? reply_hex().c_str() : "");
  cJSON_AddItemToObject(result, "ble_after_stop", ble_diagnostics());
  load.output = -1;
  cJSON_AddStringToObject(result, "reason", reason.c_str());
  cJSON_AddBoolToObject(result, "activation_observed", activation);
  cJSON_AddNumberToObject(result, "matching_current_samples", matching_samples);
  cJSON_AddBoolToObject(result, "stop_toggle_sent", stop_sent);
  cJSON_AddBoolToObject(result, "current_zero_after_stop", zero_after_stop);
  cJSON_AddBoolToObject(result, "manual_check_required", !zero_after_stop);
  cJSON_AddBoolToObject(result, "load_verified", false);
  return result;
#endif
}
// Local commissioning of the recovered 9-byte protocol. Fixed, output-off
// experiment; optional bounded 0.100 A pulse, no production profile enable.
static cJSON *load_numeric_write_check(int &status, bool restore_only,
                                       bool pulse, unsigned gap_ms = 500) {
#if CONFIG_IDF_TARGET_ESP32
  (void)restore_only;
  (void)pulse;
  (void)gap_ms;
  return error(status, 422, "ble_board_required");
#else
  if (restore_only && pulse)
    return error(status, 400, "restore_and_pulse_are_exclusive");
  if (pulse && gap_ms != 500)
    return error(status, 400, "pulse_requires_default_gap");
  if (settings_pending() || config.load_stop_pending || lease.active() ||
      (config.load_profile != "monitor" && !numeric_ble_profile()) ||
      config.load_verified || load_desynced || !fresh(load) ||
      !load.link || !load.link->connected())
    return error(status, 409, "idle_fresh_monitor_required");
  if (load.reading.voltage > 25 || load.reading.current > 0.005 ||
      load.reading.temperature >= 40)
    return error(status, 409, "bench_baseline_outside_limits");
  auto result = cJSON_CreateObject();
  auto events = cJSON_AddArrayToObject(result, "events");
  const auto started = now_ms();
  cJSON_AddNumberToObject(result, "command_gap_ms", gap_ms);
  cJSON_AddNumberToObject(result, "cleanup_gap_ms", 500);
  unsigned active_gap_ms = gap_ms;
  auto hex = [](const Bytes &bytes) {
    std::string s;
    for (auto b : bytes) {
      char text[3];
      snprintf(text, sizeof(text), "%02x", b);
      s += text;
    }
    return s;
  };
  auto send = [&](uint8_t command, uint32_t value, const char *phase) {
    if (command == 1)
      pending_load_stop(true);
    // Leave room for the module UART and the preceding ATT transaction.
    if (active_gap_ms)
      vTaskDelay(pdMS_TO_TICKS(active_gap_ms));
    auto event = cJSON_CreateObject();
    cJSON_AddNumberToObject(event, "elapsed_ms", now_ms() - started);
    auto frame = px3_command(command, value);
    cJSON_AddStringToObject(event, "phase", phase);
    cJSON_AddStringToObject(event, "tx_hex", hex(frame).c_str());
    bool sent = load.link->connected() && load.link->send(frame);
    cJSON_AddBoolToObject(event, "queued", sent);
    cJSON_AddItemToArray(events, event);
    return sent;
  };
  auto query = [&](uint8_t command, uint32_t &value, const char *phase) {
    load_frames();
    if (!send(command, 0, phase))
      return false;
    auto event = cJSON_GetArrayItem(events, cJSON_GetArraySize(events) - 1);
    Bytes raw;
    auto deadline = now_ms() + 1500;
    while (now_ms() < deadline && load.link->connected()) {
      auto bytes = load.link->receive();
      if (raw.size() + bytes.size() <= 512)
        raw.insert(raw.end(), bytes.begin(), bytes.end());
      bool answered = false;
      for (auto &f : load_stream.feed(bytes.data(), bytes.size())) {
        if (f.size() == 36) {
          last_load_report = f;
          report_frame(f);
        }
        if (!answered && px3_reply(f, command, value))
          answered = true;
      }
      if (answered) {
        cJSON_AddStringToObject(event, "rx_hex", hex(raw).c_str());
        cJSON_AddNumberToObject(event, "raw_value", value);
        return true;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    cJSON_AddStringToObject(event, "rx_hex", hex(raw).c_str());
    cJSON_AddBoolToObject(event, "timeout", true);
    return false;
  };
  uint32_t state = 1, mode = 255, original = 0, measured = 0;
  bool baseline = query(0x10, state, "before") && state == 0 &&
                  query(0x21, mode, "before") && mode == 0 &&
                  query(0x12, measured, "before") && measured <= 5 &&
                  query(0x17, original, "before") &&
                  (restore_only ? original <= 200
                                : original == 99 || original == 100);
  cJSON_AddNumberToObject(result, "original_raw", original);
  // The last display-confirmed setting is 0.100 A. Its query currently
  // returns 99; restore the nominal setting, and report both raw values.
  cJSON_AddNumberToObject(result, "restore_target_raw", 100);
  cJSON_AddBoolToObject(result, "restore_only", restore_only);
  cJSON_AddBoolToObject(result, "pulse_requested", pulse);
  uint32_t volts = 0;
  if (baseline && pulse)
    baseline = query(0x11, volts, "source_voltage") &&
               volts >= 23000 && volts <= 25000;
  if (!baseline) {
    cJSON_AddStringToObject(result, "reason", "off_cc_0_100a_baseline_required");
    cJSON_AddBoolToObject(result, "passed", false);
    return result;
  }
  bool passed = true;
  for (auto target : {100u, 150u, 200u, 100u}) {
    if (restore_only)
      break;
    if (!query(0x10, state, "guard") || state != 0 ||
        !query(0x21, mode, "guard") || mode != 0 ||
        !query(0x12, measured, "guard") || measured > 5 ||
        !send(0x01, 0, "absolute_off") ||
        !query(0x10, state, "off_readback") || state != 0 ||
        !send(0x02, target, "set_current")) {
      passed = false;
      break;
    }
    // Allow the main loop to process configuration changes before reading.
    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t readback = 0;
    if (!query(0x17, readback, "setpoint_readback") || readback != target) {
      passed = false;
      break;
    }
  }
  bool activated = false, measured_pulse = false;
  if (pulse && passed) {
    uint32_t target = 0;
    passed = query(0x10, state, "pulse_guard") && state == 0 &&
             query(0x21, mode, "pulse_guard") && mode == 0 &&
             query(0x17, target, "pulse_guard") && target == 100 &&
             query(0x11, volts, "pulse_guard") &&
             volts >= 23000 && volts <= 25000;
    if (passed) {
      // Absolute ON is byte 3 = 1, NOT the low byte of the 24-bit value.
      bool sent = send(0x01, 0x010000, "absolute_on");
      if (sent) {
        activated = query(0x10, state, "on_readback") && state == 1;
        if (activated) {
          // The device's averaged current may still show a ramp value at
          // the first sample. Retry measurements only, never the ON command.
          // Overcurrent, invalid voltage or a timeout stop immediately.
          for (unsigned sample = 0; sample < 3; ++sample) {
            if (!query(0x12, measured, "pulse_current") || measured > 130 ||
                !query(0x11, volts, "pulse_voltage") ||
                volts < 23000 || volts > 25000)
              break;
            if (measured >= 80) {
              measured_pulse = true;
              break;
            }
          }
        }
      }
      passed = sent && activated && measured_pulse;
    }
  }
  cJSON_AddBoolToObject(result, "activation_confirmed", activated);
  cJSON_AddBoolToObject(result, "pulse_measurement_confirmed", measured_pulse);
  // Cleanup also runs after a failed write/readback/activation. Absolute OFF is
  // idempotent. Only restore a current target after confirming OFF and CC.
  active_gap_ms = 500;
  bool off_sent = send(0x01, 0, "cleanup_off");
  bool off = query(0x10, state, "cleanup_off_readback") && state == 0;
  bool cc = query(0x21, mode, "cleanup_mode") && mode == 0;
  uint32_t restored = 0;
  bool restore_sent = off && cc && send(0x02, 100, "restore_nominal");
  vTaskDelay(pdMS_TO_TICKS(100));
  bool restore_answer = restore_sent &&
                        query(0x17, restored, "restore_readback");
  bool zero = query(0x12, measured, "final_current") && measured <= 5;
  bool final_off = query(0x10, state, "final_output") && state == 0;
  if (off_sent && final_off && zero)
    pending_load_stop(false);
  bool exact = restore_answer && restored == 100;
  cJSON_AddNumberToObject(result, "restored_raw", restored);
  cJSON_AddBoolToObject(result, "restored_exactly", exact);
  cJSON_AddBoolToObject(result, "output_off_confirmed", off_sent && final_off);
  cJSON_AddBoolToObject(result, "zero_current_confirmed", zero);
  cJSON_AddBoolToObject(result, "passed", passed && exact && final_off && zero);
  cJSON_AddBoolToObject(result, "manual_check_required",
                        !exact || !final_off || !zero);
  cJSON_AddBoolToObject(result, "load_verified", false);
  cJSON_AddItemToObject(result, "ble", ble_diagnostics());
  load.output = -1;
  load_desynced = !exact || !final_off || !zero;
  return result;
#endif
}
static bool setting_query(uint8_t command, uint32_t &value, cJSON *events = nullptr) {
  load_frames();
  probe_rx.clear();
  collecting_probe = true;
  bool ok = probe_px3(command, value);
  collecting_probe = false;
  if (events) {
    auto e = cJSON_CreateObject();
    cJSON_AddNumberToObject(e, "read_command", command);
    cJSON_AddBoolToObject(e, "answered", ok);
    if (ok)
      cJSON_AddNumberToObject(e, "raw_value", value);
    std::string hex;
    for (uint8_t byte : probe_rx) {
      char text[3];
      snprintf(text, sizeof(text), "%02x", byte);
      hex += text;
    }
    cJSON_AddStringToObject(e, "rx_hex", hex.c_str());
    cJSON_AddItemToArray(events, e);
  }
  return ok;
}
static bool setting_write(uint8_t command, uint32_t value) {
  bool ok = numeric_ble_send(command, value);
  // Setting changes may trigger main-loop work or nonvolatile writes.
  // ATT acknowledgement alone does not establish command completion.
  vTaskDelay(pdMS_TO_TICKS(1000));
  return ok;
}
// Only for absolute, idempotent setters with an exact readback. A wrong value
// is read again after a short settle interval before one bounded resend.
// A missing reply desynchronizes the untagged protocol: stop, reconnect and
// let the persistent restoration journal handle the uncertain write.
static bool setting_write_verified(uint8_t write, uint32_t wire, uint8_t read,
                                   uint32_t expected, uint32_t &actual,
                                   cJSON *events = nullptr,
                                   unsigned *attempts = nullptr,
                                   bool *sent_any = nullptr) {
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    if (attempts)
      ++*attempts;
    if (!setting_write(write, wire))
      return false;
    if (sent_any)
      *sent_any = true;
    if (!setting_query(read, actual, events))
      return false;
    if (actual == expected)
      return true;
    vTaskDelay(pdMS_TO_TICKS(200));
    if (!setting_query(read, actual, events))
      return false;
    if (actual == expected)
      return true;
  }
  return false;
}
static bool setting_value_allowed(const SettingCheck &s, uint32_t wire,
                                  uint32_t &expected) {
  // These are conservative bridge limits for the present 25 V bench, not
  // claims about the DL24M's full hardware range. Calibration and special
  // modes intentionally have no entry in this catalog.
  bool allowed = false;
  switch (s.write) {
  case 2:
    allowed = s.mode == 1 ? wire <= 1000
              : s.mode == 2 ? wire <= 2400
              : s.mode == 3 && wire <= 100;
    break;
  case 3: allowed = wire <= 250; break;
  case 4: allowed = wire <= 86400 && wire % 60 == 0; break;
  case 0x41: allowed = wire <= 1; break;
  case 0x42:
  case 0x43: allowed = wire >= 48 && wire <= 57; break;
  case 0x44: allowed = wire <= 120; break;
  case 0x45: allowed = wire <= 2; break;
  case 0x47: allowed = wire >= 3000 && wire <= 25000; break;
  case 0x48: allowed = wire >= 50 && wire <= 2500; break;
  case 0x49: allowed = wire >= 100 && wire <= 1500; break;
  case 0x4d:
  case 0x50: allowed = wire >= 40 && wire <= 100; break;
  case 0x4e: allowed = wire >= 20 && wire <= 90; break;
  }
  if (!allowed || s.write == 6)
    return false;
  expected = s.read_factor ? wire * s.read_factor
             : ((wire / 3600) << 16) | (((wire % 3600) / 60) << 8);
  return true;
}
static bool restore_load_setting() {
  const auto *s = setting_check(config.load_restore_setting);
  if (!s || load_desynced || !load.link || !load.link->connected())
    return false;
  if (!numeric_ble_off())
    return false;
  uint32_t actual = 0, mode = 255, cc = 0;
  bool known = config.load_restore_expected != std::numeric_limits<uint32_t>::max();
  if (known && s->mode &&
      !setting_write_verified(6, s->mode, 0x21, s->mode, mode))
    return false;
  if (known && s->read == 0x31 && !setting_query(0x31, actual))
    return false; // Enter settings before restoring the saved language.
  // Always write the original value, even if an unsupported/lagging read
  // returned the original value after the attempted test change.
  if (known && !setting_write_verified(s->write, config.load_restore_wire,
                                       s->read, config.load_restore_expected,
                                       actual))
    return false;
  if (s->read == 0x31 && !setting_write(0x4c, 0))
    return false;
  if (s->mode && !setting_write_verified(6, 0, 0x21, 0, mode))
    return false;
  if (!setting_query(0x21, mode) || mode != 0 ||
      !setting_query(0x17, cc))
    return false;
  if (cc != config.load_restore_cc &&
      !setting_write_verified(2, config.load_restore_cc, 0x17,
                              config.load_restore_cc, cc))
    return false;
  if (!numeric_ble_off())
    return false;
  load.set_current = cc / 1000.;
  config.load_restore_setting.clear();
  config.load_restore_wire = config.load_restore_expected = config.load_restore_cc = 0;
  save_config();
  return true;
}
static bool commit_load_setting(const SettingCheck &s) {
  if (!numeric_ble_off())
    return false;
  uint32_t mode = 255, cc = 0;
  if (s.read == 0x31 && !setting_write(0x4c, 0))
    return false;
  if (s.mode && !setting_write_verified(6, 0, 0x21, 0, mode))
    return false;
  if (!setting_query(0x21, mode) || mode != 0 ||
      !setting_query(0x17, cc))
    return false;
  if (cc != config.load_restore_cc &&
      !setting_write_verified(2, config.load_restore_cc, 0x17,
                              config.load_restore_cc, cc))
    return false;
  if (!numeric_ble_off())
    return false;
  load.set_current = cc / 1000.;
  config.load_restore_setting.clear();
  config.load_restore_wire = config.load_restore_expected = config.load_restore_cc = 0;
  save_config();
  return true;
}
static cJSON *load_setting_test(const cJSON *body, int &status, bool counters,
                                bool apply = false) {
  const auto *s = counters ? nullptr : setting_check(str(body, "setting"));
  if (!counters && !s)
    return error(status, 400, "unknown_reversible_setting");
  if (apply && s->write == 6)
    return error(status, 400, "mode_selection_requires_separate_control");
  uint32_t requested_wire = 0, requested_expected = 0;
  if (apply) {
    auto value = cJSON_GetObjectItemCaseSensitive(body, "wire_value");
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) ||
        value->valuedouble < 0 || value->valuedouble > 0xffffff ||
        value->valuedouble != std::floor(value->valuedouble))
      return error(status, 400, "integer_wire_value_required");
    requested_wire = uint32_t(value->valuedouble);
    if (!setting_value_allowed(*s, requested_wire, requested_expected))
      return error(status, 400, "wire_value_outside_bridge_limits");
  }
  if (counters && !flag(body, "confirm_reset"))
    return error(status, 400, "counter_reset_confirmation_required");
  if (!numeric_ble_profile() || config.load_verified || lease.active() ||
      load_desynced || config.load_stop_pending || settings_pending() ||
      !load.link || !load.link->connected())
    return error(status, 409, "idle_unverified_numeric_ble_required");
  auto j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "setting", counters ? "counters" : s->name);
  auto events = cJSON_AddArrayToObject(j, "events");
  uint32_t state = 2, mode = 255, current = 0, volts = 0, temp = 0, cc = 0;
  bool baseline = setting_query(0x10, state, events) && state == 0 &&
                  setting_query(0x21, mode, events) && mode == 0 &&
                  setting_query(0x12, current, events) && current <= 5 &&
                  setting_query(0x11, volts, events) && volts <= 25000 &&
                  setting_query(0x16, temp, events) && temp < 400 &&
                  setting_query(0x17, cc, events) && cc <= 200;
  if (!baseline) {
    cJSON_AddStringToObject(j, "reason", "off_cc_bench_baseline_required");
    cJSON_AddBoolToObject(j, "passed", false);
    return j;
  }
  if (counters) {
    uint32_t before[3]{}, after[3]{};
    bool ok = true;
    auto b = cJSON_AddArrayToObject(j, "before");
    auto a = cJSON_AddArrayToObject(j, "after");
    for (unsigned i = 0; i < 3 && ok; ++i) {
      ok = setting_query(0x13 + i, before[i], events);
      if (ok)
        cJSON_AddItemToArray(b, cJSON_CreateNumber(before[i]));
    }
    bool sent = ok && setting_write(5, 0);
    ok = sent;
    for (unsigned i = 0; i < 3 && ok; ++i) {
      ok = setting_query(0x13 + i, after[i], events);
      if (ok) {
        cJSON_AddItemToArray(a, cJSON_CreateNumber(after[i]));
        ok = after[i] == 0;
      }
    }
    bool off = numeric_ble_off();
    cJSON_AddBoolToObject(j, "reset_sent", sent);
    cJSON_AddBoolToObject(j, "output_off_confirmed", off);
    cJSON_AddBoolToObject(j, "passed", ok && off);
    return j;
  }
  uint32_t original = 0;
  // Entering settings or selecting another setpoint bank is itself mutable.
  // Save the obligation to exit/return to CC before either operation.
  if (s->mode || s->read == 0x31) {
    config.load_restore_setting = s->name;
    config.load_restore_wire = 0;
    config.load_restore_expected = std::numeric_limits<uint32_t>::max();
    config.load_restore_cc = cc;
    save_config();
  }
  if (s->mode &&
      (!setting_write_verified(6, s->mode, 0x21, s->mode, mode, events) ||
       !numeric_ble_off())) {
    bool restored = restore_load_setting();
    cJSON_AddStringToObject(j, "reason", "mode_selection_unconfirmed");
    cJSON_AddBoolToObject(j, "restored_exactly", restored);
    cJSON_AddBoolToObject(j, "passed", false);
    return j;
  }
  if (!setting_query(s->read, original, events)) {
    cJSON_AddStringToObject(j, "reason", "original_value_unavailable");
    if (settings_pending())
      cJSON_AddBoolToObject(j, "restored_exactly", restore_load_setting());
    cJSON_AddBoolToObject(j, "passed", false);
    return j;
  }
  uint32_t wire = 0;
  bool reversible = false;
  if (s->read_factor) {
    reversible = original % s->read_factor == 0;
    wire = original / s->read_factor;
  } else {
    reversible = !(original & 255) && ((original >> 8) & 255) < 60;
    wire = (original >> 16) * 3600 + ((original >> 8) & 255) * 60;
  }
  uint32_t trial_wire = apply ? requested_wire : s->trial_wire;
  uint32_t trial_expected = apply ? requested_expected : s->trial_expected;
  if (!apply && s->read == 0x31) {
    reversible = original <= 1;
    trial_wire = trial_expected = original == 0 ? 1 : 0;
  } else if (!apply && s->mode && original == trial_expected) {
    ++trial_wire;
    ++trial_expected;
  }
  if (!reversible || (!apply && original == trial_expected)) {
    if (settings_pending())
      cJSON_AddBoolToObject(j, "restored_exactly", restore_load_setting());
    cJSON_AddStringToObject(j, "reason", "original_not_representable_or_trial_unchanged");
    cJSON_AddBoolToObject(j, "passed", false);
    return j;
  }
  cJSON_AddNumberToObject(j, "original_raw", original);
  cJSON_AddNumberToObject(j, "restore_wire", wire);
  cJSON_AddNumberToObject(j, "trial_wire", trial_wire);
  cJSON_AddNumberToObject(j, "expected_raw", trial_expected);
  cJSON_AddNumberToObject(j, "original_cc_raw", cc);
  if (apply && original == trial_expected) {
    bool restored = !settings_pending() || restore_load_setting();
    cJSON_AddBoolToObject(j, "unchanged", true);
    cJSON_AddBoolToObject(j, "committed", restored);
    cJSON_AddBoolToObject(j, "passed", restored);
    return j;
  }
  config.load_restore_setting = s->name;
  config.load_restore_wire = wire;
  config.load_restore_expected = original;
  config.load_restore_cc = cc;
  save_config(); // Must precede the first setting mutation.
  uint32_t actual = 0;
  bool sent = false;
  unsigned attempts = 0;
  bool changed = setting_write_verified(s->write, trial_wire, s->read,
                                        trial_expected, actual, events,
                                        &attempts, &sent);
  cJSON_AddBoolToObject(j, "trial_sent", sent);
  cJSON_AddBoolToObject(j, "trial_confirmed", changed);
  cJSON_AddNumberToObject(j, "write_attempts", attempts);
  if (sent && !load_desynced)
    cJSON_AddNumberToObject(j, "trial_readback", actual);
  bool committed = apply && changed && commit_load_setting(*s);
  bool restored = committed ? false : restore_load_setting();
  if (apply) {
    cJSON_AddBoolToObject(j, "committed", committed);
    cJSON_AddBoolToObject(j, "rollback_confirmed", restored);
  }
  cJSON_AddBoolToObject(j, "restored_exactly", restored);
  cJSON_AddBoolToObject(j, "restore_pending", settings_pending());
  cJSON_AddBoolToObject(j, "output_off_confirmed",
                        (committed || restored) && load.output == 0);
  cJSON_AddBoolToObject(j, "passed", changed && (apply ? committed : restored));
  return j;
}
cJSON *api_call(const std::string &method, const std::string &path,
                const cJSON *body, const std::string &token, int &status,
                bool local_usb) {
  status = 200;
  if (method == "POST" && path == "/devices/load/emergency-stop") {
    if (!numeric_ble_profile() && !config.load_stop_pending)
      return error(status, 422, "numeric_ble_profile_required");
    pending_load_stop(true);
    config.load_verified = false;
    save_config();
    bool confirmed = false;
    if (lease.active() && lease.devices[0]) {
      stop("emergency_stop"); // Owned sources first.
      confirmed = stopped_outputs[0] == 0;
    } else {
      confirmed = numeric_ble_off();
      stopped_outputs[0] = confirmed ? 0 : -1;
    }
    if (!confirmed && load_desynced)
      select_ble(config.ble_address);
    auto j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "status", confirmed ? "confirmed" : "pending");
    cJSON_AddBoolToObject(j, "output_off_confirmed", confirmed);
    cJSON_AddBoolToObject(j, "zero_current_confirmed", confirmed);
    cJSON_AddBoolToObject(j, "load_stop_pending", config.load_stop_pending);
    return j;
  }
  if (method == "POST" && config.load_stop_pending &&
      path.rfind("/diagnostics/load/", 0) == 0)
    return error(status, 409, "pending_shutdown");
  if (method == "POST" && settings_pending() &&
      path.rfind("/diagnostics/load/", 0) == 0)
    return error(status, 409, "setting_restore_pending");
  if (method == "POST" && (path == "/diagnostics/load/setting-check" ||
                           path == "/diagnostics/load/reset-counters" ||
                           path == "/devices/load/setting")) {
    if (path != "/devices/load/setting" && !local_usb)
      return error(status, 403, "local_usb_required");
    return load_setting_test(body, status,
                             path == "/diagnostics/load/reset-counters",
                             path == "/devices/load/setting");
  }
  unsigned probe_gap_ms = path == "/diagnostics/load/probe" ? 0 : 500;
  if (method == "POST" &&
      (path == "/diagnostics/load/probe" || path == "/diagnostics/load/write-check")) {
    auto gap = cJSON_GetObjectItem(body, "command_gap_ms");
    if (gap) {
      if (!local_usb)
        return error(status, 403, "local_usb_required");
      if (!cJSON_IsNumber(gap) || !std::isfinite(gap->valuedouble) ||
          gap->valuedouble < 0 || gap->valuedouble > 500 ||
          gap->valuedouble != std::floor(gap->valuedouble))
        return error(status, 400, "command_gap_ms_integer_0_to_500_required");
      probe_gap_ms = unsigned(gap->valuedouble);
    }
  }
  if (method == "POST" && path == "/diagnostics/load/write-check") {
    if (!local_usb)
      return error(status, 403, "local_usb_required");
    return load_numeric_write_check(status, flag(body, "restore_only"),
                                     flag(body, "pulse"), probe_gap_ms);
  }
  if (method == "POST" && path == "/diagnostics/load/probe") {
    std::string protocol = str(body, "protocol",
#if CONFIG_IDF_TARGET_ESP32
                               "spp"
#else
                               "px100-v3"
#endif
    );
    if (protocol != "px100" && protocol != "px100-v3" &&
        protocol != "spp" && protocol != "spp-legacy")
      return error(status, 400, "unknown_probe_protocol");
#if CONFIG_IDF_TARGET_ESP32
    if (protocol != "spp" && protocol != "spp-legacy")
      return error(status, 422, "unsupported_probe_protocol_on_board");
#else
    if (protocol == "spp" || protocol == "spp-legacy")
      return error(status, 422, "unsupported_probe_protocol_on_board");
#endif
    bool spp_protocol = protocol == "spp" || protocol == "spp-legacy";
    if (cJSON_GetObjectItem(body, "command_gap_ms") && protocol != "px100-v3")
      return error(status, 422, "gap_override_requires_numeric_ble");
    std::string query = str(body, "query", "all");
    const std::array<std::string, 5> names{"output", "set_current", "voltage",
                                           "current", "temperature"};
    const std::array<uint8_t, 5> commands{0x10, 0x17, 0x11, 0x12, 0x16};
    bool numeric = protocol == "px100-v3" || protocol == "spp";
    auto known = std::find_if(dl24m_reads.begin(), dl24m_reads.end(),
                              [&](const auto &r) { return query == r.name; });
    if (query != "all" && !(numeric && query == "full") &&
        (numeric ? known == dl24m_reads.end()
                 : std::find(names.begin(), names.end(), query) == names.end()))
      return error(status, 400, "unknown_probe_query");
    if (lease.active())
      return error(status, 409, "session_active");
    // Audited numeric reads do not need a preceding unsolicited FF55 report.
    // Continuous request/reply traffic can suppress those reports even when
    // the link and numeric replies remain healthy. Mutation guards are separate.
    if (!load.link || !load.link->connected() || load_desynced ||
        (!numeric && !spp_protocol && !fresh(load)))
      return error(status, 409, "fresh_synchronized_load_required");
    auto j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "protocol", protocol.c_str());
    cJSON_AddStringToObject(j, "query", query.c_str());
    cJSON_AddItemToObject(j, spp_protocol ? "spp" : "ble",
                          spp_protocol ? spp_diagnostics() : ble_diagnostics());
    auto values = cJSON_AddArrayToObject(j, "queries");
    bool ok = true;
    std::vector<LoadRegister> selected;
    if (query == "all")
      for (size_t index = 0; index < commands.size(); ++index)
        selected.push_back({names[index].c_str(), commands[index]});
    else if (query == "full")
      selected.assign(dl24m_reads.begin(), dl24m_reads.end());
    else if (numeric)
      selected.push_back(*known);
    else
      for (size_t index = 0; index < commands.size(); ++index)
        if (query == names[index])
          selected.push_back({names[index].c_str(), commands[index]});
    cJSON_AddNumberToObject(j, "command_gap_ms", numeric ? probe_gap_ms : 0);
    const auto started = now_ms();
    // Only audited, non-mutating registers. In particular, 22 and 31 are
    // NOT ordinary reads despite returning CA CB responses.
    for (const auto &reg : selected) {
      uint8_t cmd = reg.command;
      if (numeric && probe_gap_ms)
        vTaskDelay(pdMS_TO_TICKS(probe_gap_ms));
      load_frames();
      probe_rx.clear();
      collecting_probe = true;
      auto sent_at = now_ms();
      uint32_t value = 0;
      bool answered = protocol == "spp"
                          ? load_query(cmd, value)
                          : protocol == "spp-legacy"
                                ? px(px_command(cmd), true, value, 2000)
                          : protocol == "px100-v3"
                                ? probe_px3(cmd, value)
                                : px(px_command(cmd), true, value, 2000);
      collecting_probe = false;
      auto item = cJSON_CreateObject();
      cJSON_AddStringToObject(item, "name", reg.name);
      cJSON_AddNumberToObject(item, "elapsed_ms", sent_at - started);
      cJSON_AddNumberToObject(item, "response_ms", now_ms() - sent_at);
      cJSON_AddNumberToObject(item, "command", cmd);
      cJSON_AddBoolToObject(item, "answered", answered);
      if (answered)
        cJSON_AddNumberToObject(item, "raw_value", value);
      std::string hex;
      for (uint8_t byte : probe_rx) {
        char b[3];
        snprintf(b, sizeof(b), "%02x", byte);
        hex += b;
      }
      cJSON_AddStringToObject(item, "rx_hex", hex.c_str());
      cJSON_AddItemToObject(item,
                            spp_protocol ? "spp_after_query"
                                         : "ble_after_query",
                            spp_protocol ? spp_diagnostics()
                                         : ble_diagnostics());
      cJSON_AddItemToArray(values, item);
      if (!answered) {
        ok = false;
        break; // Do not let a late untagged reply answer the next query.
      }
    }
    cJSON_AddBoolToObject(j, "all_queries_answered", ok);
    if (!ok) {
#if CONFIG_IDF_TARGET_ESP32
      select_spp(config.spp_address);
#else
      select_ble(config.ble_address);
#endif
    }
    return j;
  }
  if (method == "GET") {
    if (path == "/diagnostics/network")
      return network_diagnostics();
    if (path == "/diagnostics/load") {
      auto j = cJSON_CreateObject();
      std::string hex;
      for (auto byte : last_load_report) {
        char b[3];
        snprintf(b, sizeof(b), "%02x", byte);
        hex += b;
      }
      cJSON_AddStringToObject(j, "last_report_hex", hex.c_str());
      cJSON_AddBoolToObject(j, "command_stream_synchronized", !load_desynced);
      cJSON_AddBoolToObject(j, "checksum_ignored", config.ignore_checksum);
      cJSON_AddItemToObject(j, "ble", ble_diagnostics());
      cJSON_AddItemToObject(j, "spp", spp_diagnostics());
      return j;
    }
    if (path == "/devices")
      return devices_json();
    if (path == "/discovery")
      return discovery_json();
    if (path == "/config")
      return configuration_json();
    if (path == "/session")
      return session_json();
    for (int i = 0; i < 3; i++)
      if (path == std::string("/devices/") + ids[i])
        return device_json(i);
  }
  if (method == "POST" && path == "/config") {
    if (settings_pending())
      return error(status, 409, "setting_restore_pending");
    if (config.load_stop_pending)
      return error(status, 409, "pending_shutdown");
    if (lease.active())
      return error(status, 409, "session_active");
    Config candidate = config;
    candidate.ble_address =
        str(body, "ble_address", config.ble_address.c_str());
    candidate.spp_address =
        str(body, "spp_address", config.spp_address.c_str());
    candidate.spp_pin = str(body, "spp_pin", config.spp_pin.c_str());
    candidate.load_profile =
        str(body, "load_profile", config.load_profile.c_str());
    if (candidate.load_profile != "monitor" &&
        candidate.load_profile != "px100-centi" &&
        candidate.load_profile != "px100-milli-read" &&
        candidate.load_profile != "dl24m-ble" &&
        candidate.load_profile != "dl24m-spp")
      return error(status, 400, "unknown_load_profile");
#if CONFIG_IDF_TARGET_ESP32
    if (candidate.load_profile != "monitor" &&
        candidate.load_profile != "dl24m-spp")
      return error(status, 422, "unsupported_load_profile_on_board");
#else
    if (candidate.load_profile == "dl24m-spp")
      return error(status, 422, "unsupported_load_profile_on_board");
#endif
    if (!candidate.ble_address.empty()) {
      unsigned b[6];
      char extra;
      if (sscanf(candidate.ble_address.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x%c",
                 &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &extra) != 6 ||
          candidate.ble_address.size() != 17)
        return error(status, 400, "invalid_ble_address");
    }
    if (!candidate.spp_address.empty()) {
      unsigned b[6];
      char extra;
      if (candidate.spp_address.size() != 17 ||
          sscanf(candidate.spp_address.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x%c",
                 &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &extra) != 6)
        return error(status, 400, "invalid_spp_address");
    }
    if (candidate.spp_pin != "1234" && candidate.spp_pin != "0000")
      return error(status, 400, "invalid_spp_pin");
    candidate.ignore_checksum =
        flag(body, "ignore_checksum", config.ignore_checksum);
    candidate.serials[0] = str(body, "psu1_serial", config.serials[0].c_str());
    candidate.serials[1] = str(body, "psu2_serial", config.serials[1].c_str());
    if (!candidate.serials[0].empty() &&
        candidate.serials[0] == candidate.serials[1])
      return error(status, 400, "duplicate_identity");
    auto limits = cJSON_GetObjectItem(body, "limits");
    if (limits) {
      if (!cJSON_IsArray(limits) || cJSON_GetArraySize(limits) != 3)
        return error(status, 400, "three_limits_required");
      for (int i = 0; i < 3; i++) {
        auto l = cJSON_GetArrayItem(limits, i);
        Limits v{num(l, "voltage_v"), num(l, "current_a"), num(l, "power_w")};
        if ((!v.valid() &&
             !(v.voltage == 0 && v.current == 0 && v.power == 0)) ||
            v.voltage > (i ? 60 : 200) || v.current > (i ? 24 : 40))
          return error(status, 400, "invalid_limits");
        candidate.limits[i] = v;
      }
    }
    bool changed = candidate.ble_address != config.ble_address ||
                   candidate.spp_address != config.spp_address ||
                   candidate.spp_pin != config.spp_pin ||
                   candidate.load_profile != config.load_profile ||
                   candidate.ignore_checksum != config.ignore_checksum;
    if (changed)
      candidate.load_verified = false;
    config = candidate;
    save_config();
    if (changed) {
      load.received = 0;
      load.output = -1;
      load_stream.clear();
      load_desynced = false;
#if CONFIG_IDF_TARGET_ESP32
      select_spp(config.spp_address);
#else
      select_ble(config.ble_address);
#endif
    }
    return configuration_json();
  }
  if (method == "POST" && path == "/commission/load") {
    if (settings_pending())
      return error(status, 409, "setting_restore_pending");
    if (config.load_stop_pending)
      return error(status, 409, "pending_shutdown");
    if (lease.active())
      return error(status, 409, "session_active");
    if (numeric_ble_profile() && flag(body, "bench_24v_1a")) {
      if (!local_usb)
        return error(status, 403, "local_usb_required");
      if (!config.limits[0].contains(25, 0.2))
        return error(status, 409, "bench_limits_must_cover_25v_0_2a_5w");
      config.load_verified = false;
      save_config();
      auto result = load_numeric_write_check(status, false, true);
      if (flag(result, "passed") && numeric_ble_sample(true)) {
        config.load_verified = true;
        load.output = 0;
        load.set_current = 0.1;
        save_config();
        cJSON_ReplaceItemInObject(result, "load_verified", cJSON_CreateBool(true));
      } else if (flag(result, "passed")) {
        cJSON_ReplaceItemInObject(result, "passed", cJSON_CreateBool(false));
        cJSON_AddStringToObject(result, "reason", "numeric_measurement_refresh_failed");
      }
      return result;
    }
    if (!flag(body, "source_physically_disconnected") ||
        !flag(body, "cc_mode_selected") ||
        !flag(body, "display_scaling_checked"))
      return error(status, 400, "commissioning_checks_required");
    if (config.load_profile == "monitor" || !fresh(load) ||
        !config.limits[0].valid() || config.limits[0].current < 0.2)
      return error(status, 409, "profile_connection_or_limits_missing");
    config.load_verified = false;
    save_config();
#if CONFIG_IDF_TARGET_ESP32
    uint32_t before_output = 0, before_current = 0;
    uint32_t repeated_output = 0, repeated_current = 0;
    if (!load_query(0x10, before_output) || before_output != 0 ||
        !load_query(0x17, before_current) || before_current > 40000 ||
        !load_query(0x10, repeated_output) || repeated_output != before_output ||
        !load_query(0x17, repeated_current) ||
        repeated_current != before_current)
      return error(status, 409, "spp_readback_not_confirmed");
    load.output = 0;
    load.set_current = before_current / 1000.;
#endif
    bool ok = load_output(false) && load_current(0.1) && load_current(0.2);
    bool restored_current = load_current(0);
    bool restored_output = load_output(false);
    bool restored = restored_current && restored_output;
    if (!ok || !restored)
      return error(status, 409,
                   "profile_unverified_output_state_must_be_checked");
    config.load_verified = true;
    save_config();
    return success();
  }
  if (method == "POST" && path == "/session") {
    if (settings_pending())
      return error(status, 409, "setting_restore_pending");
    if (config.load_stop_pending)
      return error(status, 409, "pending_shutdown");
    if (lease.active())
      return error(status, 409, "session_busy");
    auto a = cJSON_GetObjectItem(body, "devices");
    if (!cJSON_IsArray(a) || !cJSON_GetArraySize(a))
      return error(status, 400, "devices_required");
    std::array<bool, 3> selected{};
    cJSON *item;
    cJSON_ArrayForEach(item, a) {
      int id = -1;
      for (int i = 0; i < 3; i++)
        if (cJSON_IsString(item) && std::string(item->valuestring) == ids[i])
          id = i;
      if (id < 0 || selected[id])
        return error(status, 400, "invalid_device_selection");
      auto d = device(id);
      if (!d || !fresh(*d) || !writable(id) || d->output != 0 ||
          !config.limits[id].valid())
        return error(status, 409, "device_not_ready_or_output_not_off");
      selected[id] = true;
    }
    if (!local_usb && !wifi_connected())
      return error(status, 409, "wifi_disconnected");
    lease.token = random_token();
    lease.devices = selected;
    lease.deadline = now_ms() + 10000;
    lease.require_wifi = !local_usb;
    auto j = success();
    cJSON_AddStringToObject(j, "lease", lease.token.c_str());
    cJSON_AddNumberToObject(j, "expires_in_ms", 10000);
    return j;
  }
  if (method == "POST" &&
      (path == "/session/heartbeat" || path == "/session/stop")) {
    if (!lease.owns(token, now_ms()))
      return error(status, 409, "invalid_or_expired_lease");
    if (path == "/session/heartbeat") {
      lease.deadline = now_ms() + 10000;
      return success();
    }
    stop("client_stop");
    return session_json();
  }
  if (method == "POST")
    for (int id = 0; id < 3; id++)
      if (path == std::string("/devices/") + ids[id] + "/command") {
        if (!lease.owns(token, now_ms()) || !lease.devices[id])
          return error(status, 409, "device_not_owned");
        auto d = device(id);
        if (!d || !fresh(*d) || !writable(id))
          return error(status, 409, "device_not_ready");
        int count = 0;
        cJSON *f;
        cJSON_ArrayForEach(f, body) {
          if (std::string(f->string) != "voltage_v" &&
              std::string(f->string) != "current_a" &&
              std::string(f->string) != "output")
            return error(status, 400, "unknown_command");
          count++;
        }
        if (count != 1)
          return error(status, 400, "one_command_per_request");
        if (!numeric_field(body, "voltage_v") ||
            !numeric_field(body, "current_a"))
          return error(status, 400, "finite_number_required");
        auto v = cJSON_GetObjectItem(body, "voltage_v"),
             i = cJSON_GetObjectItem(body, "current_a"),
             o = cJSON_GetObjectItem(body, "output");
        if (v && !id)
          return error(status, 422, "unsupported");
        if (o && !cJSON_IsBool(o))
          return error(status, 400, "boolean_required");
        auto voltage =
            v ? v->valuedouble : (id ? d->set_voltage : d->reading.voltage);
        auto current = i ? i->valuedouble : d->set_current;
        bool off = o && cJSON_IsFalse(o);
        bool valid = config.limits[id].contains(voltage, current);
        if (d->output == 0 && (v || i)) {
          const auto &l = config.limits[id];
          valid = l.valid() && (v ? voltage >= 0 && voltage <= l.voltage
                                  : current >= 0 && current <= l.current);
        }
        if (!off && !valid)
          return error(status, 422, "limit_exceeded");
        const double resolution =
#if CONFIG_IDF_TARGET_ESP32
            id == 0 ? 1000. : 100.;
#else
            !id && numeric_ble_profile() ? 1000. : 100.;
#endif
        if ((v || i) &&
            std::abs((v ? v->valuedouble : i->valuedouble) * resolution -
                     std::round((v ? v->valuedouble : i->valuedouble) * resolution)) >
                0.0001)
          return error(status, 422,
#if CONFIG_IDF_TARGET_ESP32
                       id == 0 ? "resolution_is_0_001" : "resolution_is_0_01"
#else
                       !id && numeric_ble_profile() ? "resolution_is_0_001"
                                                    : "resolution_is_0_01"
#endif
          );
        // An authenticated, validated control command is also a liveness
        // message. Renew at receipt/start, never after a slow transaction:
        // BLE pacing must not expire a live owner mid-command.
        lease.deadline = now_ms() + 10000;
        bool ok = o ? output(id, cJSON_IsTrue(o))
                    : (id ? rd_write(*d, v ? 8 : 9,
                                     uint16_t(std::lround(
                                         (v ? v->valuedouble : i->valuedouble) *
                                         100)))
                          : load_current(current));
        if (!ok) {
          stop("command_unconfirmed");
          return error(status, 504,
                       "unconfirmed_session_stopped_check_outputs");
        }
        return success();
      }
  return error(status, 404, "not_found");
}
} // namespace lab

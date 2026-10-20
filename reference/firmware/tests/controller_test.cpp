// Compile the real coordinator against fake byte transports and FreeRTOS hooks.
#include "../main/devices.cpp"
#define CONFIG_LAB_SERIAL_DIAGNOSTICS 1
#include "../main/serial.cpp"
#include <cassert>
#include <iostream>
static uint64_t clock_ms = 100;
static bool wifi = true;
static bool persisted_stop_pending = false;
static std::string persisted_setting;
static std::vector<std::string> writes;
void vTaskDelay(uint32_t ms) { clock_ms += ms; }
namespace lab {
Config config;
SemaphoreHandle_t state_mutex = nullptr;
uint64_t now_ms() { return clock_ms; }
bool wifi_connected() { return wifi; }
cJSON *network_diagnostics() { return cJSON_CreateObject(); }
std::string random_token() { return "test-token"; }
void save_config() {
  persisted_stop_pending = config.load_stop_pending;
  persisted_setting = config.load_restore_setting;
}
void enter_setup() {}
std::string setup_ssid() { return "LabBridge-Setup"; }
void select_ble(const std::string &) {}
std::vector<std::string> discovered_ble() { return {}; }
cJSON *ble_diagnostics() { return cJSON_CreateObject(); }
std::shared_ptr<Link> ble_link() { return load.link; }
std::shared_ptr<Link> spp_link() { return {}; }
void select_spp(const std::string &) {}
std::vector<std::string> discovered_spp() { return {}; }
cJSON *spp_diagnostics() { return cJSON_CreateObject(); }
std::array<std::shared_ptr<Link>, 2> usb_links() {
  return {psus[0].link, psus[1].link};
}
const char *str(const cJSON *j, const char *k, const char *f) {
  auto v = cJSON_GetObjectItemCaseSensitive(j, k);
  return cJSON_IsString(v) ? v->valuestring : f;
}
double num(const cJSON *j, const char *k, double f) {
  auto v = cJSON_GetObjectItemCaseSensitive(j, k);
  return cJSON_IsNumber(v) ? v->valuedouble : f;
}
bool flag(const cJSON *j, const char *k, bool f) {
  auto v = cJSON_GetObjectItemCaseSensitive(j, k);
  return cJSON_IsBool(v) ? cJSON_IsTrue(v) : f;
}
class FakeRD : public Link {
public:
  uint32_t serial;
  uint16_t volts = 0, amps = 0, on = 0;
  bool online = true, corrupt = false, fail_write = false;
  Bytes reply;
  explicit FakeRD(uint32_t s) : serial(s) {}
  bool connected() const override { return online; }
  bool send(const Bytes &b) override {
    if (!online)
      return false;
    if (b[1] == 3) {
      std::array<uint16_t, 20> r{};
      r[0] = 60241;
      r[1] = serial >> 16;
      r[2] = serial & 65535;
      r[5] = 25;
      r[8] = volts;
      r[9] = amps;
      r[18] = on;
      reply = {1, 3, 40};
      for (auto v : r) {
        reply.push_back(v >> 8);
        reply.push_back(v & 255);
      }
      auto crc = crc16(reply.data(), reply.size());
      reply.push_back(crc & 255);
      reply.push_back(crc >> 8);
      if (corrupt)
        reply.back() ^= 1;
    } else {
      auto reg = be16(&b[2]), v = be16(&b[4]);
      writes.push_back(std::to_string(serial) + ":" + std::to_string(reg) +
                       ":" + std::to_string(v));
      if (fail_write) {
        reply.clear();
        return true;
      }
      if (reg == 8)
        volts = v;
      if (reg == 9)
        amps = v;
      if (reg == 18)
        on = v;
      reply = b;
    }
    return true;
  }
  Bytes receive() override {
    Bytes b;
    reply.swap(b);
    return b;
  }
};
class FakeLoad : public Link {
public:
  std::vector<uint8_t> commands;
  bool online = true, wrong_scale = false, drop_replies = false;
  bool bench_reports = false, ignore_start = false, ignore_stop = false;
  bool ignore_numeric_setpoint = false;
  uint8_t ignore_next_setting_command = 0;
  bool ignore_next_numeric_setpoint = false;
  bool numeric_measurement = false, ignore_numeric_on = false;
  bool ignore_numeric_off = false;
  unsigned numeric_ramp_reads = 0;
  uint32_t mode = 0;
  bool settings_enabled = false, fail_setting_reply = false;
  bool settings_menu = false;
  bool fail_menu_read = false;
  std::array<uint32_t, 4> mode_currents{100, 1200, 2400, 10};
  std::array<uint32_t, 256> registers{};
  unsigned toggle_count = 0, bench_current_ma = 100;
  unsigned off_current_ma = 0;
  uint64_t last_report_at = 0;
  uint32_t current = 0, on = 0;
  Bytes reply;
  bool connected() const override { return online; }
  bool send(const Bytes &b) override {
    if (!online)
      return false;
    if (b[0] == 0xff) {
      assert(b == Bytes({0xff, 0x55, 0x11, 0x02, 0x32, 0, 0, 0, 0, 1}));
      ++toggle_count;
      writes.push_back("load:toggle");
      if (!((toggle_count == 1 && ignore_start) ||
            (toggle_count == 2 && ignore_stop)))
        on = !on;
      return true;
    }
    commands.push_back(b[2]);
    if (b[2] == ignore_next_setting_command ||
        (b[2] == 2 && ignore_next_numeric_setpoint)) {
      ignore_next_setting_command = 0;
      ignore_next_numeric_setpoint = false;
      reply.clear();
      return true; // ATT succeeds but the device does not apply this write.
    }
    if (dl24m_read_only(b[2]) || b[2] == 0x31) {
      if (b[2] == 0x31)
        settings_menu = true;
      if (b[2] == 0x31 && fail_menu_read)
        drop_replies = true;
      uint32_t v = b[2] == 0x10   ? on
                   : b[2] == 0x21 ? mode
                   : b[2] == 0x11 ? 23900
                   : b[2] == 0x12 ? (numeric_measurement && on ? bench_current_ma : off_current_ma)
                   : b[2] == 0x16 ? 25
                                  : current * (wrong_scale ? 10 : 1);
      if (settings_enabled && b[2] != 0x10 && b[2] != 0x11 &&
          b[2] != 0x12 && b[2] != 0x16 && b[2] != 0x17 && b[2] != 0x21)
        v = registers[b[2]];
      if (b[2] == 0x12 && numeric_measurement && on && numeric_ramp_reads) {
        --numeric_ramp_reads;
        v = 41;
      }
      reply = {0xca, 0xcb, uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v),
               0xce, 0xcf};
      if (b.size() == 9) {
        assert(b == px3_command(b[2]));
        reply = {
            0xca, 0xcb, b[2], uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0,
            0,    0xce, 0xcf};
      }
    } else {
      if (b[2] == 1) {
        if (b.size() == 9 && b[3] == 1)
          assert(persisted_stop_pending);
        if (!(ignore_numeric_on && b[3] == 1) &&
            !(ignore_numeric_off && b[3] == 0))
          on = b[3];
        writes.push_back("load:output:" + std::to_string(b[3]));
      }
      if (b[2] == 2 && !ignore_numeric_setpoint)
        current = b.size() == 9 ? be24(&b[3]) : unsigned(b[3]) * 100 + b[4];
      if (settings_enabled) {
        if (b[2] == 2 && mode < 4)
          mode_currents[mode] = current;
        if (b[2] == 0x4c)
          settings_menu = false;
        for (auto &s : setting_checks)
          if (s.write == b[2] && b[2] != 2) {
            assert(!persisted_setting.empty());
            uint32_t value = be24(&b[3]);
            if (s.write == 6) {
              mode_currents[mode] = current;
              mode = value;
              current = mode_currents[mode];
            } else {
              if (s.write == 0x41)
                assert(settings_menu);
              registers[s.read] = s.read_factor ? value * s.read_factor :
                                  ((value / 3600) << 16) | (((value % 3600) / 60) << 8);
            }
            if (fail_setting_reply)
              drop_replies = true;
            break;
          }
        if (b[2] == 5)
          registers[0x13] = registers[0x14] = registers[0x15] = 0;
      }
      reply = b.size() == 9 ? Bytes{} : Bytes{0x6f};
    }
    if (drop_replies)
      reply.clear();
    return true;
  }
  Bytes receive() override {
    Bytes b;
    reply.swap(b);
    if (bench_reports && clock_ms - last_report_at >= 1000) {
      last_report_at = clock_ms;
      Bytes report(36);
      report[0] = 0xff;
      report[1] = 0x55;
      report[2] = 1;
      report[3] = 2;
      report[6] = 239;
      unsigned ma = on ? bench_current_ma : 0;
      report[8] = ma >> 8;
      report[9] = ma;
      report[25] = 25;
      uint8_t sum = 0;
      for (unsigned i = 2; i < 35; ++i)
        sum += report[i];
      report[35] = sum ^ 0x44;
      b.insert(b.end(), report.begin(), report.end());
    }
    return b;
  }
};
static void reset() {
  clock_ms = 100;
  wifi = true;
  config = Config{};
  persisted_stop_pending = false;
  persisted_setting.clear();
  load_desynced = false;
  load_stream.clear();
  lease.clear();
  stop_reason.clear();
  writes.clear();
  config.serials = {"123", "456"};
  config.load_verified = true;
  config.load_profile = "px100-centi";
  for (auto &l : config.limits)
    l = {15, .5, 6};
  load = Device{};
  load.link = std::make_shared<FakeLoad>();
  load.connected = true;
  load.received = 100;
  load.output = 0;
  for (int i = 0; i < 2; i++) {
    psus[i] = Device{};
    psus[i].link = std::make_shared<FakeRD>(i ? 456 : 123);
    assert(rd_poll(psus[i]));
  }
}
static cJSON *call(const char *method, const char *path, const char *json,
                   int expected, const char *token = "test-token") {
  auto body = cJSON_Parse(json);
  int code = 0;
  auto result = api_call(method, path, body, token, code);
  cJSON_Delete(body);
  if (code != expected) {
    char *s = cJSON_PrintUnformatted(result);
    std::cerr << path << " -> " << code << " " << s << " expected " << expected
              << "\n";
    cJSON_free(s);
  }
  assert(code == expected);
  return result;
}
static void request(const char *path, const char *json, int expected = 200) {
  cJSON_Delete(call("POST", path, json, expected));
}
} // namespace lab
int main() {
  using namespace lab;
  reset();
  config.ble_address = "aa:bb:cc:dd:ee:ff";
  auto discovery = discovery_json();
  auto ble_devices = cJSON_GetObjectItem(discovery, "ble");
  assert(cJSON_GetArraySize(ble_devices) == 1);
  assert(std::string(cJSON_GetArrayItem(ble_devices, 0)->valuestring)
             .find(config.ble_address) == 0);
  cJSON_Delete(discovery);
  reset();
  request("/session", R"({"devices":["load","psu1","psu2"]})");
  request("/session", R"({"devices":["load"]})", 409);
  request("/devices/psu1/command", R"({"voltage_v":12})");
  request("/devices/psu1/command", R"({"current_a":0.5})");
  request("/devices/psu1/command", R"({"current_a":0.51})", 422);
  request("/devices/load/command", R"({"voltage_v":12})", 422);
  request("/devices/load/command", R"({"current_a":0.105})", 422);
  request("/devices/load/command", R"({"current_a":0.1})");
  request("/devices/psu1/command", R"({"output":true})");
  request("/devices/load/command", R"({"output":true})");
  request("/session/stop", "{}");
  assert(writes[writes.size() - 3] == "123:18:0" &&
         writes[writes.size() - 2] == "456:18:0" &&
         writes.back() == "load:output:0");
  reset();
  config.load_verified = false;
  request("/session", R"({"devices":["load"]})", 409);
  request(
      "/commission/load",
      R"({"source_physically_disconnected":true,"cc_mode_selected":true,"display_scaling_checked":true})");
  assert(config.load_verified);
  reset();
  config.load_verified = false;
  std::static_pointer_cast<FakeLoad>(load.link)->wrong_scale = true;
  request(
      "/commission/load",
      R"({"source_physically_disconnected":true,"cc_mode_selected":true,"display_scaling_checked":true})",
      409);
  assert(!config.load_verified);
  reset();
  request("/session", R"({"devices":["psu1"]})");
  auto old = std::static_pointer_cast<FakeRD>(psus[0].link);
  old->serial = 999;
  request("/devices/psu1/command", R"({"output":true})", 504);
  assert(writes.empty());
  reset();
  request("/session", R"({"devices":["psu1"]})");
  clock_ms = 10001; // still before 10100 deadline
  request("/session/heartbeat", "{}");
  clock_ms = 20001;
  request("/devices/psu1/command", R"({"output":true})", 409);
  uint64_t last = clock_ms;
  poll_once(last);
  assert(!lease.active() && stop_reason == "device_unavailable");
  reset();
  request("/session", R"({"devices":["load","psu1"]})");
  wifi = false;
  last = clock_ms;
  poll_once(last);
  assert(!lease.active() && stop_reason == "wifi_disconnected");
  reset();
  config.serials[1] = "123";
  std::static_pointer_cast<FakeRD>(psus[1].link)->serial = 123;
  rd_poll(psus[1]);
  assert(device(1) == nullptr);
  reset();
  auto bad = std::static_pointer_cast<FakeRD>(psus[0].link);
  bad->corrupt = true;
  assert(!rd_poll(psus[0]));
  reset();
  request("/session", R"({"devices":["psu1"]})");
  std::static_pointer_cast<FakeRD>(psus[0].link)->fail_write = true;
  request("/devices/psu1/command", R"({"output":true})", 504);
  assert(!lease.active() && stopped_outputs[1] == -1);
  // A front-panel change after the last cached sample must not bypass limits.
  reset();
  request("/session", R"({"devices":["psu1"]})");
  auto changed = std::static_pointer_cast<FakeRD>(psus[0].link);
  changed->volts = 1200;
  changed->amps = 200;
  request("/devices/psu1/command", R"({"output":true})", 504);
  assert(changed->on == 0);
  for (const auto &write : writes)
    assert(write != "123:18:1");
  // Disabled optional slots may have zero limits without blocking setup.
  reset();
  request(
      "/config",
      R"({"limits":[{"voltage_v":15,"current_a":0.5,"power_w":6},{"voltage_v":15,"current_a":0.5,"power_w":6},{"voltage_v":0,"current_a":0,"power_w":0}]})");
  assert(!config.limits[2].valid());
  reset();
  request("/session", R"({"devices":["load","psu1"]})");
  std::static_pointer_cast<FakeLoad>(load.link)->drop_replies = true;
  request("/devices/load/command", R"({"current_a":0.1})", 504);
  assert(load_desynced && !lease.active());
  assert(writes.size() == 2 && writes[0] == "123:18:0" &&
         writes[1] == "load:output:0");
  assert(stopped_outputs[0] == -1 && stopped_outputs[1] == 0);
  // The physical USB API uses the same lease and device readiness checks.
  auto console = [](const char *json, int expected) {
    auto reply = serial_reply(json);
    assert(num(reply, "status") == expected);
    cJSON_Delete(reply);
  };
  reset();
  console(
      R"({"id":1,"op":"post","path":"/devices/load/command","output":true})",
      400);
  console(R"({"id":2,"op":"get","path":"/devices/load/command"})", 404);
  console(R"({"op":"request","method":"POST","path":"/devices/psu1/command","body":{"output":true}})", 409);
  console(R"({"op":"request","method":"POST","path":"/session","body":[]})", 400);
  console(R"({"op":"request","method":"DELETE","path":"/devices"})", 400);
  console("null", 400);
  console("{broken", 400);
  console(R"({"op":"select","address":"bad"})", 400);
  assert(writes.empty() && config.load_verified);
  reset();
  wifi = false;
  console(R"({"op":"request","method":"POST","path":"/session","body":{"devices":["psu1"]}})", 200);
  assert(lease.active() && !lease.require_wifi);
  console(R"({"op":"request","method":"POST","path":"/devices/psu1/command","body":{"output":true},"lease":"bad"})", 409);
  assert(writes.empty());
  console(R"({"op":"request","method":"POST","path":"/devices/psu1/command","body":{"output":true},"lease":"test-token"})", 200);
  assert(std::static_pointer_cast<FakeRD>(psus[0].link)->on == 1);
  last = clock_ms;
  poll_once(last);
  assert(lease.active());
  console(R"({"op":"request","method":"POST","path":"/session/stop","body":{},"lease":"test-token"})", 200);
  assert(!lease.active() && std::static_pointer_cast<FakeRD>(psus[0].link)->on == 0);
  reset();
  wifi = false;
  console(R"({"op":"request","method":"POST","path":"/session","body":{"devices":["psu1"]}})", 200);
  clock_ms += 10001;
  assert(rd_poll(psus[0]));
  last = clock_ms;
  poll_once(last);
  assert(!lease.active() && stop_reason == "lease_expired");
  // The legacy key pulse cannot become a general actuator API. It requires
  // explicit bench conditions and leaves the monitor profile unverified.
  console(R"({"op":"pulse"})", 400);
  for (int scenario = 0; scenario < 4; ++scenario) {
    reset();
    config.load_profile = "monitor";
    config.load_verified = false;
    load.reading = {23.9, 0, 0, 0, 0, 25};
    load.sequence = 1;
    auto bench = std::static_pointer_cast<FakeLoad>(load.link);
    bench->bench_reports = true;
    bench->ignore_start = scenario == 1;
    bench->ignore_stop = scenario == 2;
    bench->bench_current_ma = scenario == 3 ? 500 : 100;
    auto confirmation = cJSON_Parse(
        R"({"cc_0_10a_output_off_confirmed":true,"source_24v_1a_confirmed":true})");
    int code = 0;
    auto pulse = load_bench_pulse(confirmation, code);
    assert(code == 200 && !config.load_verified && load.output == -1);
    assert(bench->toggle_count == (scenario == 1 ? 1u : 2u));
    assert(flag(pulse, "activation_observed") == (scenario != 1));
    assert(flag(pulse, "current_zero_after_stop") ==
           (scenario == 0 || scenario == 3));
    assert(flag(pulse, "manual_check_required") ==
           (scenario == 1 || scenario == 2));
    if (scenario == 0)
      assert(std::string(str(pulse, "reason")) == "measured_pulse_completed");
    if (scenario == 3)
      assert(std::string(str(pulse, "reason")) ==
             "measurement_outside_bench_limits");
    cJSON_Delete(pulse);
    cJSON_Delete(confirmation);
  }
  reset();
  console(
      R"({"id":3,"op":"select","address":"aa:bb:cc:dd:ee:ff","load_profile":"px100-centi","load_verified":true})",
      200);
  assert(writes.empty() && config.load_profile == "monitor" &&
         !config.load_verified);
  console(R"({"op":"get","path":"/devices/load"})", 200);
  reset();
  console(R"({"op":"probe","command":1,"output":true})", 200);
  auto probed = std::static_pointer_cast<FakeLoad>(load.link);
  assert(
      (probed->commands == std::vector<uint8_t>{0x10, 0x17, 0x11, 0x12, 0x16}));
  assert(writes.empty() && probed->on == 0 && probed->current == 0);
  reset();
  console(R"({"op":"probe","protocol":"px100-v3","query":"full"})", 200);
  probed = std::static_pointer_cast<FakeLoad>(load.link);
  assert(probed->commands.size() == 22 && writes.empty());
  assert(std::find(probed->commands.begin(), probed->commands.end(), 0x22)
         == probed->commands.end());
  assert(std::find(probed->commands.begin(), probed->commands.end(), 0x31)
         == probed->commands.end());
  for (const char *options : {R"({"command_gap_ms":-1})",
                              R"({"command_gap_ms":501})",
                              R"({"command_gap_ms":0.1})",
                              R"({"command_gap_ms":"0"})"}) {
    reset();
    auto body = cJSON_Parse(options);
    int status;
    auto result = api_call("POST", "/diagnostics/load/probe", body, "", status, true);
    assert(status == 400 && std::static_pointer_cast<FakeLoad>(load.link)->commands.empty());
    cJSON_Delete(result);
    cJSON_Delete(body);
  }
  for (unsigned gap : {0u, 20u, 100u, 500u}) {
    reset();
    load.received = 0; // Numeric read-only queries work without an FF55 report.
    auto body = cJSON_CreateObject();
    cJSON_AddNumberToObject(body, "command_gap_ms", gap);
    cJSON_AddStringToObject(body, "query", "full");
    int status;
    auto denied = api_call("POST", "/diagnostics/load/probe", body, "", status, false);
    assert(status == 403);
    cJSON_Delete(denied);
    auto result = api_call("POST", "/diagnostics/load/probe", body, "", status, true);
    assert(status == 200 && flag(result, "all_queries_answered") && writes.empty());
    assert(num(result, "command_gap_ms") == gap);
    assert(std::static_pointer_cast<FakeLoad>(load.link)->commands.size() == 22);
    cJSON_Delete(result);
    cJSON_Delete(body);
  }
  reset();
  load.received = 0;
  console(R"({"op":"probe","protocol":"px100"})", 409);
  assert(std::static_pointer_cast<FakeLoad>(load.link)->commands.empty());
  reset();
  config.load_profile = "monitor";
  config.load_verified = false;
  load.reading = {24, 0, 0, 0, 0, 25};
  auto no_gap_load = std::static_pointer_cast<FakeLoad>(load.link);
  no_gap_load->current = 100;
  auto no_gap_options = cJSON_Parse(R"({"command_gap_ms":0})");
  int no_gap_status = 0;
  auto no_gap = api_call("POST", "/diagnostics/load/write-check", no_gap_options,
                         "", no_gap_status, true);
  assert(no_gap_status == 200 && flag(no_gap, "passed") && !config.load_stop_pending);
  assert(no_gap_load->on == 0 && no_gap_load->current == 100);
  assert(std::find(writes.begin(), writes.end(), "load:output:1") == writes.end());
  cJSON_Delete(no_gap);
  cJSON_AddBoolToObject(no_gap_options, "pulse", true);
  no_gap = api_call("POST", "/diagnostics/load/write-check", no_gap_options,
                    "", no_gap_status, true);
  assert(no_gap_status == 400);
  cJSON_Delete(no_gap);
  cJSON_Delete(no_gap_options);
  reset();
  console(R"({"op":"probe","protocol":"px100-v3","query":"setting_31"})", 400);
  console(R"({"op":"probe","protocol":"px100","query":"full"})", 400);
  assert(std::static_pointer_cast<FakeLoad>(load.link)->commands.empty());
  reset();
  console(R"({"op":"probe","query":"output_on"})", 400);
  probed = std::static_pointer_cast<FakeLoad>(load.link);
  assert(probed->commands.empty());
  console(R"({"op":"probe","protocol":"px100-v3","query":"voltage"})", 200);
  assert(probed->commands == std::vector<uint8_t>{0x11});
  assert(writes.empty() && probed->on == 0 && probed->current == 0);
  reset();
  console(R"({"op":"probe","protocol":"bad"})", 400);
  assert(std::static_pointer_cast<FakeLoad>(load.link)->commands.empty());
  console(R"({"op":"probe","protocol":"px100-v3","command":1,"output":true})",
          200);
  probed = std::static_pointer_cast<FakeLoad>(load.link);
  assert(
      (probed->commands == std::vector<uint8_t>{0x10, 0x17, 0x11, 0x12, 0x16}));
  assert(writes.empty() && probed->on == 0 && probed->current == 0);
  reset();
  probed = std::static_pointer_cast<FakeLoad>(load.link);
  probed->drop_replies = true;
  console(R"({"op":"probe"})", 200);
  assert((probed->commands == std::vector<uint8_t>{0x10}));
  assert(load_desynced && writes.empty());
  reset();
  request("/session", R"({"devices":["load","psu1"]})");
  console(R"({"op":"select","address":"aa:bb:cc:dd:ee:ff"})", 409);
  console(R"({"op":"probe"})", 409);
  console(
      R"({"op":"pulse","cc_0_10a_output_off_confirmed":true,"source_24v_1a_confirmed":true})",
      409);
  assert(writes.empty() && config.load_verified);
  for (int scenario = 0; scenario < 6; ++scenario) {
    reset();
    config.load_profile = "monitor";
    config.load_verified = false;
    load.reading = {24, 0, 0, 0, 0, 25};
    auto numeric = std::static_pointer_cast<FakeLoad>(load.link);
    numeric->current = scenario == 5 ? 200 : 99;
    numeric->mode = scenario == 1 ? 1 : 0;
    numeric->on = scenario == 2 ? 1 : 0;
    numeric->drop_replies = scenario == 3;
    numeric->ignore_numeric_setpoint = scenario == 4;
    int status = 0;
    auto denied = api_call("POST", "/diagnostics/load/write-check", nullptr,
                           "", status, false);
    assert(status == 403 && numeric->commands.empty());
    cJSON_Delete(denied);
    auto options = cJSON_Parse(scenario == 5 ? R"({"restore_only":true})" : "{}");
    auto result = api_call("POST", "/diagnostics/load/write-check", options,
                           "", status, true);
    cJSON_Delete(options);
    assert(status == 200 && !config.load_verified);
    assert(flag(result, "passed") == (scenario == 0 || scenario == 5));
    assert(numeric->toggle_count == 0);
    if (scenario >= 1 && scenario <= 3) {
      assert(writes.empty());
      assert(std::find(numeric->commands.begin(), numeric->commands.end(),
                       2) == numeric->commands.end());
    } else {
      assert(numeric->on == 0);
      if (scenario == 0 || scenario == 5) {
        assert(numeric->current == 100);
        assert(flag(result, "restored_exactly"));
        if (scenario == 5)
          assert(std::count(numeric->commands.begin(), numeric->commands.end(),
                            2) == 1);
      } else {
        assert(flag(result, "manual_check_required"));
      }
    }
    cJSON_Delete(result);
  }
  for (int scenario = 0; scenario < 4; ++scenario) {
    reset();
    config.load_profile = "monitor";
    config.load_verified = false;
    load.reading = {24, 0, 0, 0, 0, 25};
    auto numeric = std::static_pointer_cast<FakeLoad>(load.link);
    numeric->current = 100;
    numeric->numeric_measurement = true;
    numeric->ignore_numeric_on = scenario == 1;
    numeric->bench_current_ma = scenario == 2 ? 200 : 100;
    numeric->numeric_ramp_reads = scenario == 3 ? 1 : 0;
    auto options = cJSON_Parse(R"({"pulse":true})");
    int status = 0;
    auto result = api_call("POST", "/diagnostics/load/write-check", options,
                           "", status, true);
    assert(status == 200);
    assert(flag(result, "passed") == (scenario == 0 || scenario == 3));
    assert(flag(result, "activation_confirmed") == (scenario != 1));
    assert(flag(result, "output_off_confirmed"));
    assert(flag(result, "zero_current_confirmed"));
    assert(numeric->on == 0 && numeric->current == 100);
    assert(std::count(writes.begin(), writes.end(), "load:output:1") == 1);
    cJSON_Delete(result);
    cJSON_Delete(options);
  }
  reset();
  config.load_profile = "dl24m-ble";
  config.load_verified = false;
  config.limits[0] = {25, .2, 5};
  load.reading = {24, 0, 0, 0, 0, 25};
  auto production = std::static_pointer_cast<FakeLoad>(load.link);
  production->current = 100;
  // Real numeric mode can suppress unsolicited FF55 reports. The regular
  // API must obtain fresh measurements from numeric queries alone.
  production->bench_reports = false;
  production->numeric_measurement = true;
  auto options = cJSON_Parse(R"({"bench_24v_1a":true})");
  int status = 0;
  auto commissioned = api_call("POST", "/commission/load", options,
                               "", status, true);
  assert(status == 200 && flag(commissioned, "passed") &&
         config.load_verified && load.output == 0);
  cJSON_Delete(commissioned);
  cJSON_Delete(options);
  request("/session", R"({"devices":["load"]})");
  clock_ms = lease.deadline - 100;
  load.received = clock_ms; // a new measurement immediately before command
  request("/devices/load/command", R"({"current_a":0.150})");
  assert(production->current == 150 && lease.deadline > clock_ms);
  request("/devices/load/command", R"({"current_a":0.1505})", 422);
  request("/session/heartbeat", "{}");
  request("/devices/load/command", R"({"output":true})");
  assert(production->on == 1);
  request("/session/heartbeat", "{}");
  request("/devices/load/command", R"({"current_a":0.200})");
  assert(production->current == 200);
  request("/session/heartbeat", "{}");
  request("/devices/load/command", R"({"output":false})");
  assert(production->on == 0);
  request("/session/heartbeat", "{}");
  production->mode = 1; // front-panel change must block CC activation
  request("/devices/load/command", R"({"output":true})", 504);
  assert(production->on == 0 && !config.load_verified && !lease.active());

  // Stop remains available after a lease has disappeared and commissioning
  // has been revoked. It must never send a toggle or ON.
  for (unsigned scenario = 0; scenario < 5; ++scenario) {
    reset();
    config.load_profile = "dl24m-ble";
    config.load_verified = false;
    auto n = std::static_pointer_cast<FakeLoad>(load.link);
    n->on = 1;
    n->numeric_measurement = true;
    n->online = scenario != 1;
    n->drop_replies = scenario == 2;
    n->ignore_numeric_off = scenario == 3;
    n->off_current_ma = scenario == 4 ? 100 : 0;
    auto stopped = call("POST", "/devices/load/emergency-stop", "{}", 200, "");
    assert(flag(stopped, "output_off_confirmed") == (scenario == 0));
    assert(config.load_stop_pending == (scenario != 0));
    assert(persisted_stop_pending == config.load_stop_pending);
    cJSON_Delete(stopped);
    assert(!config.load_verified && !lease.active());
    if (scenario) {
      request("/config", R"({"ble_address":"00:11:22:33:44:55"})", 409);
      request("/commission/load", "{}", 409);
      request("/session", R"({"devices":["load"]})", 409);
      // Disconnect/reconnect clears an ambiguous reply stream; the
      // durable request survives, and polling retries only absolute OFF.
      n->online = false;
      uint64_t last = 0;
      clock_ms += 2000;
      poll_once(last);
      n->online = true;
      n->drop_replies = false;
      n->ignore_numeric_off = false;
      n->off_current_ma = 0;
      clock_ms += 2000;
      poll_once(last);
      assert(!config.load_stop_pending && !persisted_stop_pending);
      assert(!config.load_verified && n->on == 0 && stopped_outputs[0] == 0);
    }
    assert(n->toggle_count == 0);
    assert(std::find(writes.begin(), writes.end(), "load:output:1") == writes.end());
  }
  // Lost link while an owned load is ON: stop cannot write, but it must
  // arm the durable request before clearing the lease.
  reset();
  config.load_profile = "dl24m-ble";
  request("/session", R"({"devices":["load"]})");
  auto lost = std::static_pointer_cast<FakeLoad>(load.link);
  lost->on = 1;
  lost->online = false;
  stop("device_unavailable");
  assert(!lease.active() && config.load_stop_pending && persisted_stop_pending);
  assert(!config.load_verified && stopped_outputs[0] == -1);
  // Boot-equivalent volatile reset with the saved pending flag retained.
  load_desynced = false;
  load.connected = false;
  load.received = 0;
  lost->online = true;
  last = 0;
  clock_ms += 2000;
  poll_once(last);
  assert(lost->on == 0 && !config.load_stop_pending && !config.load_verified);
  // Every reversible setting writes a durable original before mutation,
  // restores the exact original and leaves OFF/CC/current unchanged.
  for (const auto &s : setting_checks) {
    reset();
    config.load_profile = "dl24m-ble";
    config.load_verified = false;
    auto n = std::static_pointer_cast<FakeLoad>(load.link);
    n->settings_enabled = true;
    n->current = 100;
    uint32_t original = s.read_factor ? s.trial_expected + s.read_factor : 0;
    if (s.write == 6)
      original = 0;
    if (s.read == 0x31 || s.write == 0x45)
      original = 0;
    if (s.mode)
      original = n->mode_currents[s.mode];
    n->registers[s.read] = original;
    auto body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "setting", s.name);
    int status;
    auto denied = api_call("POST", "/diagnostics/load/setting-check", body, "", status, false);
    assert(status == 403 && n->commands.empty());
    cJSON_Delete(denied);
    auto result = api_call("POST", "/diagnostics/load/setting-check", body, "", status, true);
    assert(status == 200 && flag(result, "passed") && flag(result, "restored_exactly"));
    assert(!settings_pending() && persisted_setting.empty());
    assert((s.mode ? n->mode_currents[s.mode] :
            s.write == 6 ? n->mode : n->registers[s.read]) == original);
    assert(n->current == 100 && n->on == 0 && n->toggle_count == 0);
    assert(!n->settings_menu && n->mode == 0);
    cJSON_Delete(result);
    cJSON_Delete(body);
  }
  // An acknowledged absolute write can be ignored by the device. The
  // initial and delayed readbacks still show the original; one resend works.
  reset();
  config.load_profile = "dl24m-ble";
  config.load_verified = false;
  auto retrying = std::static_pointer_cast<FakeLoad>(load.link);
  retrying->settings_enabled = true;
  retrying->current = 100;
  retrying->registers[0x37] = 21000;
  retrying->ignore_next_setting_command = 0x47;
  auto retry_body = cJSON_Parse(R"({"setting":"overvoltage"})");
  int retry_status;
  auto retry_result = api_call("POST", "/diagnostics/load/setting-check",
                               retry_body, "", retry_status, true);
  assert(retry_status == 200 && flag(retry_result, "passed"));
  assert(num(retry_result, "write_attempts") == 2);
  assert(retrying->registers[0x37] == 21000 && !settings_pending());
  cJSON_Delete(retry_body);
  cJSON_Delete(retry_result);
  // Productive absolute settings commit only after exact readback; their
  // rollback journal is cleared only after OFF/CC/CC-target are confirmed.
  reset();
  config.load_profile = "dl24m-ble";
  config.load_verified = false;
  auto applying = std::static_pointer_cast<FakeLoad>(load.link);
  applying->settings_enabled = true;
  applying->current = 100;
  applying->registers[0x37] = 21000;
  auto apply_body = cJSON_Parse(R"({"setting":"overvoltage","wire_value":3000})");
  int apply_status;
  auto applied = api_call("POST", "/devices/load/setting", apply_body,
                          "", apply_status);
  assert(apply_status == 200 && flag(applied, "passed") &&
         flag(applied, "committed") && !settings_pending());
  assert(applying->registers[0x37] == 3000 && applying->on == 0 &&
         applying->mode == 0 && applying->current == 100);
  cJSON_Delete(apply_body);
  cJSON_Delete(applied);
  apply_body = cJSON_Parse(R"({"setting":"overvoltage","wire_value":21000})");
  applied = api_call("POST", "/devices/load/setting", apply_body,
                     "", apply_status);
  assert(apply_status == 200 && flag(applied, "passed") &&
         applying->registers[0x37] == 21000);
  cJSON_Delete(apply_body);
  cJSON_Delete(applied);
  apply_body = cJSON_Parse(R"({"setting":"setpoint_mode_1","wire_value":1000})");
  applied = api_call("POST", "/devices/load/setting", apply_body,
                     "", apply_status);
  assert(apply_status == 200 && flag(applied, "passed") &&
         applying->mode_currents[1] == 1000 && applying->mode == 0 &&
         applying->current == 100 && !settings_pending());
  cJSON_Delete(apply_body);
  cJSON_Delete(applied);
  apply_body = cJSON_Parse(R"({"setting":"setpoint_mode_1","wire_value":1200})");
  auto invalid_apply = api_call("POST", "/devices/load/setting", apply_body,
                                "", apply_status);
  assert(apply_status == 400 && applying->mode_currents[1] == 1000);
  cJSON_Delete(apply_body);
  cJSON_Delete(invalid_apply);
  apply_body = cJSON_Parse(R"({"setting":"cutoff_voltage","wire_value":251})");
  invalid_apply = api_call("POST", "/devices/load/setting", apply_body,
                           "", apply_status);
  assert(apply_status == 400 && !settings_pending());
  cJSON_Delete(apply_body);
  cJSON_Delete(invalid_apply);
  apply_body = cJSON_Parse(R"({"setting":"setpoint_mode_1","wire_value":500})");
  applied = api_call("POST", "/devices/load/setting", apply_body,
                     "", apply_status);
  assert(apply_status == 200 && flag(applied, "passed") &&
         applying->mode_currents[1] == 500 && applying->current == 100);
  cJSON_Delete(apply_body);
  cJSON_Delete(applied);
  reset();
  config.load_profile = "dl24m-ble";
  config.load_verified = false;
  auto cc_retry = std::static_pointer_cast<FakeLoad>(load.link);
  cc_retry->current = 100;
  cc_retry->ignore_next_numeric_setpoint = true;
  assert(load_current(0.150));
  assert(cc_retry->current == 150 && cc_retry->on == 0);
  assert(std::count(cc_retry->commands.begin(), cc_retry->commands.end(), 2) == 2);
  reset();
  config.load_profile = "dl24m-ble";
  config.load_verified = false;
  auto recovering = std::static_pointer_cast<FakeLoad>(load.link);
  recovering->settings_enabled = true;
  recovering->current = 100;
  recovering->registers[0x37] = 21000;
  recovering->fail_setting_reply = true;
  auto body = cJSON_Parse(R"({"setting":"overvoltage"})");
  int restore_status;
  auto failure = api_call("POST", "/diagnostics/load/setting-check", body, "", restore_status, true);
  assert(restore_status == 200 && !flag(failure, "passed") && settings_pending());
  assert(config.load_restore_expected == 21000 && config.load_restore_wire == 21000);
  assert(recovering->registers[0x37] == 3000);
  cJSON_Delete(body);
  cJSON_Delete(failure);
  request("/config", "{}", 409);
  request("/session", R"({"devices":["load"]})", 409);
  // Boot/reconnect-equivalent: retain NVS original, clear volatile parser.
  recovering->drop_replies = recovering->fail_setting_reply = false;
  load_desynced = false;
  assert(restore_load_setting());
  assert(recovering->registers[0x37] == 21000 && recovering->current == 100);
  assert(!settings_pending() && recovering->on == 0);
  recovering->registers[0x13] = 0x010203;
  recovering->registers[0x14] = 2505;
  recovering->registers[0x15] = 12678;
  body = cJSON_Parse(R"({"confirm_reset":true})");
  auto reset_result = api_call("POST", "/diagnostics/load/reset-counters", body,
                               "", restore_status, true);
  assert(restore_status == 200 && flag(reset_result, "passed"));
  assert(recovering->registers[0x13] == 0 && recovering->registers[0x14] == 0 &&
         recovering->registers[0x15] == 0 && recovering->on == 0);
  cJSON_Delete(body);
  cJSON_Delete(reset_result);
  // Crash/timeout after entering a menu or bank but before its original
  // value is read: exit/restore CC without inventing a saved parameter.
  for (const char *name : {"language", "setpoint_mode_1"}) {
    reset();
    config.load_profile = "dl24m-ble";
    config.load_verified = false;
    auto n = std::static_pointer_cast<FakeLoad>(load.link);
    n->settings_enabled = true;
    n->current = 100;
    n->registers[0x31] = 1;
    n->fail_menu_read = std::string(name) == "language";
    n->fail_setting_reply = !n->fail_menu_read;
    body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "setting", name);
    auto result = api_call("POST", "/diagnostics/load/setting-check", body, "",
                           restore_status, true);
    assert(restore_status == 200 && !flag(result, "passed") && settings_pending());
    assert(config.load_restore_expected == std::numeric_limits<uint32_t>::max());
    n->drop_replies = n->fail_menu_read = n->fail_setting_reply = false;
    load_desynced = false;
    clock_ms += 2000;
    last = 0;
    poll_once(last);
    assert(!settings_pending() && !n->settings_menu && n->mode == 0 &&
           n->current == 100 && n->registers[0x31] == 1 &&
           n->mode_currents[1] == 1200 && n->on == 0);
    cJSON_Delete(body);
    cJSON_Delete(result);
  }
  std::cout << "firmware controller tests passed\n";
}

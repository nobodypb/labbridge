// Exercise the production SPP coordinator with a deterministic PC-protocol peer.
#include "../main/devices.cpp"
#include <cassert>
#include <iostream>

static uint64_t clock_ms = 100;
void vTaskDelay(uint32_t ms) { clock_ms += ms; }
namespace lab {
Config config;
SemaphoreHandle_t state_mutex = nullptr;
uint64_t now_ms() { return clock_ms; }
bool wifi_connected() { return true; }
cJSON *network_diagnostics() { return cJSON_CreateObject(); }
std::string random_token() { return "spp-test-lease"; }
void save_config() {}
void enter_setup() {}
void select_ble(const std::string &) {}
std::vector<std::string> discovered_ble() { return {}; }
cJSON *ble_diagnostics() { return cJSON_CreateObject(); }
std::shared_ptr<Link> ble_link() { return {}; }
void select_spp(const std::string &) {}
std::vector<std::string> discovered_spp() { return {}; }
cJSON *spp_diagnostics() { return cJSON_CreateObject(); }
std::shared_ptr<Link> spp_link() { return load.link; }
std::array<std::shared_ptr<Link>, 2> usb_links() { return {}; }
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

class FakeSpp : public Link {
public:
  uint32_t setpoint = 100, output = 0;
  bool online = true, ignore_write = false, fail_query = false;
  Bytes pending;
  bool connected() const override { return online; }
  bool send(const Bytes &frame) override {
    if (!online || (frame.size() != 9 && frame.size() != 6) ||
        frame[0] != 0xb1 || frame[1] != 0xb2 || frame.back() != 0xb6)
      return false;
    auto command = frame[2];
    if (frame.size() == 6) {
      uint32_t answer = command == 0x10 ? output : command == 0x17 ? setpoint : 0;
      pending = {0xca, 0xcb, uint8_t(answer >> 16), uint8_t(answer >> 8),
                 uint8_t(answer), 0xce, 0xcf};
      return true;
    }
    auto value = be24(&frame[3]);
    if (command == 1 || command == 2) {
      if (!ignore_write) {
        if (command == 1) output = value;
        else setpoint = value;
      }
      return true; // This protocol has no immediate write ACK.
    }
    if (fail_query)
      return true;
    uint32_t answer = command == 0x10 ? output : command == 0x17 ? setpoint :
                      command == 0x11 ? 12000 : command == 0x12 ? 0 : 25;
    pending = {0xca, 0xcb, command, uint8_t(answer >> 16),
               uint8_t(answer >> 8), uint8_t(answer), 0, 0, 0xce, 0xcf};
    return true;
  }
  Bytes receive() override {
    Bytes result;
    result.swap(pending);
    return result;
  }
};

static cJSON *request(const char *method, const char *path, const char *body,
                      int expected, const char *lease_token = "") {
  auto input = cJSON_Parse(body);
  int status = 0;
  auto answer = api_call(method, path, input, lease_token, status, true);
  cJSON_Delete(input);
  if (status != expected) {
    char *s = cJSON_PrintUnformatted(answer);
    std::cerr << path << ": " << status << " " << s << '\n';
    cJSON_free(s);
  }
  assert(status == expected);
  return answer;
}
} // namespace lab

int main() {
  using namespace lab;
  config.spp_address = "aa:bb:cc:dd:ee:ff";
  config.load_profile = "dl24m-spp";
  config.limits[0] = {15, .5, 6};
  auto peer = std::make_shared<FakeSpp>();
  load.link = peer;
  load.connected = true;
  load.received = now_ms();
  load.reading.voltage = 12;
  load.output = 0;

  auto legacy = request("POST", "/diagnostics/load/probe",
                         R"({"protocol":"spp-legacy","query":"output"})", 200);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(legacy, "all_queries_answered")));
  cJSON_Delete(legacy);

  auto probe = request("POST", "/diagnostics/load/probe",
                       R"({"protocol":"spp","query":"all"})", 200);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(probe, "all_queries_answered")));
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(probe, "queries")) == 5);
  cJSON_Delete(probe);

  auto commission = request("POST", "/commission/load",
      R"({"source_physically_disconnected":true,"cc_mode_selected":true,"display_scaling_checked":true})", 200);
  cJSON_Delete(commission);
  assert(config.load_verified && peer->setpoint == 0 && peer->output == 0);

  auto session = request("POST", "/session", R"({"devices":["load"]})", 200);
  cJSON_Delete(session);
  cJSON_Delete(request("POST", "/devices/load/command",
                        R"({"current_a":0.125})", 200, "spp-test-lease"));
  assert(peer->setpoint == 125);
  cJSON_Delete(request("POST", "/devices/load/command",
                        R"({"output":true})", 200, "spp-test-lease"));
  assert(peer->output == 1);
  cJSON_Delete(request("POST", "/devices/load/command",
                        R"({"current_a":0.2})", 200, "spp-test-lease"));
  assert(peer->setpoint == 200);
  cJSON_Delete(request("POST", "/session/stop", "{}", 200, "spp-test-lease"));
  assert(peer->output == 0 && config.load_verified);

  cJSON_Delete(request("POST", "/session", R"({"devices":["load"]})", 200));
  peer->ignore_write = true;
  cJSON_Delete(request("POST", "/devices/load/command",
                        R"({"current_a":0.25})", 504, "spp-test-lease"));
  assert(!config.load_verified && !lease.active());
  assert(peer->output == 0);
  cJSON_Delete(request("POST", "/session", R"({"devices":["load"]})", 409));
  config.load_verified = true;
  peer->online = false;
  uint64_t last_poll = now_ms();
  poll_once(last_poll);
  assert(!config.load_verified && load.output == -1);
  std::cout << "SPP coordinator tests passed\n";
}

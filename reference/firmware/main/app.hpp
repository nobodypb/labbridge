#pragma once
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "protocol.hpp"
#include <memory>
#include <mutex>
#include <string>

namespace lab {
uint64_t now_ms();
std::string random_token();
const char *str(const cJSON *, const char *, const char *fallback = "");
double num(const cJSON *, const char *, double fallback = 0);
bool flag(const cJSON *, const char *, bool fallback = false);
struct Config {
  std::string ssid, password, api_token;
  std::string ble_address, spp_address, spp_pin = "1234",
              load_profile = "monitor";
  bool ignore_checksum = false, load_verified = false;
  // Durable numeric BLE shutdown obligation, bound to the configured address.
  bool load_stop_pending = false;
  std::string load_restore_setting;
  uint32_t load_restore_wire = 0, load_restore_expected = 0, load_restore_cc = 0;
  std::array<std::string, 2> serials{};
  std::array<Limits, 3> limits{};
};
extern Config config;
extern SemaphoreHandle_t state_mutex;
struct Guard {
  Guard() { xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY); }
  ~Guard() { xSemaphoreGiveRecursive(state_mutex); }
};
void save_config();
void start_network();
void enter_setup();
std::string setup_ssid();
bool wifi_connected();
cJSON *network_diagnostics();
void start_devices();
void start_serial_diagnostics();
cJSON *devices_json();
cJSON *discovery_json();
cJSON *configuration_json();
cJSON *load_bench_pulse(const cJSON *confirmation, int &status);
// Returns owned JSON and HTTP status; called under state lock.
cJSON *api_call(const std::string &method, const std::string &path,
                const cJSON *body, const std::string &lease, int &status,
                bool local_usb = false);

// Byte transport: no knowledge of Modbus or ATORCH commands.
class Link {
public:
  virtual ~Link() = default;
  virtual bool connected() const = 0;
  virtual bool send(const Bytes &) = 0;
  // ATORCH's phone app uses a GATT Write Command for remote keys when FFE1
  // advertises WRITE_NO_RESPONSE. Other byte links may use their normal send.
  virtual bool send_without_response(const Bytes &bytes) { return send(bytes); }
  virtual Bytes receive() = 0;
};
void start_transports();
void select_ble(const std::string &address);
std::vector<std::string> discovered_ble();
cJSON *ble_diagnostics();
std::shared_ptr<Link> ble_link();
std::shared_ptr<Link> spp_link();
void select_spp(const std::string &address);
std::vector<std::string> discovered_spp();
cJSON *spp_diagnostics();
std::array<std::shared_ptr<Link>, 2> usb_links();
} // namespace lab

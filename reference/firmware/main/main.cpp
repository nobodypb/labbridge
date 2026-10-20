#include "app.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cstdio>

namespace lab {
Config config;
SemaphoreHandle_t state_mutex;
uint64_t now_ms() { return esp_timer_get_time() / 1000; }
std::string random_token() {
  char s[33];
  for (int i = 0; i < 4; i++)
    snprintf(s + i * 8, 9, "%08lx", (unsigned long)esp_random());
  return s;
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
static void read_limits(const cJSON *j) {
  auto a = cJSON_GetObjectItem(j, "limits");
  for (int i = 0; i < 3; i++) {
    auto v = cJSON_GetArrayItem(a, i);
    config.limits[i] = {num(v, "voltage_v"), num(v, "current_a"),
                        num(v, "power_w")};
  }
}
void save_config() {
  auto j = configuration_json();
  cJSON_AddStringToObject(j, "ssid", config.ssid.c_str());
  cJSON_AddStringToObject(j, "password", config.password.c_str());
  cJSON_AddStringToObject(j, "api_token", config.api_token.c_str());
  char *s = cJSON_PrintUnformatted(j);
  nvs_handle_t n;
  ESP_ERROR_CHECK(nvs_open("labbridge", NVS_READWRITE, &n));
  ESP_ERROR_CHECK(nvs_set_str(n, "config", s));
  ESP_ERROR_CHECK(nvs_commit(n));
  nvs_close(n);
  cJSON_free(s);
  cJSON_Delete(j);
}
static void load_config() {
  nvs_handle_t n;
  ESP_ERROR_CHECK(nvs_open("labbridge", NVS_READWRITE, &n));
  size_t len = 0;
  if (nvs_get_str(n, "config", nullptr, &len) == ESP_OK && len < 8192) {
    std::string s(len, '\0');
    nvs_get_str(n, "config", s.data(), &len);
    auto j = cJSON_Parse(s.c_str());
    config.ssid = str(j, "ssid");
    config.password = str(j, "password");
    config.api_token = str(j, "api_token");
    config.ble_address = str(j, "ble_address");
    config.spp_address = str(j, "spp_address");
    config.spp_pin = str(j, "spp_pin", "1234");
    config.load_profile = str(j, "load_profile", "monitor");
    config.ignore_checksum = flag(j, "ignore_checksum");
    config.load_verified = flag(j, "load_verified");
    config.load_stop_pending = flag(j, "load_stop_pending");
    config.load_restore_setting = str(j, "load_restore_setting");
    config.load_restore_wire = num(j, "load_restore_wire");
    config.load_restore_expected = num(j, "load_restore_expected");
    config.load_restore_cc = num(j, "load_restore_cc");
    if (config.load_stop_pending || !config.load_restore_setting.empty())
      config.load_verified = false;
    config.serials[0] = str(j, "psu1_serial");
    config.serials[1] = str(j, "psu2_serial");
    read_limits(j);
    cJSON_Delete(j);
  }
  nvs_close(n);
  bool fresh = config.api_token.empty();
  if (fresh)
    config.api_token = random_token();
  if (fresh) {
    save_config();
    printf("FIRST SETUP: connect to the open LabBridge-Setup Wi-Fi, then open "
           "http://192.168.4.1/\n");
  }
}
static void button_task(void *) {
#if CONFIG_LAB_BOARD_STICKS3
  constexpr auto button = GPIO_NUM_11;
#else
  constexpr auto button = GPIO_NUM_0;
#endif
  gpio_set_direction(button, GPIO_MODE_INPUT);
  gpio_set_pull_mode(button, GPIO_PULLUP_ONLY);
  uint64_t pressed = 0;
  bool done = false;
  for (;;) {
    if (gpio_get_level(button) == 0) {
      if (!pressed)
        pressed = now_ms();
      if (!done && now_ms() - pressed > 4000) {
        enter_setup();
        done = true;
      }
    } else {
      pressed = 0;
      done = false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
} // namespace lab
extern "C" void app_main() {
  using namespace lab;
  auto err = nvs_flash_init(); // Never silently erase stored identities/limits
                               // on an NVS error.
  ESP_ERROR_CHECK(err);
  state_mutex = xSemaphoreCreateRecursiveMutex();
  load_config();
  start_network();
  start_transports();
#if CONFIG_IDF_TARGET_ESP32
  select_spp(config.spp_address);
#else
  select_ble(config.ble_address);
#endif
  start_devices();
  start_serial_diagnostics();
  xTaskCreate(button_task, "setup_button", 3072, nullptr, 3, nullptr);
}

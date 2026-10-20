#include "dl24m_driver.hpp"
#include "esp_app_desc.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "labbridge/catalog.hpp"
#include "nvs.h"
#include "nvs_flash.h"
#include "platform.hpp"
#include "rd60xx_driver.hpp"
#include <sstream>
namespace labbridge::esp {
std::recursive_mutex mutex;
std::unique_ptr<Node> node;
Json configuration;
BoardProfile board;
static Catalog catalog;
static std::vector<std::shared_ptr<Endpoint>> endpoints;
static uint64_t reboot_at = 0;
uint64_t now_ms() { return esp_timer_get_time() / 1000; }
std::string random_id() {
  char s[33];
  for (int i = 0; i < 4; ++i)
    snprintf(s + i * 8, 9, "%08lx", static_cast<unsigned long>(esp_random()));
  return s;
}
Json read_nvs(const std::string &key, Json fallback) {
  nvs_handle_t n;
  ESP_ERROR_CHECK(nvs_open("labbridge2", NVS_READWRITE, &n));
  size_t size = 0;
  if (nvs_get_str(n, key.c_str(), nullptr, &size) != ESP_OK || size > 32768) {
    nvs_close(n);
    return fallback;
  }
  std::string text(size, '\0');
  auto err = nvs_get_str(n, key.c_str(), text.data(), &size);
  nvs_close(n);
  ESP_ERROR_CHECK(err);
  if (!text.empty() && text.back() == '\0')
    text.pop_back();
  return Json::parse(text);
}
void write_nvs(const std::string &key, const Json &value) {
  nvs_handle_t n;
  ESP_ERROR_CHECK(nvs_open("labbridge2", NVS_READWRITE, &n));
  auto text = value.dump();
  ESP_ERROR_CHECK(nvs_set_str(n, key.c_str(), text.c_str()));
  ESP_ERROR_CHECK(nvs_commit(n));
  nvs_close(n);
}
static Limits limits(const Json &j) {
  auto l = j.item("limits");
  return {l.number("voltage_v"), l.number("current_a"), l.number("power_w"),
          uint64_t(l.number("max_age_ms", 3000)),
          uint64_t(l.number("max_duration_s")) * 1000};
}
static auto persistence(const Json &j) {
  auto key = j.text("driver") + ":" + j.text("endpoint");
  return [key](const Json &value) {
    auto journal = read_nvs("journals");
    journal.set(key, value);
    write_nvs("journals", journal);
  };
}
static Json journal(const Json &j) {
  return read_nvs("journals").item(j.text("driver") + ":" + j.text("endpoint"));
}
static void modules() {
  catalog.register_module("dl24m", {"ble"}, [](const Json &j) {
    auto wire = ble_endpoint(j.text("endpoint"));
    endpoints.push_back(wire);
    return std::make_shared<Dl24mDriver>(j.text("id"), j.text("endpoint"),
                                         *wire, limits(j), persistence(j),
                                         journal(j));
  });
  catalog.register_module("rd60xx", {"usb", "uart"}, [](const Json &j) {
    auto wire =
        j.text("transport") == "usb"
            ? usb_endpoint(j.text("endpoint"))
            : uart_endpoint({j.text("id"), j.text("driver"),
                             j.text("transport"), j.text("endpoint"), pins(j)});
    endpoints.push_back(wire);
    return std::make_shared<Rd60xxDriver>(
        j.text("id"), std::stoul(j.text("endpoint")), *wire, limits(j),
        persistence(j), journal(j), j.text("transport"));
  });
  catalog.register_module("gpio_interlock", {"gpio"}, switch_driver);
  catalog.register_module("ds18b20", {"onewire"}, probe_driver);
}
static void validate_configuration(const Json &c) {
  if (c.text("board") != board.id)
    throw ConfigurationError(
        "Configuration board does not match firmware build");
  auto registry = catalog.validate(c, board);
  (void)registry;
  auto a = c.at("devices");
  for (auto p = a->child; p; p = p->next) {
    Json j(cJSON_Duplicate(p, true));
    auto d = j.text("driver");
    auto count = pins(j).size();
    if (!limits(j).valid())
      throw ConfigurationError("Positive safety limits required");
    if (d == "dl24m") {
      auto e = j.text("endpoint");
      if (e.size() != 17)
        throw ConfigurationError("Invalid BLE address");
      for (size_t i = 0; i < e.size(); ++i)
        if (i % 3 == 2 ? e[i] != ':'
                       : !std::isxdigit(static_cast<unsigned char>(e[i])))
          throw ConfigurationError("Invalid BLE address");
    }
    if (d == "rd60xx") {
      auto l = limits(j);
      if (l.voltage > 60 || l.current > 24 || l.power > 1440)
        throw ConfigurationError(
            "RD6024 limits exceed supported hardware range");
      auto s = j.text("endpoint");
      if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos ||
          std::stoull(s) > UINT32_MAX || std::stoull(s) == 0)
        throw ConfigurationError(
            "RD endpoint must be instrument register serial");
      if (j.text("transport") == "uart" && count != 2)
        throw ConfigurationError("UART needs TX,RX pins");
    }
    if (d == "gpio_interlock" && (count != 4 || !j.has("interlock")))
      throw ConfigurationError(
          "Interlock needs four pins and explicit polarity/break settings");
    if (d == "ds18b20" &&
        (count != 1 || j.text("endpoint").size() != 16 ||
         j.text("endpoint").find_first_not_of("0123456789abcdefABCDEF") !=
             std::string::npos))
      throw ConfigurationError("Probe requires one pin and ROM identity");
  }
}
static void safety_loop(void *) {
  for (;;) {
    {
      std::lock_guard<std::recursive_mutex> lock(mutex);
      node->tick(now_ms());
      if (reboot_at && now_ms() >= reboot_at)
        esp_restart();
    }
    sleep_ms(5);
  }
}
static void migrate_legacy_wifi() {
  if (read_nvs("wifi").has("ssid"))
    return;
  nvs_handle_t n;
  if (nvs_open("labbridge", NVS_READONLY, &n) != ESP_OK)
    return;
  size_t len = 0;
  if (nvs_get_str(n, "config", nullptr, &len) != ESP_OK || len < 2 ||
      len > 8192) {
    nvs_close(n);
    return;
  }
  std::string text(len, '\0');
  auto err = nvs_get_str(n, "config", text.data(), &len);
  nvs_close(n);
  if (err != ESP_OK)
    return;
  if (!text.empty() && text.back() == '\0')
    text.pop_back();
  try {
    auto old = Json::parse(text);
    auto ssid = old.text("ssid"), psk = old.text("password");
    if (ssid.empty() || ssid.size() > 32 || psk.size() > 64 ||
        (!psk.empty() && psk.size() < 8))
      return;
    Json wifi;
    wifi.set("ssid", ssid).set("psk", psk);
    write_nvs("wifi", wifi);
  } catch (const std::invalid_argument &) {
  }
}
static void initialise() {
#if CONFIG_LAB_BOARD_STICKS3
  board = BoardProfile::stick_s3();
#elif CONFIG_IDF_TARGET_ESP32
  board = BoardProfile::esp32_devkit();

#else
  board = BoardProfile::s3_usb_otg();
#endif
  std::istringstream pins_stream(CONFIG_LAB_ALLOWED_PINS);
  std::string pin;
  while (std::getline(pins_stream, pin, ',')) {
    if (!pin.empty())
      board.available_pins.push_back(std::stoi(pin));
  }
  if (!board.available_pins.empty()) {
    board.capacity["gpio"] = 2;
    board.capacity["onewire"] = 2;
    board.capacity["uart"] = 1;
  }
  modules();
  configuration = read_nvs("configuration");
  if (!configuration.has("revision")) {
    configuration.set("revision", 1.)
        .set("node_id", "labbridge")
        .set("board", board.id)
        .set("devices", Json::array());
    write_nvs("configuration", configuration);
  }
  auto auth = read_nvs("auth");
  if (!auth.has("token")) {
    auth.set("token", random_id());
    write_nvs("auth", auth);
  }
  node = std::make_unique<Node>(
      configuration.text("node_id"), random_id(), board.id,
      std::string("0.1.0+") + esp_app_get_description()->version);
  try {
    validate_configuration(configuration);
    catalog.instantiate(*node, configuration, board);
  } catch (const std::exception &e) {
    printf("Configuration rejected: %s\n", e.what());
  }
  node->bind("getConfiguration", [](const Json &, const Headers &, uint64_t) {
    return Response{
        200,
        configuration,
        {{"ETag",
          "\"" + std::to_string(uint64_t(configuration.number("revision"))) +
              "\""}}};
  });
  node->bind("setConfiguration", [](const Json &body, const Headers &headers,
                                    uint64_t now) {
    if (!node->quiescent(now))
      return problem(409,
                     "Stop outputs and release leases before reconfiguration");
    std::string match;
    for (auto &h : headers)
      if (h.first == "If-Match" || h.first == "if-match")
        match = h.second;
    auto expected = "\"" +
                    std::to_string(uint64_t(configuration.number("revision"))) +
                    "\"";
    if (match != expected)
      return problem(412, "Configuration revision changed");
    try {
      validate_configuration(body);
    } catch (const std::exception &e) {
      return problem(422, e.what());
    }
    auto updated = body;
    updated.set("revision", configuration.number("revision") + 1);
    write_nvs("configuration", updated);
    configuration = updated;
    reboot_at = now + 500;
    return Response{
        200,
        updated,
        {{"ETag",
          "\"" + std::to_string(uint64_t(updated.number("revision"))) + "\""}}};
  });
  start_ble();
  start_usb();
  migrate_legacy_wifi();
  start_network();
  start_console();
  xTaskCreate(safety_loop, "node_safety", 12288, nullptr, 6, nullptr);
}
} // namespace labbridge::esp
extern "C" void app_main() {
  ESP_ERROR_CHECK(nvs_flash_init());
  labbridge::esp::initialise();
}

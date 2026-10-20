#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "labbridge/node.hpp"
#include "labbridge/resources.hpp"
#include "labbridge/timing.hpp"
#include "labbridge/transaction.hpp"
#include <mutex>
namespace labbridge::esp {
uint64_t now_ms();
inline void sleep_ms(uint32_t ms) {
  vTaskDelay(delay_ticks(ms, configTICK_RATE_HZ));
}
Json read_nvs(const std::string &key, Json fallback = Json());
void write_nvs(const std::string &key, const Json &value);
std::string random_id();
extern std::recursive_mutex mutex;
extern std::unique_ptr<Node> node;
extern Json configuration;
extern BoardProfile board;
void start_ble();
std::shared_ptr<Endpoint> ble_endpoint(const std::string &address);
void start_usb();
std::shared_ptr<Endpoint> usb_endpoint(const std::string &serial);
std::shared_ptr<Endpoint> uart_endpoint(const DeviceSpec &);
void start_network();
void start_rd_network();
bool rd_provisioning_active();
bool wifi_network_busy();
std::shared_ptr<Endpoint> tcp_endpoint(const std::string &, unsigned model);
Response dispatch(const std::string &, const std::string &, const Json &,
                  const Headers &, bool local = false);
void start_console();
} // namespace labbridge::esp
namespace labbridge::esp {
std::vector<int> pins(const Json &);
std::shared_ptr<Driver> switch_driver(const Json &);
std::shared_ptr<Driver> probe_driver(const Json &);
std::shared_ptr<Driver> ina228_driver(const Json &);
std::shared_ptr<Driver> yocto_watt_driver(const Json &);
bool valid_hub_url(const std::string &);
} // namespace labbridge::esp

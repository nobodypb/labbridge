#include "driver/i2c_master.h"
#include "esp_http_client.h"
#include "ina228.hpp"
#include "platform.hpp"
#include "yocto_watt.hpp"
#include <cctype>
#include <memory>
namespace labbridge::esp {
namespace {
class I2c : public ina228::RegisterIo {
  i2c_master_dev_handle_t device_ = nullptr;

public:
  explicit I2c(const Json &j) {
    static i2c_master_bus_handle_t bus = nullptr;
    static std::vector<int> bus_pins;
    auto p = pins(j);
    if (p.size() != 2)
      throw std::invalid_argument("I2C needs SDA,SCL");
    if (!bus) {
      i2c_master_bus_config_t config = {};
      config.i2c_port = I2C_NUM_0;
      config.sda_io_num = gpio_num_t(p[0]);
      config.scl_io_num = gpio_num_t(p[1]);
      config.clk_source = I2C_CLK_SRC_DEFAULT;
      config.glitch_ignore_cnt = 7;
      config.flags.enable_internal_pullup = true;
      ESP_ERROR_CHECK(i2c_new_master_bus(&config, &bus));
      bus_pins = p;
    }
    if (bus_pins != p)
      throw std::invalid_argument("I2C0 pin mismatch");
    i2c_device_config_t c = {};
    c.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    c.device_address = std::stoul(j.text("endpoint"));
    c.scl_speed_hz = 100000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &c, &device_));
  }
  bool read(uint8_t r, uint8_t *out, size_t n) override {
    return i2c_master_transmit_receive(device_, &r, 1, out, n, 30) == ESP_OK;
  }
  bool write(uint8_t r, uint16_t value) override {
    uint8_t b[] = {r, uint8_t(value >> 8), uint8_t(value)};
    return i2c_master_transmit(device_, b, 3, 30) == ESP_OK;
  }
};
struct InaWorker {
  std::shared_ptr<MeterDriver> meter;
  I2c io;
  ina228::Reader reader;
  InaWorker(std::shared_ptr<MeterDriver> m, const Json &j)
      : meter(std::move(m)), io(j),
        reader(io, {j.item("ina228").number("shunt_ohms"),
                    j.item("ina228").number("max_current_a"),
                    unsigned(j.item("ina228").number("adc_range"))}) {}
  static void run(void *context) {
    auto self = static_cast<InaWorker *>(context);
    bool initialized = false, ever = false;
    for (;;) {
      if (!initialized) {
        initialized = self->reader.initialize();
        if (initialized && ever)
          self->meter->advance_epoch();
        ever |= initialized;
      }
      if (initialized) {
        auto sample = self->reader.sample();
        if (sample.status == ina228::Status::valid)
          self->meter->publish(std::move(sample.values), now_ms());
        else if (sample.status == ina228::Status::fault) {
          self->meter->invalidate();
          initialized = false;
        }
      } else
        self->meter->invalidate();
      sleep_ms(initialized ? 100 : 1000);
    }
  }
};
struct HttpWorker {
  std::shared_ptr<MeterDriver> meter;
  std::string url, serial;
  uint64_t uptime = 0;
  bool seen = false;
  static void run(void *context) {
    auto self = static_cast<HttpWorker *>(context);
    for (;;) {
      bool ok = false;
      esp_http_client_config_t config = {};
      config.url = self->url.c_str();
      config.timeout_ms = 1000;
      config.disable_auto_redirect = true;
      auto client = esp_http_client_init(&config);
      if (client) {
        if (esp_http_client_open(client, 0) == ESP_OK) {
          auto length = esp_http_client_fetch_headers(client);
          if (esp_http_client_get_status_code(client) == 200 &&
              length <= 16384) {
            std::string body;
            char buffer[512];
            while (body.size() <= 16384) {
              int n = esp_http_client_read(client, buffer, sizeof(buffer));
              if (n < 0) {
                body.clear();
                break;
              }
              if (n == 0)
                break;
              body.append(buffer, n);
            }
            if (body.size() <= 16384 &&
                esp_http_client_is_complete_data_received(client))
              try {
                auto api = Json::parse(body);
                auto values = yocto_watt::decode(api, self->serial);
                auto module = api.item("module");
                if (!module.has("upTime"))
                  throw std::invalid_argument("Missing Yocto uptime");
                auto uptime = uint64_t(module.number("upTime"));
                if (self->seen && uptime < self->uptime)
                  self->meter->advance_epoch();
                self->uptime = uptime;
                self->seen = true;
                self->meter->publish(std::move(values), now_ms());
                ok = true;
              } catch (const std::exception &) {
              }
          }
        }
        esp_http_client_cleanup(client);
      }
      if (!ok)
        self->meter->invalidate();
      sleep_ms(500);
    }
  }
};
Json sensor_limits(const Json &j) {
  auto l = j.item("limits");
  l.set("max_age_ms",
        j.number("sensor_max_age_ms", l.number("max_age_ms", 1000)));
  return l;
}
} // namespace
bool valid_hub_url(const std::string &url) {
  if (url.rfind("http://", 0) != 0 || url.size() > 256 || url.size() <= 7)
    return false;
  auto authority = url.substr(7);
  return authority.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNO"
                                     "PQRSTUVWXYZ0123456789.-:") ==
             std::string::npos &&
         authority.front() != ':' && authority.back() != ':';
}
std::shared_ptr<Driver> ina228_driver(const Json &j) {
  auto m = std::make_shared<MeterDriver>(j.text("id"), "ina228", "i2c",
                                         j.text("endpoint"), sensor_limits(j),
                                         ina228::channels());
  auto worker = new InaWorker(m, j);
  if (xTaskCreate(InaWorker::run, "ina228", 4096, worker, 3, nullptr) != pdPASS)
    throw std::runtime_error("INA228 worker allocation failed");
  return m;
}
std::shared_ptr<Driver> yocto_watt_driver(const Json &j) {
  auto hub = j.item("yocto_watt").text("hub_url");
  if (!valid_hub_url(hub))
    throw std::invalid_argument("Invalid hub URL");
  auto m = std::make_shared<MeterDriver>(j.text("id"), "yocto_watt", "http",
                                         j.text("endpoint"), sensor_limits(j),
                                         yocto_watt::channels());
  auto worker =
      new HttpWorker{m, hub + "/bySerial/" + j.text("endpoint") + "/api.json",
                     j.text("endpoint")};
  if (xTaskCreate(HttpWorker::run, "yocto_watt", 8192, worker, 3, nullptr) !=
      pdPASS)
    throw std::runtime_error("Yocto worker allocation failed");
  return m;
}
} // namespace labbridge::esp

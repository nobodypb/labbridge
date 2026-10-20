#include "driver/gpio.h"
#include "driver/uart.h"
#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "labbridge/interlock.hpp"
#include "onewire_bus.h"
#include "platform.hpp"
#include <atomic>
#include <cmath>
namespace labbridge::esp {
namespace {
Json limits(const Json &config) { return config.item("limits"); }
Json progress() {
  Json j;
  j.set("status", "running")
      .set("stage", "feedback")
      .set("read_attempts", 0.)
      .set("write_attempts", 0.)
      .set("reconnects", 0.)
      .set("duration_ms", 0.)
      .set("final_output", "unknown")
      .set("restore_pending", false);
  return j;
}
class Pins : public SwitchIo {
  std::vector<int> pins_;
  bool high_, feedback_high_;

public:
  Pins(const std::vector<int> &pins, bool high, bool feedback)
      : pins_(pins), high_(high), feedback_high_(feedback) {
    if (pins.size() != 4)
      throw std::invalid_argument("Interlock pins: charge, discharge, charge "
                                  "feedback, discharge feedback");
    for (int i = 0; i < 2; ++i) {
      gpio_set_level(gpio_num_t(pins[i]), !high_);
      ESP_ERROR_CHECK(
          gpio_set_direction(gpio_num_t(pins[i]), GPIO_MODE_OUTPUT));
    }
    for (int i = 2; i < 4; ++i) {
      ESP_ERROR_CHECK(gpio_set_direction(gpio_num_t(pins[i]), GPIO_MODE_INPUT));
      ESP_ERROR_CHECK(gpio_set_pull_mode(gpio_num_t(pins[i]),
                                         feedback_high_ ? GPIO_PULLDOWN_ONLY
                                                        : GPIO_PULLUP_ONLY));
    }
  }
  void drive(bool c, bool d) override {
    gpio_set_level(gpio_num_t(pins_[0]), c ? high_ : !high_);
    gpio_set_level(gpio_num_t(pins_[1]), d ? high_ : !high_);
  }
  bool charge_closed() const override {
    return bool(gpio_get_level(gpio_num_t(pins_[2]))) == feedback_high_;
  }
  bool discharge_closed() const override {
    return bool(gpio_get_level(gpio_num_t(pins_[3]))) == feedback_high_;
  }
};
class Switch : public Driver {
  Json config_, result_ = progress();
  Pins pins_;
  Interlock sw_;
  bool pending_ = true;
  uint64_t started_ = 0;

public:
  Switch(Json config, std::vector<int> pins)
      : config_(std::move(config)),
        pins_(pins, config_.item("interlock").boolean("active_high"),
              config_.item("interlock").boolean("feedback_active_high")),
        sw_(pins_, uint32_t(config_.item("interlock").number("break_ms", 20))) {
    sw_.stop(now_ms());
  }
  Json describe(uint64_t) {
    Json j;
    auto caps = Json::array();
    for (auto c : {"output", "switch.path", "switch.feedback"})
      caps.append(std::string(c));
    j.set("id", config_.text("id"))
        .set("driver", "gpio_interlock")
        .set("identity", config_.text("endpoint"))
        .set("transport", "gpio")
        .set("capabilities", caps)
        .set("state", sw_.faulted() ? "fault"
                      : pending_    ? "busy"
                                    : "ready")
        .set("output", !sw_.confirmed()               ? "unknown"
                       : sw_.path() == Path::isolated ? "off"
                                                      : "on")
        .set("limits", limits(config_));
    return j;
  }
  Json measurements(uint64_t now) {
    Json j;
    j.set("device_id", config_.text("id"))
        .set("sequence", 0.)
        .set("received_ms", double(now))
        .set("age_ms", 0.)
        .set("quality", sw_.confirmed() ? "valid" : "unavailable")
        .set("temperature_channels", Json::array());
    return j;
  }
  bool supports(const std::string &op) const { return op == "setSwitchPath"; }
  void tick(uint64_t now) {
    sw_.tick(now);
    if (pending_ && (sw_.confirmed() || sw_.faulted())) {
      pending_ = false;
      result_.set("status", sw_.faulted() ? "failed" : "succeeded")
          .set("stage", sw_.faulted() ? "feedback_fault" : "confirmed")
          .set("duration_ms", double(now - started_));
    }
  }
  void stop(uint64_t now) {
    sw_.stop(now);
    pending_ = true;
    started_ = now;
    result_ = progress();
  }
  Json operation(uint64_t now) {
    auto j = result_;
    j.set("final_output", describe(now).text("output"));
    return j;
  }
  Json execute(const std::string &op, const Json &body, uint64_t now) {
    if (op != "setSwitchPath")
      throw std::invalid_argument("Unsupported switch operation");
    auto p = body.text("path");
    Path path = p == "charge"      ? Path::charge
                : p == "discharge" ? Path::discharge
                                   : Path::isolated;
    if (p != "isolated" && p != "charge" && p != "discharge")
      throw std::invalid_argument("Invalid switch path");
    if (path == Path::isolated)
      stop(now);
    else {
      if (!sw_.select(path, now))
        throw std::invalid_argument(
            "Interlock fault latched; inspect contacts before restart");
      pending_ = true;
      started_ = now;
      result_ = progress();
    }
    return operation(now);
  }
};
class Probe : public Driver {
  Json config_;
  int pin_;
  uint64_t rom_;
  std::mutex data_;
  double value_ = NAN;
  uint64_t received_ = 0, sequence_ = 0;
  static void worker(void *ctx) {
    auto self = static_cast<Probe *>(ctx);
    onewire_bus_handle_t bus = nullptr;
    onewire_bus_config_t cfg = {};
    cfg.bus_gpio_num = self->pin_;
    cfg.flags.en_pull_up = true;
    onewire_bus_rmt_config_t rmt = {};
    rmt.max_rx_bytes = 10;
    if (onewire_new_bus_rmt(&cfg, &rmt, &bus) != ESP_OK) {
      vTaskDelete(nullptr);
      return;
    }
    ds18b20_device_handle_t device = nullptr;
    for (;;) {
      if (!device) {
        onewire_device_iter_handle_t iter = nullptr;
        if (onewire_new_device_iter(bus, &iter) == ESP_OK) {
          onewire_device_t found = {};
          while (onewire_device_iter_get_next(iter, &found) == ESP_OK)
            if (found.address == self->rom_) {
              ds18b20_config_t dc = {};
              ds18b20_new_device_from_enumeration(&found, &dc, &device);
              break;
            }
          onewire_del_device_iter(iter);
        }
      }
      float temp = 0;
      bool ok = device &&
                ds18b20_trigger_temperature_conversion(device) == ESP_OK &&
                ds18b20_get_temperature(device, &temp) == ESP_OK &&
                std::isfinite(temp) && temp >= -55 && temp <= 125;
      {
        std::lock_guard<std::mutex> l(self->data_);
        if (ok) {
          self->value_ = temp;
          self->received_ = now_ms();
          ++self->sequence_;
        } else
          self->value_ = NAN;
      }
      if (!ok && device) {
        ds18b20_del_device(device);
        device = nullptr;
      }
      sleep_ms(200);
    }
  }

public:
  Probe(Json config, int pin) : config_(std::move(config)), pin_(pin) {
    auto s = config_.text("endpoint");
    if (s.size() != 16 ||
        s.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
      throw std::invalid_argument(
          "DS18B20 endpoint must be its 16-hex-digit ROM");
    rom_ = std::stoull(s, nullptr, 16);
    xTaskCreate(worker, "cell_probe", 4096, this, 3, nullptr);
  }
  Json measurements(uint64_t now) {
    std::lock_guard<std::mutex> l(data_);
    bool valid =
        std::isfinite(value_) && received_ <= now && now - received_ <= 3000;
    Json j, t;
    auto a = Json::array();
    t.set("id", "cell")
        .set("kind", "cell")
        .set("quality", valid ? "valid" : "unavailable")
        .set("received_ms", double(received_))
        .set("age_ms", double(now - received_));
    if (std::isfinite(value_))
      t.set("value_c", value_);
    a.append(t);
    j.set("device_id", config_.text("id"))
        .set("sequence", double(sequence_))
        .set("received_ms", double(received_))
        .set("age_ms", double(now - received_))
        .set("quality", valid ? "valid" : "unavailable")
        .set("temperature_channels", a);
    return j;
  }
  Json describe(uint64_t now) {
    auto m = measurements(now);
    Json j;
    auto a = Json::array();
    a.append(std::string("temperature.external"));
    j.set("id", config_.text("id"))
        .set("driver", "ds18b20")
        .set("transport", "onewire")
        .set("identity", config_.text("endpoint"))
        .set("capabilities", a)
        .set("state", m.text("quality") == "valid" ? "ready" : "unavailable")
        .set("output", "off");
    if (config_.has("limits"))
      j.set("limits", limits(config_));
    return j;
  }
  bool supports(const std::string &op) const {
    return op == "getTemperatureChannels";
  }
  void tick(uint64_t) {}
  void stop(uint64_t) {}
  Json execute(const std::string &op, const Json &, uint64_t now) {
    if (op != "getTemperatureChannels")
      throw std::invalid_argument("Read-only probe");
    Json j;
    j.set("channels", measurements(now).item("temperature_channels"));
    return j;
  }
};
class Uart : public Endpoint {
  uart_port_t port_ = UART_NUM_1;

public:
  explicit Uart(const DeviceSpec &spec) {
    if (spec.pins.size() != 2)
      throw std::invalid_argument("UART pins must specify TX,RX");
    uart_config_t c = {};
    c.baud_rate = 115200;
    c.data_bits = UART_DATA_8_BITS;
    c.parity = UART_PARITY_DISABLE;
    c.stop_bits = UART_STOP_BITS_1;
    c.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    c.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(port_, &c));
    ESP_ERROR_CHECK(uart_set_pin(port_, spec.pins[0], spec.pins[1],
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(port_, 2048, 2048, 0, nullptr, 0));
  }
  bool connected() const { return true; }
  uint64_t generation() const { return 1; }
  bool send(const Bytes &b) {
    return uart_tx_chars(port_, reinterpret_cast<const char *>(b.data()),
                         b.size()) == int(b.size());
  }
  Bytes receive() {
    uint8_t b[256];
    int n = uart_read_bytes(port_, b, sizeof(b), 0);
    return n > 0 ? Bytes(b, b + n) : Bytes{};
  }
};
} // namespace
std::vector<int> pins(const Json &j) {
  std::vector<int> result;
  auto a = j.at("pins");
  if (cJSON_IsArray(a))
    for (auto p = a->child; p; p = p->next)
      result.push_back(p->valueint);
  return result;
}
std::shared_ptr<Driver> switch_driver(const Json &j) {
  return std::make_shared<Switch>(j, pins(j));
}
std::shared_ptr<Driver> probe_driver(const Json &j) {
  auto p = pins(j);
  if (p.size() != 1)
    throw std::invalid_argument("DS18B20 requires one data GPIO");
  return std::make_shared<Probe>(j, p[0]);
}
std::shared_ptr<Endpoint> uart_endpoint(const DeviceSpec &spec) {
  return std::make_shared<Uart>(spec);
}
} // namespace labbridge::esp

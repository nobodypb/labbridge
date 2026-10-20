#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform.hpp"
#include "rd60xx.hpp"
#include <atomic>
#include <deque>
#if CONFIG_IDF_TARGET_ESP32S3
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"
static uint8_t opening_usb_address =
    0; // Only written by the single USB connection task.
extern "C" esp_err_t
__wrap_cdc_acm_host_open_v1_dispatch(uint16_t vid, uint16_t pid, uint8_t iface,
                                     const cdc_acm_host_device_config_t *d,
                                     cdc_acm_dev_hdl_t *out) {
  cdc_acm_host_open_config_t c = {};
  c.vid = vid;
  c.pid = pid;
  c.interface_idx = iface;
  c.dev_addr = opening_usb_address;
  c.connection_timeout_ms = d->connection_timeout_ms;
  c.out_buffer_size = d->out_buffer_size;
  c.in_buffer_size = d->in_buffer_size;
  c.event_cb = d->event_cb;
  c.data_cb = d->data_cb;
  c.user_arg = d->user_arg;
  return cdc_acm_host_open_v2(&c, out);
}

namespace labbridge::esp {
class UsbLink : public Endpoint {
public:
  std::string serial;
  std::atomic<bool> online{false}, busy{false};
  std::atomic<uint64_t> epoch{1};
  uint8_t address = 0;
  std::mutex io, device_mutex;
  Bytes rx;
  std::deque<Bytes> tx;
  std::unique_ptr<CdcAcmDevice> device;
  explicit UsbLink(std::string s) : serial(std::move(s)) {}
  bool connected() const override { return online; }
  bool writable() const override { return online && !busy; }
  uint64_t generation() const override { return epoch; }
  void append(const uint8_t *p, size_t n) {
    std::lock_guard<std::mutex> l(io);
    if (rx.size() + n > 4096) {
      rx.clear();
      online = false;
      return;
    }
    rx.insert(rx.end(), p, p + n);
  }
  void cancel_pending() override {
    std::lock_guard<std::mutex> lock(io);
    if (!tx.empty()) {
      tx.clear();
      busy = false;
    }
  }
  Bytes receive() override {
    std::lock_guard<std::mutex> l(io);
    Bytes b;
    b.swap(rx);
    return b;
  }
  bool send(const Bytes &b) override {
    std::lock_guard<std::mutex> l(io);
    if (!online || busy)
      return false;
    busy = true;
    tx.push_back(b);
    return true;
  }
  static void worker(void *ctx) {
    auto self = static_cast<UsbLink *>(ctx);
    for (;;) {
      Bytes b;
      {
        std::lock_guard<std::mutex> l(self->io);
        if (!self->tx.empty()) {
          b = std::move(self->tx.front());
          self->tx.pop_front();
        }
      }
      if (!b.empty()) {
        std::lock_guard<std::mutex> l(self->device_mutex);
        if (!self->online || !self->device ||
            self->device->tx_blocking(b.data(), b.size(), 400) != ESP_OK)
          self->online = false;
        self->busy = false;
      }
      sleep_ms(5);
    }
  }
};
static std::vector<std::shared_ptr<UsbLink>> usb;
#if CONFIG_LAB_BOARD_USB_OTG
static std::mutex usb_registry_mutex;
static std::map<uint8_t, std::pair<uint16_t, uint16_t>> usb_registry;
static void usb_new(usb_device_handle_t dev) {
  const usb_device_desc_t *desc;
  usb_device_info_t info;
  if (usb_host_get_device_descriptor(dev, &desc) != ESP_OK ||
      usb_host_device_info(dev, &info) != ESP_OK)
    return;
  std::lock_guard<std::mutex> g(usb_registry_mutex);
  usb_registry[info.dev_addr] = {desc->idVendor, desc->idProduct};
}
static bool usb_rx(const uint8_t *p, size_t n, void *ctx) {
  static_cast<UsbLink *>(ctx)->append(p, n);
  return true;
}
static void usb_event(const cdc_acm_host_dev_event_data_t *e, void *ctx) {
  if (e->type == CDC_ACM_HOST_DEVICE_DISCONNECTED ||
      e->type == CDC_ACM_HOST_ERROR)
    static_cast<UsbLink *>(ctx)->online = false;
}
static void usb_events(void *) {
  for (;;) {
    uint32_t f;
    usb_host_lib_handle_events(portMAX_DELAY, &f);
    if (f & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
  }
}
static void usb_manager(void *) {
  for (;;) {
    for (auto &link : usb) {
      if (link->online)
        continue;
      std::lock_guard<std::mutex> lock(link->device_mutex);
      link->device.reset();
      link->address = 0;
      link->receive();
      cdc_acm_host_device_config_t cfg = {};
      cfg.connection_timeout_ms = 100;
      cfg.out_buffer_size = 256;
      cfg.in_buffer_size = 256;
      cfg.event_cb = usb_event;
      cfg.data_cb = usb_rx;
      cfg.user_arg = link.get();
      uint8_t addresses[16];
      int count = 0;
      usb_host_device_addr_list_fill(16, addresses, &count);
      std::map<uint8_t, std::pair<uint16_t, uint16_t>> candidates;
      {
        std::lock_guard<std::mutex> g(usb_registry_mutex);
        candidates = usb_registry;
      }
      for (int n = 0; n < count && !link->online; n++) {
        uint8_t addr = addresses[n];
        bool used = false;
        for (auto &other : usb)
          if (other->address == addr)
            used = true;
        if (used || !candidates.count(addr))
          continue;
        auto [vid, pid] = candidates[addr];
        opening_usb_address = addr;
        try {
          if (vid == 0x1a86 &&
              (pid == 0x7523 || pid == 0x5523 || pid == 0x7522))
            link->device = std::make_unique<esp_usb::CH34x>(pid, &cfg);
          if (vid == 0x10c4 && pid == 0xea60)
            link->device = std::make_unique<esp_usb::CP210x>(pid, &cfg);
          if (vid == 0x0403 && pid == 0x6001)
            link->device = std::make_unique<esp_usb::FT23x>(pid, &cfg);
        } catch (esp_err_t) {
        }
        if (link->device)
          link->address = addr;
        if (link->device) {
          cdc_acm_line_coding_t coding = {115200, 0, 0, 8};
          if (link->device->line_coding_set(&coding) == ESP_OK) {
            link->device->set_control_line_state(true, true);
            auto request = rd60xx::read(1, 0, 20);
            link->device->tx_blocking(request.data(), request.size(), 400);
            Bytes reply;
            auto end = now_ms() + 500;
            while (now_ms() < end && reply.size() < 45) {
              auto b = link->receive();
              reply.insert(reply.end(), b.begin(), b.end());
              sleep_ms(5);
            }
            bool match = false;
            if (rd60xx::valid(reply, 1, 3) && reply.size() == 45) {
              uint32_t serial = (uint32_t(reply[5]) << 24) |
                                (uint32_t(reply[6]) << 16) |
                                (uint32_t(reply[7]) << 8) | reply[8];
              match = std::to_string(serial) == link->serial;
            }
            if (match) {
              ++link->epoch;
              link->online = true;
            } else {
              link->device.reset();
              link->address = 0;
            }
          } else
            link->device.reset();
        }
      }
    }
    sleep_ms(500);
  }
}
#endif
std::shared_ptr<Endpoint> usb_endpoint(const std::string &serial) {
  auto link = std::make_shared<UsbLink>(serial);
  usb.push_back(link);
  xTaskCreate(UsbLink::worker, "usb_tx", 4096, link.get(), 4, nullptr);
  return link;
}
void start_usb() {
#if CONFIG_LAB_BOARD_USB_OTG
  // ESP32-S3-USB-OTG: route USB to host; supply VBUS from USB DEV 5 V, current
  // limited.
  for (auto pin : {GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_12, GPIO_NUM_13})
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_13, 0);
  gpio_set_level(GPIO_NUM_12, 0);
  gpio_set_level(GPIO_NUM_17, 1);
  gpio_set_level(GPIO_NUM_18, 1);
  sleep_ms(10);
  gpio_set_level(GPIO_NUM_12, 1);
  usb_host_config_t cfg = {};
  cfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
  ESP_ERROR_CHECK(usb_host_install(&cfg));
  xTaskCreate(usb_events, "usb_events", 4096, nullptr, 10, nullptr);
  cdc_acm_host_driver_config_t cdc = {};
  cdc.driver_task_stack_size = 4096;
  cdc.driver_task_priority = 5;
  cdc.xCoreID = 0;
  cdc.new_dev_cb = usb_new;
  ESP_ERROR_CHECK(cdc_acm_host_install(&cdc));
  xTaskCreate(usb_manager, "usb_connect", 6144, nullptr, 4, nullptr);
#endif
}
} // namespace labbridge::esp
#else
namespace labbridge::esp {
std::shared_ptr<Endpoint> usb_endpoint(const std::string &) {
  throw std::invalid_argument("USB host unavailable");
}
void start_usb() {}
} // namespace labbridge::esp
#endif

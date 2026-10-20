#include "app.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <map>

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

namespace lab {
class BufferedLink : public Link {
protected:
  mutable std::mutex rx_mutex;
  Bytes rx;

public:
  std::atomic<bool> online{false};
  bool connected() const override { return online; }
  void append(const uint8_t *p, size_t n) {
    std::lock_guard<std::mutex> l(rx_mutex);
    if (rx.size() + n > 4096) {
      rx.clear();
      online = false;
      return;
    }
    rx.insert(rx.end(), p, p + n);
  }
  Bytes receive() override {
    std::lock_guard<std::mutex> l(rx_mutex);
    Bytes out;
    out.swap(rx);
    return out;
  }
};
class UsbLink : public BufferedLink {
public:
  uint8_t address = 0;
  std::mutex device_mutex;
  std::unique_ptr<CdcAcmDevice> device;
  bool send(const Bytes &bytes) override {
    std::lock_guard<std::mutex> l(device_mutex);
    return online && device &&
           device->tx_blocking(const_cast<uint8_t *>(bytes.data()),
                               bytes.size(), 400) == ESP_OK;
  }
};
static std::array<std::shared_ptr<UsbLink>, 2> usb;
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
      for (int n = 0; n < count && !link->device; n++) {
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
      }
      if (link->device) {
        cdc_acm_line_coding_t coding = {115200, 0, 0, 8};
        if (link->device->line_coding_set(&coding) == ESP_OK) {
          link->device->set_control_line_state(true, true);
          link->online = true;
        } else
          link->device.reset();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
#endif
class BleLink : public BufferedLink {
public:
  std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE}, value_handle{0};
  std::atomic<uint8_t> properties{0};
  std::atomic<int> write_status{-1};
  std::atomic<int> last_write_type{0};
  std::mutex write_mutex;
  static int written(uint16_t conn, const ble_gatt_error *error,
                     ble_gatt_attr *, void *ctx) {
    auto self = static_cast<BleLink *>(ctx);
    if (conn == self->connection)
      self->write_status = error->status;
    return 0;
  }
  bool send(const Bytes &b) override {
    std::lock_guard<std::mutex> guard(write_mutex);
    if (!online)
      return false;
    write_status = -1;
    // Prefer an ATT Write Request when available. Its acknowledgment only
    // confirms GATT delivery; device commands still require protocol readback.
    if (properties & BLE_GATT_CHR_PROP_WRITE) {
      last_write_type = 1;
      const uint16_t conn = connection;
      if (ble_gattc_write_flat(conn, value_handle, b.data(), b.size(), written,
                               this) != 0)
        return false;
      // NimBLE accepting the request is not the ATT Write Response. Wait for
      // that response before another command can use FFE1. It still does not
      // acknowledge execution of the ATORCH command itself.
      const auto deadline = now_ms() + 1200;
      while (now_ms() < deadline && online && connection == conn) {
        int status = write_status.load();
        if (status != -1)
          return status == 0;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      // A late response must not be allowed to confirm a later command.
      online = false;
      ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
      return false;
    }
    if (properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP) {
      last_write_type = 2;
      return ble_gattc_write_no_rsp_flat(connection, value_handle, b.data(),
                                         b.size()) == 0;
    }
    return false;
  }
  bool send_without_response(const Bytes &b) override {
    std::lock_guard<std::mutex> guard(write_mutex);
    if (!online || !(properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP))
      return false;
    write_status = -1; // A Write Command has no ATT acknowledgement.
    last_write_type = 2;
    return ble_gattc_write_no_rsp_flat(connection, value_handle, b.data(),
                                       b.size()) == 0;
  }
};
static std::shared_ptr<BleLink> ble;
static std::mutex ble_mutex;
static std::string target;
static std::vector<std::string> seen;
static std::atomic<bool> synced{false};
static std::atomic<uint64_t> connecting_at{0};
static std::atomic<uint64_t> last_notification_at{0};
static std::atomic<uint32_t> connection_count{0}, disconnect_count{0};
static uint16_t service_end = 0;
static uint8_t own_type;
static int gap_event(ble_gap_event *, void *);
static void scan() {
  if (synced && !ble->online && !ble_gap_conn_active() &&
      !ble_gap_disc_active() && ble->connection == BLE_HS_CONN_HANDLE_NONE) {
    ble_gap_disc_params p = {};
    p.passive = 0;
    p.filter_duplicates = 1;
    ble_gap_disc(own_type, 5000, &p, gap_event, nullptr);
  }
}
static std::string address_string(const ble_addr_t &a) {
  char s[18];
  snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", a.val[5], a.val[4],
           a.val[3], a.val[2], a.val[1], a.val[0]);
  return s;
}
static int subscribed(uint16_t conn, const ble_gatt_error *e, ble_gatt_attr *,
                      void *) {
  if (e->status == 0) {
    last_notification_at = now_ms();
    ble->online = true;
  } else
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  return 0;
}
static int descriptor(uint16_t conn, const ble_gatt_error *e, uint16_t,
                      const ble_gatt_dsc *d, void *) {
  if (!e->status && ble_uuid_u16(&d->uuid.u) == 0x2902) {
    uint8_t enable[] = {1, 0};
    ble_gattc_write_flat(conn, d->handle, enable, 2, subscribed, nullptr);
  }
  return 0;
}
static int characteristic(uint16_t conn, const ble_gatt_error *e,
                          const ble_gatt_chr *c, void *) {
  if (!e->status && ble_uuid_u16(&c->uuid.u) == 0xffe1) {
    ble->value_handle = c->val_handle;
    ble->properties = c->properties;
  }
  if (e->status == BLE_HS_EDONE) {
    if (ble->value_handle)
      ble_gattc_disc_all_dscs(conn, ble->value_handle, service_end, descriptor,
                              nullptr);
    else
      ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  }
  return 0;
}
static int service(uint16_t conn, const ble_gatt_error *e,
                   const ble_gatt_svc *s, void *) {
  if (!e->status) {
    service_end = s->end_handle;
    ble_gattc_disc_all_chrs(conn, s->start_handle, s->end_handle,
                            characteristic, nullptr);
  }
  if (e->status == BLE_HS_EDONE && !service_end)
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  return 0;
}
static int gap_event(ble_gap_event *e, void *) {
  switch (e->type) {
  case BLE_GAP_EVENT_DISC: {
    ble_hs_adv_fields fields = {};
    ble_hs_adv_parse_fields(&fields, e->disc.data, e->disc.length_data);
    std::string name =
        fields.name ? std::string((char *)fields.name, fields.name_len) : "";
    auto addr = address_string(e->disc.addr);
    bool match;
    {
      std::lock_guard<std::mutex> l(ble_mutex);
      match = addr == target;
      if (name.find("DL24") != std::string::npos) {
        auto desc = addr + " " + name;
        if (std::find(seen.begin(), seen.end(), desc) == seen.end() &&
            seen.size() < 16)
          seen.push_back(desc);
      }
    }
    if (match) {
      ble_gap_disc_cancel();
      ble_gap_connect(own_type, &e->disc.addr, 5000, nullptr, gap_event,
                      nullptr);
    }
    break;
  }
  case BLE_GAP_EVENT_CONNECT:
    if (!e->connect.status) {
      ble_gap_conn_desc peer = {};
      bool selected = false;
      if (ble_gap_conn_find(e->connect.conn_handle, &peer) == 0) {
        std::lock_guard<std::mutex> lock(ble_mutex);
        selected = address_string(peer.peer_ota_addr) == target;
      }
      if (!selected) {
        ble_gap_terminate(e->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        break;
      }
      ble->connection = e->connect.conn_handle;
      ++connection_count;
      connecting_at = now_ms();
      ble->value_handle = 0;
      service_end = 0;
      ble_uuid16_t uuid = BLE_UUID16_INIT(0xffe0);
      ble_gattc_disc_svc_by_uuid(ble->connection, &uuid.u, service, nullptr);
    }
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ++disconnect_count;
    ble->online = false;
    ble->write_status = -2;
    last_notification_at = 0;
    ble->connection = BLE_HS_CONN_HANDLE_NONE;
    ble->value_handle = 0;
    ble->receive();
    break;
  case BLE_GAP_EVENT_NOTIFY_RX: {
    if (e->notify_rx.attr_handle != ble->value_handle)
      break;
    uint16_t n = OS_MBUF_PKTLEN(e->notify_rx.om);
    if (n <= 512) {
      uint8_t b[512];
      os_mbuf_copydata(e->notify_rx.om, 0, n, b);
      last_notification_at = now_ms();
      ble->append(b, n);
    }
    break;
  }
  default:
    break;
  }
  return 0;
}
static void on_sync() {
  ble_hs_util_ensure_addr(0);
  ble_hs_id_infer_auto(0, &own_type);
  synced = true;
  scan();
}
static void ble_host(void *) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}
static void scan_task(void *) {
  for (;;) {
    if (ble->connection != BLE_HS_CONN_HANDLE_NONE && !ble->online &&
        now_ms() - connecting_at > 5000)
      ble_gap_terminate(ble->connection, BLE_ERR_REM_USER_CONN_TERM);
    // Some modules remain connected after notifications stop. A GATT link
    // without fresh reports cannot confirm any later keypress or load state.
    if (ble->connection != BLE_HS_CONN_HANDLE_NONE && ble->online &&
        last_notification_at && now_ms() - last_notification_at > 7000)
      ble_gap_terminate(ble->connection, BLE_ERR_REM_USER_CONN_TERM);
    scan();
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
void select_ble(const std::string &address) {
  {
    std::lock_guard<std::mutex> l(ble_mutex);
    target = address;
    std::transform(target.begin(), target.end(), target.begin(), ::tolower);
  }
  if (ble && ble->connection != BLE_HS_CONN_HANDLE_NONE)
    ble_gap_terminate(ble->connection, BLE_ERR_REM_USER_CONN_TERM);
}
std::vector<std::string> discovered_ble() {
  std::lock_guard<std::mutex> l(ble_mutex);
  return seen;
}
cJSON *ble_diagnostics() {
  auto j = cJSON_CreateObject();
  cJSON_AddNumberToObject(j, "connection_count", connection_count.load());
  cJSON_AddNumberToObject(j, "disconnect_count", disconnect_count.load());
  cJSON_AddBoolToObject(j, "connected", ble && ble->online);
  cJSON_AddNumberToObject(j, "ffe1_handle", ble ? ble->value_handle.load() : 0);
  cJSON_AddNumberToObject(j, "ffe1_properties",
                          ble ? ble->properties.load() : 0);
  cJSON_AddStringToObject(j, "write_type",
                          ble && ble->last_write_type == 2
                              ? "without_response"
                              : ble && ble->last_write_type == 1
                                    ? "request"
                                    : "none");
  cJSON_AddNumberToObject(j, "last_att_write_status",
                          ble ? ble->write_status.load() : -1);
  return j;
}
std::shared_ptr<Link> ble_link() { return ble; }
std::shared_ptr<Link> spp_link() { return {}; }
void select_spp(const std::string &) {}
std::vector<std::string> discovered_spp() { return {}; }
cJSON *spp_diagnostics() { return cJSON_CreateObject(); }
std::array<std::shared_ptr<Link>, 2> usb_links() { return {usb[0], usb[1]}; }
void start_transports() {
  ble = std::make_shared<BleLink>();
  for (auto &u : usb)
    u = std::make_shared<UsbLink>();
#if CONFIG_LAB_BOARD_USB_OTG
  // ESP32-S3-USB-OTG: route USB to host; supply VBUS from USB DEV 5 V, current
  // limited.
  for (auto pin : {GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_12, GPIO_NUM_13})
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_13, 0);
  gpio_set_level(GPIO_NUM_12, 0);
  gpio_set_level(GPIO_NUM_17, 1);
  gpio_set_level(GPIO_NUM_18, 1);
  vTaskDelay(pdMS_TO_TICKS(10));
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
#else
  ESP_LOGI("board",
           "StickS3: BLE enabled, USB retained for Serial/JTAG console");
#endif
  ESP_ERROR_CHECK(nimble_port_init());
  ble_hs_cfg.sync_cb = on_sync;
  nimble_port_freertos_init(ble_host);
  xTaskCreate(scan_task, "ble_scan", 3072, nullptr, 3, nullptr);
}
} // namespace lab

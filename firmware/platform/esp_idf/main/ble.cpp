#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "platform.hpp"
#include <algorithm>
#include <atomic>
#include <deque>
namespace labbridge::esp {
class Ble : public Endpoint {
public:
  std::string address;
  std::atomic<bool> online{false}, busy{false};
  std::atomic<uint64_t> epoch{1};
  std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE};
  uint16_t value_handle = 0, service_end = 0;
  uint8_t properties = 0;
  std::atomic<int> ack{-1};
  uint64_t connecting_at = 0;
  std::mutex io;
  Bytes rx;
  std::deque<Bytes> tx;
  explicit Ble(std::string a) : address(std::move(a)) {}
  bool connected() const override { return online; }
  bool writable() const override { return online && !busy; }
  uint64_t generation() const override { return epoch; }
  bool send(const Bytes &b) override {
    std::lock_guard<std::mutex> l(io);
    if (!online || busy || b.size() > 256)
      return false;
    busy = true;
    tx.push_back(b);
    return true;
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
  void append(const uint8_t *p, size_t n) {
    std::lock_guard<std::mutex> l(io);
    if (rx.size() + n > 4096) {
      online = false;
      ble_gap_terminate(connection, BLE_ERR_REM_USER_CONN_TERM);
      return;
    }
    rx.insert(rx.end(), p, p + n);
  }
  static int written(uint16_t conn, const ble_gatt_error *e, ble_gatt_attr *,
                     void *ctx) {
    auto self = static_cast<Ble *>(ctx);
    if (conn == self->connection)
      self->ack = e->status;
    return 0;
  }
  static void worker(void *ctx) {
    auto self = static_cast<Ble *>(ctx);
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
        auto conn = self->connection.load();
        auto epoch = self->epoch.load();
        bool ok = false;
        self->ack = -1;
        if (self->online && (self->properties & BLE_GATT_CHR_PROP_WRITE)) {
          if (ble_gattc_write_flat(conn, self->value_handle, b.data(), b.size(),
                                   written, self) == 0) {
            auto end = now_ms() + 1200;
            while (now_ms() < end && self->online && self->epoch == epoch &&
                   self->ack < 0)
              sleep_ms(5);
            ok = self->ack == 0 && self->epoch == epoch;
          }
        } else if (self->online &&
                   (self->properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP))
          ok = ble_gattc_write_no_rsp_flat(conn, self->value_handle, b.data(),
                                           b.size()) == 0;
        if (!ok && self->connection != BLE_HS_CONN_HANDLE_NONE) {
          self->online = false;
          ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        self->busy = false;
      }
      sleep_ms(5);
    }
  }
};
static std::vector<std::shared_ptr<Ble>> endpoints;
static std::mutex registry_mutex;
static uint8_t own_type;
static std::atomic<bool> synced{false};
static std::string address_string(const ble_addr_t &a) {
  char s[18];
  snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", a.val[5], a.val[4],
           a.val[3], a.val[2], a.val[1], a.val[0]);
  return s;
}
static int subscribed(uint16_t conn, const ble_gatt_error *e, ble_gatt_attr *,
                      void *ctx) {
  auto self = static_cast<Ble *>(ctx);
  if (!e->status)
    self->online = true;
  else
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  return 0;
}
static int descriptor(uint16_t conn, const ble_gatt_error *e, uint16_t,
                      const ble_gatt_dsc *d, void *ctx) {
  if (!e->status && ble_uuid_u16(&d->uuid.u) == 0x2902) {
    uint8_t on[] = {1, 0};
    ble_gattc_write_flat(conn, d->handle, on, 2, subscribed, ctx);
  }
  return 0;
}
static int characteristic(uint16_t conn, const ble_gatt_error *e,
                          const ble_gatt_chr *c, void *ctx) {
  auto self = static_cast<Ble *>(ctx);
  if (!e->status && ble_uuid_u16(&c->uuid.u) == 0xffe1) {
    self->value_handle = c->val_handle;
    self->properties = c->properties;
  }
  if (e->status == BLE_HS_EDONE) {
    if (self->value_handle)
      ble_gattc_disc_all_dscs(conn, self->value_handle, self->service_end,
                              descriptor, ctx);
    else
      ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  }
  return 0;
}
static int service(uint16_t conn, const ble_gatt_error *e,
                   const ble_gatt_svc *s, void *ctx) {
  auto self = static_cast<Ble *>(ctx);
  if (!e->status) {
    self->service_end = s->end_handle;
    ble_gattc_disc_all_chrs(conn, s->start_handle, s->end_handle,
                            characteristic, ctx);
  }
  if (e->status == BLE_HS_EDONE && !self->service_end)
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
  return 0;
}
static int gap(ble_gap_event *e, void *ctx) {
  auto self = static_cast<Ble *>(ctx);
  switch (e->type) {
  case BLE_GAP_EVENT_DISC: {
    auto addr = address_string(e->disc.addr);
    std::lock_guard<std::mutex> l(registry_mutex);
    for (auto &p : endpoints)
      if (p->address == addr && p->connection == BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_disc_cancel();
        p->connecting_at = now_ms();
        ble_gap_connect(own_type, &e->disc.addr, 5000, nullptr, gap, p.get());
        break;
      }
    break;
  }
  case BLE_GAP_EVENT_CONNECT:
    if (self && !e->connect.status) {
      self->connection = e->connect.conn_handle;
      ++self->epoch;
      self->value_handle = self->service_end = 0;
      self->connecting_at = now_ms();
      ble_uuid16_t uuid = BLE_UUID16_INIT(0xffe0);
      ble_gattc_disc_svc_by_uuid(self->connection, &uuid.u, service, self);
    }
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    if (self) {
      self->online = false;
      self->connection = BLE_HS_CONN_HANDLE_NONE;
      ++self->epoch;
      self->ack = -2;
      self->receive();
    }
    break;
  case BLE_GAP_EVENT_NOTIFY_RX:
    if (self && e->notify_rx.attr_handle == self->value_handle) {
      uint16_t n = OS_MBUF_PKTLEN(e->notify_rx.om);
      if (n <= 512) {
        uint8_t b[512];
        os_mbuf_copydata(e->notify_rx.om, 0, n, b);
        self->append(b, n);
      }
    }
    break;
  default:
    break;
  }
  return 0;
}
static void sync() {
  ble_hs_util_ensure_addr(0);
  ble_hs_id_infer_auto(0, &own_type);
  synced = true;
}
static void host(void *) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}
static void scan(void *) {
  for (;;) {
    bool missing = false;
    {
      std::lock_guard<std::mutex> l(registry_mutex);
      for (auto &p : endpoints) {
        if (!p->online)
          missing = true;
        if (p->connection != BLE_HS_CONN_HANDLE_NONE && !p->online &&
            now_ms() - p->connecting_at > 5000)
          ble_gap_terminate(p->connection, BLE_ERR_REM_USER_CONN_TERM);
      }
    }
    if (synced && missing && !ble_gap_conn_active() && !ble_gap_disc_active()) {
      ble_gap_disc_params p = {};
      p.filter_duplicates = 1;
      ble_gap_disc(own_type, 2000, &p, gap, nullptr);
    }
    sleep_ms(500);
  }
}
std::shared_ptr<Endpoint> ble_endpoint(const std::string &address) {
  std::string a = address;
  std::transform(a.begin(), a.end(), a.begin(), ::tolower);
  if (a.size() != 17)
    throw std::invalid_argument("Expected BLE MAC address");
  auto p = std::make_shared<Ble>(a);
  {
    std::lock_guard<std::mutex> l(registry_mutex);
    endpoints.push_back(p);
  }
  xTaskCreate(Ble::worker, "ble_tx", 4096, p.get(), 4, nullptr);
  return p;
}
void start_ble() {
  ESP_ERROR_CHECK(nimble_port_init());
  ble_hs_cfg.sync_cb = sync;
  nimble_port_freertos_init(host);
  xTaskCreate(scan, "ble_scan", 4096, nullptr, 3, nullptr);
}
} // namespace labbridge::esp

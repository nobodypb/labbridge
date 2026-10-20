#include "app.hpp"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace lab {
namespace {
class SppLink final : public Link {
public:
  std::atomic<uint32_t> handle{0};
  std::atomic<bool> congested{false};
  std::atomic<bool> disconnecting{false};
  std::atomic<bool> write_pending{false};
  std::atomic<int> last_write_status{-1};
  std::mutex tx_mutex;
  std::mutex rx_mutex;
  Bytes rx;
  bool connected() const override { return handle != 0; }
  bool send(const Bytes &bytes) override {
    std::lock_guard<std::mutex> tx(tx_mutex);
    auto h = handle.load();
    if (!h || congested || bytes.empty() || bytes.size() > 512)
      return false;
    last_write_status = -1;
    write_pending = true;
    if (esp_spp_write(h, bytes.size(),
                      const_cast<uint8_t *>(bytes.data())) != ESP_OK) {
      write_pending = false;
      return false;
    }
    const auto deadline = now_ms() + 700;
    while (write_pending && connected() && now_ms() < deadline)
      vTaskDelay(pdMS_TO_TICKS(10));
    return !write_pending && connected() &&
           last_write_status == ESP_SPP_SUCCESS;
  }
  Bytes receive() override {
    std::lock_guard<std::mutex> lock(rx_mutex);
    Bytes result;
    result.swap(rx);
    return result;
  }
  void append(const uint8_t *bytes, size_t size) {
    std::lock_guard<std::mutex> lock(rx_mutex);
    if (rx.size() + size > 4096) {
      rx.clear();
      overflow = true;
      return;
    }
    rx.insert(rx.end(), bytes, bytes + size);
    last_rx_ms = now_ms();
  }
  std::atomic<bool> overflow{false};
  std::atomic<uint64_t> last_rx_ms{0};
  std::atomic<uint64_t> connected_at_ms{0};
};
std::shared_ptr<SppLink> spp;
std::mutex state_mutex_spp;
std::string selected;
std::string pairing_pin = "1234";
std::string last_error;
std::vector<std::string> found;
std::atomic<bool> initialized{false}, attempting{false}, inquiring{false};
std::atomic<uint64_t> attempt_started{0};
std::atomic<uint64_t> last_close_ms{0};
std::array<uint8_t, 6> active_address{};

std::string address_text(const uint8_t *b) {
  char s[18];
  snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", b[0], b[1], b[2],
           b[3], b[4], b[5]);
  return s;
}
bool parse_address(const std::string &s, std::array<uint8_t, 6> &b) {
  unsigned n[6];
  char extra;
  if (s.size() != 17 ||
      sscanf(s.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x%c", &n[0], &n[1], &n[2],
             &n[3], &n[4], &n[5], &extra) != 6)
    return false;
  for (int i = 0; i < 6; ++i)
    b[i] = n[i];
  return true;
}
bool selected_peer(const uint8_t *address) {
  std::lock_guard<std::mutex> lock(state_mutex_spp);
  return !selected.empty() && address_text(address) == selected;
}
void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *p) {
  if (event == ESP_BT_GAP_DISC_RES_EVT) {
    std::string name;
    for (int i = 0; i < p->disc_res.num_prop; ++i) {
      auto &prop = p->disc_res.prop[i];
      if (prop.type == ESP_BT_GAP_DEV_PROP_BDNAME && prop.len > 0)
        name.assign(static_cast<const char *>(prop.val),
                    std::min(prop.len, ESP_BT_GAP_MAX_BDNAME_LEN));
      if (name.empty() && prop.type == ESP_BT_GAP_DEV_PROP_EIR) {
        uint8_t length = 0;
        auto data = esp_bt_gap_resolve_eir_data(
            static_cast<uint8_t *>(prop.val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME,
            &length);
        if (!data)
          data = esp_bt_gap_resolve_eir_data(
              static_cast<uint8_t *>(prop.val),
              ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &length);
        if (data)
          name.assign(reinterpret_cast<const char *>(data), length);
      }
    }
    if (name.find("DL24") != std::string::npos) {
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      auto description = address_text(p->disc_res.bda) + " " + name;
      if (found.size() < 16 &&
          std::find(found.begin(), found.end(), description) == found.end())
        found.push_back(description);
    }
  } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
    inquiring = p->disc_st_chg.state != ESP_BT_GAP_DISCOVERY_STOPPED;
  } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
    std::string pin;
    {
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      pin = pairing_pin;
    }
    esp_bt_pin_code_t code{};
    memcpy(code, pin.data(), pin.size());
    esp_bt_gap_pin_reply(p->pin_req.bda,
                         selected_peer(p->pin_req.bda) &&
                             !p->pin_req.min_16_digit,
                         pin.size(), code);
  } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
    esp_bt_gap_ssp_confirm_reply(p->cfm_req.bda,
                                 selected_peer(p->cfm_req.bda));
  } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT &&
             p->auth_cmpl.stat != ESP_BT_STATUS_SUCCESS) {
    std::lock_guard<std::mutex> lock(state_mutex_spp);
    last_error = "pairing_failed";
  }
}
void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *p) {
  switch (event) {
  case ESP_SPP_INIT_EVT:
    initialized = p->init.status == ESP_SPP_SUCCESS;
    if (!initialized) {
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      last_error = "spp_init_failed";
    }
    break;
  case ESP_SPP_DISCOVERY_COMP_EVT:
    if (p->disc_comp.status == ESP_SPP_SUCCESS && p->disc_comp.scn_num) {
      if (esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER,
                          p->disc_comp.scn[0], active_address.data()) == ESP_OK)
        break;
    }
    attempting = false;
    {
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      last_error = "spp_service_not_found";
    }
    break;
  case ESP_SPP_OPEN_EVT:
    attempting = false;
    if (p->open.status == ESP_SPP_SUCCESS &&
        selected_peer(p->open.rem_bda)) {
      spp->handle = p->open.handle;
      spp->connected_at_ms = now_ms();
      spp->overflow = false;
      spp->congested = false;
      spp->disconnecting = false;
      spp->write_pending = false;
      spp->receive();
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      last_error.clear();
    } else {
      if (p->open.status == ESP_SPP_SUCCESS)
        esp_spp_disconnect(p->open.handle);
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      last_error = "spp_connection_failed";
    }
    break;
  case ESP_SPP_CLOSE_EVT:
    if (spp->handle == p->close.handle) {
      spp->handle = 0;
      spp->last_rx_ms = 0;
      spp->connected_at_ms = 0;
      spp->write_pending = false;
      spp->disconnecting = false;
      spp->receive();
    }
    attempting = false;
    last_close_ms = now_ms();
    break;
  case ESP_SPP_DATA_IND_EVT:
    if (spp->handle == p->data_ind.handle)
      spp->append(p->data_ind.data, p->data_ind.len);
    break;
  case ESP_SPP_CONG_EVT:
    if (spp->handle == p->cong.handle)
      spp->congested = p->cong.cong;
    break;
  case ESP_SPP_WRITE_EVT:
    if (spp->handle == p->write.handle) {
      spp->last_write_status = p->write.status;
      spp->congested = p->write.cong;
      spp->write_pending = false;
    }
    break;
  default:
    break;
  }
}
void spp_manager(void *) {
  uint64_t last_scan = 0, last_connect = 0;
  for (;;) {
    auto now = now_ms();
    if (spp->connected() && !spp->disconnecting) {
      auto last_data = std::max(spp->last_rx_ms.load(),
                                spp->connected_at_ms.load());
      if (spp->overflow || (last_data && now - last_data > 8000)) {
        {
          std::lock_guard<std::mutex> lock(state_mutex_spp);
          last_error = spp->overflow ? "receive_overflow" : "telemetry_timeout";
        }
        spp->disconnecting = true;
        esp_spp_disconnect(spp->handle);
        spp->overflow = false;
      }
    }
    if (attempting && now - attempt_started > 12000)
      attempting = false;
    std::string address;
    {
      std::lock_guard<std::mutex> lock(state_mutex_spp);
      address = selected;
    }
    if (initialized && !address.empty() && !spp->connected() &&
        !attempting && now - last_connect > 5000 &&
        now - last_close_ms > 5000) {
      if (inquiring)
        esp_bt_gap_cancel_discovery();
      else if (parse_address(address, active_address) &&
               esp_spp_start_discovery(active_address.data()) == ESP_OK) {
        attempting = true;
        attempt_started = now;
        last_connect = now;
      }
    } else if (initialized && address.empty() && !inquiring &&
               now - last_scan > 10000) {
      if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 6, 0) ==
          ESP_OK)
        last_scan = now;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
} // namespace

std::shared_ptr<Link> spp_link() { return spp; }
void select_spp(const std::string &address) {
  {
    std::lock_guard<std::mutex> lock(state_mutex_spp);
    selected = address;
    std::transform(selected.begin(), selected.end(), selected.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    pairing_pin = config.spp_pin;
    last_error.clear();
  }
  if (spp && spp->connected()) {
    spp->disconnecting = true;
    esp_spp_disconnect(spp->handle);
  }
  attempting = false;
}
std::vector<std::string> discovered_spp() {
  std::lock_guard<std::mutex> lock(state_mutex_spp);
  return found;
}
cJSON *spp_diagnostics() {
  auto j = cJSON_CreateObject();
  cJSON_AddBoolToObject(j, "connected", spp && spp->connected());
  cJSON_AddBoolToObject(j, "searching", inquiring);
  cJSON_AddBoolToObject(j, "connecting", attempting);
  cJSON_AddNumberToObject(j, "last_rx_ms", spp ? spp->last_rx_ms.load() : 0);
  cJSON_AddNumberToObject(j, "last_write_status",
                          spp ? spp->last_write_status.load() : -1);
  {
    std::lock_guard<std::mutex> lock(state_mutex_spp);
    cJSON_AddStringToObject(j, "selected_address", selected.c_str());
    cJSON_AddStringToObject(j, "last_error", last_error.c_str());
  }
  return j;
}
// These are intentionally empty on the classic-ESP32 profile. They keep the
// established API shape while the RD USB host and BLE fallback stay on S3.
std::shared_ptr<Link> ble_link() { return {}; }
void select_ble(const std::string &) {}
std::vector<std::string> discovered_ble() { return {}; }
cJSON *ble_diagnostics() { return cJSON_CreateObject(); }
std::array<std::shared_ptr<Link>, 2> usb_links() { return {}; }

void start_transports() {
  spp = std::make_shared<SppLink>();
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
  esp_bt_controller_config_t bt = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&bt));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
  ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));
  ESP_ERROR_CHECK(esp_bt_gap_set_device_name("LabBridge-ESP32"));
  ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,
                                           ESP_BT_NON_DISCOVERABLE));
  esp_spp_cfg_t config = BT_SPP_DEFAULT_CONFIG();
  config.mode = ESP_SPP_MODE_CB;
  ESP_ERROR_CHECK(esp_spp_enhanced_init(&config));
  xTaskCreate(spp_manager, "spp_connect", 4096, nullptr, 4, nullptr);
}
} // namespace lab

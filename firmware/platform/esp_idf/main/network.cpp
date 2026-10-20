#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "labbridge/captive_dns.hpp"
#include "lwip/sockets.h"
#include "mdns.h"
#include "platform.hpp"
#include <atomic>
#include <cstring>
namespace labbridge::esp {
extern const char page_start[] asm("_binary_index_html_start");
extern const char page_end[] asm("_binary_index_html_end");
extern const char js_start[] asm("_binary_app_js_start");
extern const char js_end[] asm("_binary_app_js_end");
static esp_netif_t *sta, *ap;
static std::atomic<bool> connected{false};
static bool setup_enabled = true;
static uint64_t apply_wifi_at = 0;
static std::mutex scan_mutex;
static Json scan_cache;
static std::atomic<bool> scan_requested{false}, scan_running{false};
static std::atomic<uint64_t> scan_finished{0};
static void scan_worker(void *) {
  for (;;) {
    if (scan_requested.exchange(false)) {
      scan_running = true;
      wifi_scan_config_t cfg = {};
      auto err = esp_wifi_scan_start(&cfg, true);
      Json result;
      auto networks = Json::array();
      if (err == ESP_OK) {
        wifi_ap_record_t records[24];
        uint16_t n = 24;
        esp_wifi_scan_get_ap_records(&n, records);
        for (int i = 0; i < n; ++i) {
          Json item;
          item.set("ssid", reinterpret_cast<const char *>(records[i].ssid))
              .set("rssi_dbm", double(records[i].rssi))
              .set("secured", records[i].authmode != WIFI_AUTH_OPEN);
          networks.append(item);
        }
      }
      result.set("networks", networks)
          .set("state", err == ESP_OK ? "complete" : "failed");
      {
        std::lock_guard<std::mutex> lock(scan_mutex);
        scan_cache = result;
      }
      scan_finished = now_ms();
      scan_running = false;
    }
    sleep_ms(100);
  }
}

static Json wifi_state() {
  Json j;
  j.set("state", connected                               ? "connected"
                 : read_nvs("wifi").text("ssid").empty() ? "setup"
                                                         : "connecting")
      .set("hostname", configuration.text("node_id") + ".local");
  if (connected) {
    esp_netif_ip_info_t ip = {};
    esp_netif_get_ip_info(sta, &ip);
    char address[16];
    snprintf(address, sizeof(address), IPSTR, IP2STR(&ip.ip));
    j.set("address", address);
  }
  return j;
}
static void event(void *, esp_event_base_t base, int32_t id, void *) {
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    connected = true;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    connected = false;
    esp_wifi_connect();
  }
}
static void connect_station() {
  auto wifi = read_nvs("wifi");
  if (wifi.text("ssid").empty())
    return;
  wifi_config_t c = {};
  auto ssid = wifi.text("ssid"), psk = wifi.text("psk");
  memcpy(c.sta.ssid, ssid.data(), ssid.size());
  memcpy(c.sta.password, psk.data(), psk.size());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &c));
  esp_wifi_disconnect();
  esp_wifi_connect();
}
Response dispatch(const std::string &method, const std::string &path,
                  const Json &body, const Headers &headers, bool local) {
  std::lock_guard<std::recursive_mutex> l(mutex);
  if (local && method == "GET" && path == "/management/token") {
    Json j;
    j.set("token", read_nvs("auth").text("token"));
    return {200, j};
  }
  return node->request(method, path, body, headers, now_ms());
}
static bool via_ap(httpd_req_t *r) {
  sockaddr_in address = {};
  socklen_t n = sizeof(address);
  if (getsockname(httpd_req_to_sockfd(r),
                  reinterpret_cast<sockaddr *>(&address), &n) != 0)
    return false;
  return address.sin_addr.s_addr == inet_addr("192.168.4.1");
}
static Headers headers(httpd_req_t *r) {
  Headers h;
  for (auto name :
       {"Authorization", "X-Lab-Lease", "Idempotency-Key", "If-Match"}) {
    size_t n = httpd_req_get_hdr_value_len(r, name);
    if (n && n < 256) {
      std::string s(n + 1, '\0');
      if (httpd_req_get_hdr_value_str(r, name, s.data(), s.size()) == ESP_OK) {
        s.resize(n);
        h[name] = s;
      }
    }
  }
  return h;
}
static esp_err_t respond(httpd_req_t *r, Response response) {
  std::string code = std::to_string(response.status) + " ";
  code += response.status < 300 ? "OK" : "Error";
  httpd_resp_set_status(r, code.c_str());
  httpd_resp_set_type(r, response.status >= 400 ? "application/problem+json"
                                                : "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  for (auto &p : response.headers)
    httpd_resp_set_hdr(r, p.first.c_str(), p.second.c_str());
  auto text = response.body.dump();
  return httpd_resp_send(r, text.data(), text.size());
}
static esp_err_t route(httpd_req_t *r) {
  std::string path = r->uri;
  auto query = path.find('?');
  if (query != std::string::npos)
    path.resize(query);
  if (r->method == HTTP_GET && (path == "/" || path == "/wifi" ||
                                path == "/devices" || path == "/app.js")) {
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(r, "Content-Security-Policy",
                       "default-src 'self'; style-src 'self' 'unsafe-inline'; "
                       "script-src 'self'; frame-ancestors 'none'");
    if (path == "/app.js") {
      httpd_resp_set_type(r, "text/javascript");
      return httpd_resp_send(r, js_start, js_end - js_start - 1);
    }
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    return httpd_resp_send(r, page_start, page_end - page_start - 1);
  }
  if (path.rfind("/api/", 0) != 0 && path != "/management/token" && via_ap(r)) {
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/wifi");
    return httpd_resp_send(r, nullptr, 0);
  }
  bool provisioning = setup_enabled && via_ap(r) &&
                      (path == "/api/v1/wifi" || path == "/api/v1/wifi/scan");
  auto h = headers(r);
  if (!provisioning &&
      h["Authorization"] != "Bearer " + read_nvs("auth").text("token"))
    return respond(
        r,
        problem(401, "Bearer token required; retrieve it over physical USB"));
  if (path == "/management/token")
    return respond(r, problem(403, "Token retrieval is USB-only"));
  if (r->content_len > 16384)
    return respond(r, problem(413, "Request too large"));
  try {
    Json body;
    if (r->content_len) {
      std::string input(r->content_len, '\0');
      size_t got = 0;
      while (got < input.size()) {
        int n = httpd_req_recv(r, input.data() + got, input.size() - got);
        if (n <= 0)
          return ESP_FAIL;
        got += n;
      }
      body = Json::parse(input);
    }
    std::string method = r->method == HTTP_GET    ? "GET"
                         : r->method == HTTP_POST ? "POST"
                         : r->method == HTTP_PUT  ? "PUT"
                                                  : "DELETE";
    return respond(r, dispatch(method, path, body, h));
  } catch (const std::exception &e) {
    return respond(r, problem(400, e.what()));
  }
}
static esp_err_t captive(httpd_req_t *r, httpd_err_code_t) {
  if (via_ap(r)) {
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/wifi");
    return httpd_resp_send(r, nullptr, 0);
  }
  return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
}
static void dns(void *) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in bind_addr = {};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(53);
  bind_addr.sin_addr.s_addr = inet_addr("192.168.4.1");
  if (bind(sock, reinterpret_cast<sockaddr *>(&bind_addr), sizeof(bind_addr)) !=
      0) {
    close(sock);
    vTaskDelete(nullptr);
    return;
  }
  for (;;) {
    uint8_t query[512], reply[528], ip[] = {192, 168, 4, 1};
    sockaddr_in peer = {};
    socklen_t len = sizeof(peer);
    int n = recvfrom(sock, query, sizeof(query), 0,
                     reinterpret_cast<sockaddr *>(&peer), &len);
    size_t out = 0;
    if (n > 0 && captive_dns_reply(query, n, ip, reply, sizeof(reply), out))
      sendto(sock, reply, out, 0, reinterpret_cast<sockaddr *>(&peer), len);
  }
}
static void wifi_worker(void *) {
  for (;;) {
    {
      std::lock_guard<std::recursive_mutex> l(mutex);
      if (apply_wifi_at && now_ms() >= apply_wifi_at) {
        apply_wifi_at = 0;
        connect_station();
      }
    }
    sleep_ms(100);
  }
}
void start_network() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  sta = esp_netif_create_default_wifi_sta();
  ap = esp_netif_create_default_wifi_ap();
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, nullptr);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, nullptr);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  wifi_config_t c = {};
  const char *ssid = "LabBridge-Setup";
  memcpy(c.ap.ssid, ssid, strlen(ssid));
  c.ap.ssid_len = strlen(ssid);
  c.ap.max_connection = 4;
  c.ap.authmode = WIFI_AUTH_OPEN;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &c));
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_netif_dhcps_stop(ap);
  esp_netif_dns_info_t info = {};
  info.ip.type = ESP_IPADDR_TYPE_V4;
  info.ip.u_addr.ip4.addr = inet_addr("192.168.4.1");
  ESP_ERROR_CHECK(esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &info));
  uint8_t offer = 1;
  ESP_ERROR_CHECK(esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                                         ESP_NETIF_DOMAIN_NAME_SERVER, &offer,
                                         sizeof(offer)));
  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));
  static char portal_uri[] = "http://192.168.4.1/wifi";
  esp_netif_dhcps_stop(ap);
  esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                         portal_uri, strlen(portal_uri));
  esp_netif_dhcps_start(ap);
  connect_station();
  ESP_ERROR_CHECK(mdns_init());
  mdns_hostname_set(configuration.text("node_id").c_str());
  mdns_instance_name_set("LabBridge");
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
  node->bind("getWifiState", [](const Json &, const Headers &, uint64_t) {
    return Response{200, wifi_state()};
  });
  node->bind("scanWifi", [](const Json &, const Headers &, uint64_t now) {
    if (!node->idle(now))
      return problem(409, "Release instruments before scanning Wi-Fi");
    if (!scan_running && !scan_requested &&
        (!scan_finished || now - scan_finished > 5000))
      scan_requested = true;
    std::lock_guard<std::mutex> lock(scan_mutex);
    auto result = scan_cache;
    if (!result.has("networks"))
      result.set("networks", Json::array());
    if (scan_running || scan_requested)
      result.set("state", "scanning");
    return Response{200, result};
  });
  node->bind("configureWifi", [](const Json &body, const Headers &,
                                 uint64_t now) {
    if (!node->idle(now))
      return problem(409,
                     "Stop and release instruments before network changes");
    auto psk = body.text("psk");
    if (!psk.empty() &&
        (psk.size() < 8 ||
         (psk.size() == 64 && psk.find_first_not_of("0123456789abcdefABCDEF") !=
                                  std::string::npos)))
      return problem(422,
                     "PSK must be empty, 8..63 characters or 64 hex digits");
    write_nvs("wifi", body);
    apply_wifi_at = now + 300;
    return Response{200, wifi_state()};
  });
  httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
  hc.uri_match_fn = httpd_uri_match_wildcard;
  hc.max_uri_handlers = 8;
  hc.stack_size = 12288;
  httpd_handle_t server;
  ESP_ERROR_CHECK(httpd_start(&server, &hc));
  for (auto method : {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE}) {
    httpd_uri_t u = {};
    u.uri = "/*";
    u.method = method;
    u.handler = route;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u));
  }
  httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive);
  xTaskCreate(scan_worker, "wifi_scan", 6144, nullptr, 3, nullptr);
  xTaskCreate(dns, "setup_dns", 3072, nullptr, 2, nullptr);
  xTaskCreate(wifi_worker, "wifi_apply", 4096, nullptr, 3, nullptr);
}
} // namespace labbridge::esp

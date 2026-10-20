#include "app.hpp"
#include "captive_dns.hpp"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <set>

namespace lab {
static std::atomic<bool> station_connected{false}, setup_active{false};
static std::atomic<bool> wifi_scanning{false}, dns_started{false};
static std::atomic<uint64_t> joined_at{0};
static esp_netif_t *sta_netif, *ap_netif;
extern const char setup_page_start[] asm("_binary_setup_html_start");
extern const char setup_page_end[] asm("_binary_setup_html_end");
bool wifi_connected() { return station_connected; }
std::string setup_ssid() {
#if CONFIG_IDF_TARGET_ESP32
  uint8_t mac[6] = {};
  if (esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK) {
    char name[32];
    snprintf(name, sizeof(name), "LabBridge-Setup-%02X%02X", mac[4], mac[5]);
    return name;
  }
#endif
  return "LabBridge-Setup";
}
cJSON *network_diagnostics() {
  auto j = cJSON_CreateObject();
  wifi_config_t ap = {};
  bool configured = esp_wifi_get_config(WIFI_IF_AP, &ap) == ESP_OK;
  cJSON_AddBoolToObject(j, "ap_active", setup_active);
  cJSON_AddStringToObject(j, "ap_ssid", setup_ssid().c_str());
  cJSON_AddBoolToObject(j, "ap_open",
                        configured && ap.ap.authmode == WIFI_AUTH_OPEN &&
                            ap.ap.password[0] == 0);
  cJSON_AddBoolToObject(j, "captive_dns_started", dns_started);
  cJSON_AddBoolToObject(j, "station_connected", station_connected);
  esp_netif_ip_info_t ip = {};
  if (ap_netif)
    esp_netif_get_ip_info(ap_netif, &ip);
  char address[20];
  snprintf(address, sizeof(address), IPSTR, IP2STR(&ip.ip));
  cJSON_AddStringToObject(j, "ap_ip", address);
  return j;
}
static const char page[] =
    R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Laborbrücke</title>
<style>body{max-width:850px;margin:3em auto;padding:0 1em;font:16px system-ui;background:#f3f5f8;color:#182432}section{background:white;padding:1.5em;margin:1em 0;border-radius:12px}label{display:block;margin:.7em 0}input,select,button{padding:.6em;margin:.2em;max-width:95%}button{cursor:pointer}pre{white-space:pre-wrap;overflow-wrap:anywhere}.limits{display:flex;flex-wrap:wrap}.limits input{width:100px}small{color:#475569}</style>
<h1>Laborbrücke</h1><p>DL24M · RD6024 · lokale Messschnittstelle</p><p><a href="/wifi">WLAN einrichten</a> · <a href="/devices">Geräteeinstellungen</a></p><p id="notice" role="status"></p>
<section><h2>Zugang</h2><label>API-Token <input id="token" type="password" autocomplete="off"></label><button onclick="refresh()">Status laden</button><pre id="status"></pre></section>
<section><h2>Geräte zuordnen</h2><button onclick="discover()">Gefundene Geräte laden</button><label><span id="load_label">DL24M</span> <select id="ble"><option value="">Nicht zugeordnet</option></select></label><label id="pin_label" hidden>SPP-Kopplungscode <select id="spp_pin"><option value="1234">1234</option><option value="0000">0000</option></select></label><label>Netzteil 1 <select id="psu1"></select></label><label>Netzteil 2 <select id="psu2"></select></label><label>Last-Profil <select id="profile"><option value="monitor">Nur Messwerte</option><option value="px100-centi">PX100 · Rücklesen in 0,01 A</option><option value="px100-milli-read">PX100 · Rücklesen in 0,001 A</option><option value="dl24m-spp">DL24M · SPP mit Rücklesung</option></select></label><label><input id="checksum" type="checkbox">DL24M-Kompatibilität: abweichende Prüfsumme zulassen</label>
<p>Grenzen für die angeschlossenen Geräte und das Prüfobjekt:</p><div id="limits"></div><button onclick="save()">Zuordnung und Grenzen speichern</button><small>Die Last-Steuerung wird erst nach der dokumentierten Inbetriebnahme freigegeben.</small></section>
<script>
const $=id=>document.getElementById(id);let cfg;const ids=['load','psu1','psu2'];
for(const [n,id] of ids.entries()){$('limits').insertAdjacentHTML('beforeend',`<label>${['Last','Netzteil 1','Netzteil 2'][n]}</label><div class="limits"><input id="v${n}" type="number" min="0" step="0.01" placeholder="Volt"><input id="i${n}" type="number" min="0" step="0.01" placeholder="Ampere"><input id="p${n}" type="number" min="0" step="0.01" placeholder="Watt"></div>`)}
async function request(path,body,setup=false){let r=await fetch((setup?'':'/api/v1')+path,{method:body?'POST':'GET',headers:{'Content-Type':'application/json',Authorization:'Bearer '+$('token').value},body:body?JSON.stringify(body):undefined});let j=await r.json();if(!r.ok)throw Error(j.error||r.status);return j}
function show(e){$('notice').textContent=e.message||e}
async function refresh(){try{let j=await request('/devices');$('status').textContent=j.devices.map(d=>`${d.id}: ${d.connected?(d.fresh?'verbunden':'Messwerte veraltet'):'getrennt'} | ${d.voltage_v??'–'} V · ${d.current_a??'–'} A · ${d.power_w??'–'} W | Ausgang: ${d.output===null||d.output===undefined?'unbekannt':d.output?'ein':'aus'}`).join('\n');show('Status aktualisiert')}catch(e){show(e)}}
function choices(id,values,selected){let s=$(id);s.replaceChildren(new Option('Nicht zugeordnet',''));for(let [v,t] of values)s.add(new Option(t,v));if(selected&&!values.some(x=>x[0]===selected))s.add(new Option(selected,selected));s.value=selected||''}
async function discover(){try{let d=await request('/discovery');cfg=await request('/config');let spp=cfg.load_transport==='spp';$('load_label').textContent=spp?'DL24M SPP':'DL24M BLE';$('pin_label').hidden=!spp;$('spp_pin').value=cfg.spp_pin||'1234';for(let o of $('profile').options)o.hidden=spp?o.value.startsWith('px100'):o.value==='dl24m-spp';choices('ble',(spp?d.spp:d.ble).map(x=>[x.slice(0,17),x]),spp?cfg.spp_address:cfg.ble_address);for(let i=1;i<=2;i++)choices('psu'+i,d.usb.map(x=>[x.serial,x.model+' · '+x.serial]),cfg['psu'+i+'_serial']);$('profile').value=cfg.load_profile;$('checksum').checked=cfg.ignore_checksum;cfg.limits.forEach((l,i)=>{ $('v'+i).value=l.voltage_v;$('i'+i).value=l.current_a;$('p'+i).value=l.power_w;});show('Geräte geladen')}catch(e){show(e)}}
async function save(){try{let b={psu1_serial:$('psu1').value,psu2_serial:$('psu2').value,load_profile:$('profile').value,ignore_checksum:$('checksum').checked,limits:ids.map((_,i)=>({voltage_v:+$('v'+i).value,current_a:+$('i'+i).value,power_w:+$('p'+i).value}))};if(cfg.load_transport==='spp'){b.spp_address=$('ble').value;b.spp_pin=$('spp_pin').value}else b.ble_address=$('ble').value;await request('/config',b);show('Gespeichert')}catch(e){show(e)}}
</script></html>)HTML";
static const char wifi_inactive_page[] =
    R"HTML(<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>WLAN einrichten</title><style>body{max-width:34rem;margin:2rem auto;padding:0 1rem;font:16px system-ui;color:#182432}</style><h1>WLAN einrichten</h1><p>Das Einrichtungs-WLAN ist gerade nicht aktiv. Den Setup-/BOOT-Taster am ESP32 vier Sekunden halten (am StickS3: KEY1), dann mit dem offenen WLAN <strong>LabBridge-Setup</strong> verbinden und <a href="http://192.168.4.1/wifi">192.168.4.1/wifi</a> öffnen.</p><p><a href="/devices">Zu den Geräteeinstellungen</a></p></html>)HTML";
static esp_err_t respond(httpd_req_t *r, cJSON *j, int status = 200) {
  const char *code = status == 200   ? "200 OK"
                     : status == 400 ? "400 Bad Request"
                     : status == 401 ? "401 Unauthorized"
                     : status == 404 ? "404 Not Found"
                     : status == 409 ? "409 Conflict"
                     : status == 422 ? "422 Unprocessable Entity"
                     : status == 504 ? "504 Gateway Timeout"
                                     : "500 Internal Server Error";
  httpd_resp_set_status(r, code);
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  char *text = cJSON_PrintUnformatted(j);
  auto result = httpd_resp_send(r, text, HTTPD_RESP_USE_STRLEN);
  cJSON_free(text);
  cJSON_Delete(j);
  return result;
}
static esp_err_t fail(httpd_req_t *r, int code, const char *s) {
  auto j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "error", s);
  return respond(r, j, code);
}
static cJSON *read_body(httpd_req_t *r) {
  if (r->content_len <= 0 || r->content_len > 4096)
    return nullptr;
  std::string body(r->content_len, '\0');
  int read = 0;
  while (read < r->content_len) {
    int n = httpd_req_recv(r, body.data() + read, r->content_len - read);
    if (n <= 0)
      return nullptr;
    read += n;
  }
  const char *end = nullptr;
  auto j = cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, &end, true);
  if (!cJSON_IsObject(j)) {
    cJSON_Delete(j);
    return nullptr;
  }
  return j;
}
static void dns_task(void *) {
  esp_netif_ip_info_t ip = {};
  ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip));
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in local = {};
  local.sin_family = AF_INET;
  local.sin_port = htons(53);
  local.sin_addr.s_addr = ip.ip.addr;
  timeval timeout = {.tv_sec = 2, .tv_usec = 0};
  if (sock < 0 ||
      setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) ||
      bind(sock, reinterpret_cast<sockaddr *>(&local), sizeof(local))) {
    if (sock >= 0)
      close(sock);
    dns_started = false;
    vTaskDelete(nullptr);
    return;
  }
  std::array<uint8_t, 4> ap_ip;
  memcpy(ap_ip.data(), &ip.ip.addr, ap_ip.size());
  for (;;) {
    uint8_t query[512], reply[512];
    sockaddr_in peer = {};
    socklen_t peer_len = sizeof(peer);
    int n = recvfrom(sock, query, sizeof(query), 0,
                     reinterpret_cast<sockaddr *>(&peer), &peer_len);
    if (n <= 0 || !setup_active)
      continue;
    size_t reply_len = 0;
    if (captive_dns_reply(query, n, ap_ip.data(), reply, sizeof(reply),
                          reply_len))
      sendto(sock, reply, reply_len, 0, reinterpret_cast<sockaddr *>(&peer),
             peer_len);
  }
}
static void configure_setup_dhcp() {
  // DHCP option 114 tells modern clients where to open the captive portal.
  // DNS and HTTP redirection cover clients that only use connectivity probes.
  static char portal_uri[] = "http://192.168.4.1/wifi";
  esp_err_t stopped = esp_netif_dhcps_stop(ap_netif);
  if (stopped != ESP_OK && stopped != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    ESP_ERROR_CHECK(stopped);
  esp_netif_dns_info_t dns = {};
  dns.ip.type = ESP_IPADDR_TYPE_V4;
  esp_netif_ip_info_t ap_ip = {};
  ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ap_ip));
  dns.ip.u_addr.ip4.addr = ap_ip.ip.addr;
  ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns));
  uint8_t offer_dns = 1;
  ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
                                         ESP_NETIF_DOMAIN_NAME_SERVER,
                                         &offer_dns, sizeof(offer_dns)));
  ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
                                         ESP_NETIF_CAPTIVEPORTAL_URI,
                                         portal_uri, strlen(portal_uri)));
  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
}
static esp_err_t wifi_page(httpd_req_t *r) {
  httpd_resp_set_type(r, "text/html; charset=utf-8");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
  if (!setup_active)
    return httpd_resp_send(r, wifi_inactive_page, HTTPD_RESP_USE_STRLEN);
  return httpd_resp_send(r, setup_page_start,
                         setup_page_end - setup_page_start);
}
static esp_err_t devices_page(httpd_req_t *r) {
  httpd_resp_set_type(r, "text/html; charset=utf-8");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
  return httpd_resp_send(r, page, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t root(httpd_req_t *r) {
  return setup_active && !station_connected ? wifi_page(r) : devices_page(r);
}
static esp_err_t captive_404(httpd_req_t *r, httpd_err_code_t) {
  if (!setup_active || r->method != HTTP_GET)
    return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
  httpd_resp_set_status(r, "303 See Other");
  httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/wifi");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_send(r, "WLAN-Einrichtung: http://192.168.4.1/wifi",
                         HTTPD_RESP_USE_STRLEN);
}
static esp_err_t api(httpd_req_t *r) {
  char auth[128] = {};
  httpd_req_get_hdr_value_str(r, "Authorization", auth, sizeof(auth));
  {
    Guard g;
    if (std::string(auth) != "Bearer " + config.api_token)
      return fail(r, 401, "invalid_token");
  }
  cJSON *body = r->method == HTTP_POST ? read_body(r) : nullptr;
  if (r->method == HTTP_POST && !body)
    return fail(r, 400, "json_object_required_max_4096_bytes");
  char token[80] = {};
  httpd_req_get_hdr_value_str(r, "X-Lab-Lease", token, sizeof(token));
  int status;
  cJSON *result;
  {
    Guard g;
    result = api_call(r->method == HTTP_GET ? "GET" : "POST",
                      std::string(r->uri).substr(7), body, token, status);
  }
  cJSON_Delete(body);
  return respond(r, result, status);
}
static esp_err_t setup(httpd_req_t *r) {
  if (!setup_active)
    return fail(r, 404, "setup_not_active");
  std::string path = r->uri;
  if (path == "/setup/status") {
    auto j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "ap_ssid", setup_ssid().c_str());
    cJSON_AddBoolToObject(j, "connected", station_connected);
    esp_netif_ip_info_t ip = {};
    esp_netif_get_ip_info(sta_netif, &ip);
    char s[20];
    snprintf(s, sizeof(s), IPSTR, IP2STR(&ip.ip));
    cJSON_AddStringToObject(j, "ip", s);
    return respond(r, j);
  }
  if (path == "/setup/scan") {
    wifi_scanning = true;
    if (!station_connected)
      esp_wifi_disconnect();
    wifi_scan_config_t scan = {};
    esp_err_t scanned = esp_wifi_scan_start(&scan, true);
    wifi_scanning = false;
    if (scanned != ESP_OK)
      return fail(r, 409, "wifi_busy_retry");
    wifi_ap_record_t records[20];
    uint16_t count = 20;
    if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK)
      return fail(r, 409, "wifi_scan_results_unavailable");
    auto j = cJSON_CreateObject();
    auto a = cJSON_AddArrayToObject(j, "networks");
    std::set<std::string> names;
    for (int i = 0; i < count; i++) {
      char name[33] = {};
      memcpy(name, records[i].ssid, 32);
      if (name[0])
        names.insert(name);
    }
    for (const auto &name : names)
      cJSON_AddItemToArray(a, cJSON_CreateString(name.c_str()));
    return respond(r, j);
  }
  if (path == "/setup/connect" && r->method == HTTP_POST) {
    auto body = read_body(r);
    if (!body)
      return fail(r, 400, "json_object_required");
    std::string ssid = str(body, "ssid"), password = str(body, "password");
    cJSON_Delete(body);
    if (ssid.empty() || ssid.size() > 32 || password.size() > 63 ||
        (!password.empty() && password.size() < 8))
      return fail(r, 400, "invalid_wifi_credentials");
    std::string token;
    {
      Guard g;
      int status;
      auto s = api_call("GET", "/session", nullptr, "", status);
      bool active = flag(s, "active");
      cJSON_Delete(s);
      if (active)
        return fail(r, 409, "stop_session_first");
      config.ssid = ssid;
      config.password = password;
      save_config();
      token = config.api_token;
    }
    esp_wifi_disconnect();
    station_connected = false;
    joined_at = 0;
    wifi_config_t cfg = {};
    memcpy(cfg.sta.ssid, ssid.data(), ssid.size());
    memcpy(cfg.sta.password, password.data(), password.size());
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
    auto j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "api_token", token.c_str());
    cJSON_AddStringToObject(j, "status", "connecting");
    return respond(r, j);
  }
  return fail(r, 404, "not_found");
}
static void wifi_event(void *, esp_event_base_t base, int32_t id, void *) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    station_connected = false;
    joined_at = 0;
  }
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    station_connected = true;
    joined_at = now_ms();
  }
}
void enter_setup() {
  Guard g;
  wifi_config_t ap = {};
  auto name = setup_ssid();
  memcpy(ap.ap.ssid, name.data(), name.size());
  ap.ap.ssid_len = name.size();
  ap.ap.password[0] = 0;
  ap.ap.authmode = WIFI_AUTH_OPEN;
  ap.ap.max_connection = 3;
  ap.ap.channel = 1;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
  configure_setup_dhcp();
  setup_active = true;
  joined_at = station_connected ? now_ms() : 0;
  bool expected = false;
  if (dns_started.compare_exchange_strong(expected, true) &&
      xTaskCreate(dns_task, "setup_dns", 3072, nullptr, 2, nullptr) != pdPASS)
    dns_started = false;
  printf("Setup Wi-Fi: %s (open) | http://192.168.4.1/wifi\n",
         name.c_str());
}
static void wifi_task(void *) {
  for (;;) {
    if (!station_connected && !wifi_scanning) {
      Guard g;
      if (!config.ssid.empty())
        esp_wifi_connect();
    }
    if (setup_active && station_connected && joined_at &&
        now_ms() - joined_at > 60000) {
      esp_wifi_set_mode(WIFI_MODE_STA);
      setup_active = false;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
void start_network() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  sta_netif = esp_netif_create_default_wifi_sta();
  ap_netif = esp_netif_create_default_wifi_ap();
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, nullptr);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event,
                             nullptr);
  esp_wifi_set_mode(WIFI_MODE_STA);
  wifi_config_t cfg = {};
  memcpy(cfg.sta.ssid, config.ssid.data(), config.ssid.size());
  memcpy(cfg.sta.password, config.password.data(), config.password.size());
  esp_wifi_set_config(WIFI_IF_STA, &cfg);
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (config.ssid.empty())
    enter_setup();
  else
    esp_wifi_connect();
  ESP_ERROR_CHECK(mdns_init());
  std::string hostname = "labbridge";
#if CONFIG_IDF_TARGET_ESP32
  uint8_t ap_mac[6] = {};
  if (esp_wifi_get_mac(WIFI_IF_AP, ap_mac) == ESP_OK) {
    char suffix[6];
    snprintf(suffix, sizeof(suffix), "-%02x%02x", ap_mac[4], ap_mac[5]);
    hostname += suffix;
  }
#endif
  ESP_ERROR_CHECK(mdns_hostname_set(hostname.c_str()));
  mdns_instance_name_set("Laborbruecke");
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
  httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
  hc.uri_match_fn = httpd_uri_match_wildcard;
  hc.stack_size = 10240;
  hc.max_uri_handlers = 8;
  hc.lru_purge_enable = true;
  hc.recv_wait_timeout = 3;
  httpd_handle_t server;
  ESP_ERROR_CHECK(httpd_start(&server, &hc));
  ESP_ERROR_CHECK(
      httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_404));
  httpd_uri_t u = {};
  u.uri = "/";
  u.method = HTTP_GET;
  u.handler = root;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u));
  u.uri = "/wifi";
  u.handler = wifi_page;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u));
  u.uri = "/devices";
  u.handler = devices_page;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u));
  for (auto method : {HTTP_GET, HTTP_POST}) {
    u.uri = "/setup/*";
    u.method = method;
    u.handler = setup;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u));
    u.uri = "/api/v1/*";
    u.handler = api;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &u));
  }
  xTaskCreate(wifi_task, "wifi_reconnect", 3072, nullptr, 3, nullptr);
}
} // namespace lab

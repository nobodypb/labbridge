#include "app.hpp"
#include <cstdio>

namespace lab {
#if CONFIG_LAB_SERIAL_DIAGNOSTICS
// The physical USB console exposes the same guarded API as HTTP. Legacy
// diagnostic operations remain for compatibility with serial_diag.py.
static cJSON *serial_reply(const std::string &line) {
  auto input =
      cJSON_ParseWithLengthOpts(line.c_str(), line.size() + 1, nullptr, true);
  auto reply = cJSON_CreateObject();
  auto id = cJSON_GetObjectItemCaseSensitive(input, "id");
  if (cJSON_IsNumber(id))
    cJSON_AddNumberToObject(reply, "id", id->valuedouble);
  int status = 400;
  cJSON *result = nullptr;
  {
    Guard guard;
    std::string op = str(input, "op");
    if (cJSON_IsObject(input) && op == "request") {
      auto method = str(input, "method");
      auto path = str(input, "path");
      auto body = cJSON_GetObjectItemCaseSensitive(input, "body");
      auto token = str(input, "lease");
      if ((std::string(method) == "GET" && !body && path[0] == '/') ||
          (std::string(method) == "POST" && cJSON_IsObject(body) &&
           path[0] == '/')) {
        result = api_call(method, path, body, token, status, true);
      } else {
        result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "error", "invalid_serial_request");
      }
    } else if (cJSON_IsObject(input) && op == "get") {
      result = api_call("GET", str(input, "path"), nullptr, "", status);
    } else if (cJSON_IsObject(input) && op == "probe") {
      result = api_call("POST", "/diagnostics/load/probe", input, "", status);
    } else if (cJSON_IsObject(input) && op == "pulse") {
      result = load_bench_pulse(input, status);
    } else if (cJSON_IsObject(input) && op == "select" &&
               cJSON_IsString(
                   cJSON_GetObjectItemCaseSensitive(input, "address"))) {
      auto body = cJSON_CreateObject();
#if CONFIG_IDF_TARGET_ESP32
      cJSON_AddStringToObject(body, "spp_address", str(input, "address"));
#else
      cJSON_AddStringToObject(body, "ble_address", str(input, "address"));
#endif
      cJSON_AddStringToObject(body, "load_profile", "monitor");
      cJSON_AddBoolToObject(body, "ignore_checksum",
                            flag(input, "ignore_checksum"));
      result = api_call("POST", "/config", body, "", status);
      cJSON_Delete(body);
    } else if (cJSON_IsObject(input) && op == "setup") {
      result = cJSON_CreateObject();
      cJSON_AddStringToObject(result, "ssid", setup_ssid().c_str());
      cJSON_AddStringToObject(result, "url", "http://192.168.4.1/wifi");
      cJSON_AddBoolToObject(result, "station_connected", wifi_connected());
      enter_setup();
      cJSON_AddItemToObject(result, "network", network_diagnostics());
      status = 200;
    }
  }
  if (!result) {
    result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "error",
                            "expected_request_get_select_setup_probe_or_pulse");
  }
  cJSON_AddNumberToObject(reply, "status", status);
  cJSON_AddItemToObject(reply, "result", result);
  cJSON_Delete(input);
  return reply;
}
static void console_task(void *) {
  setvbuf(stdin, nullptr, _IONBF, 0);
  std::string line;
  bool overflow = false;
  for (;;) {
    int c = getchar();
    if (c == EOF) {
      clearerr(stdin);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (c == '\n') {
      if (!overflow && !line.empty()) {
        auto reply = serial_reply(line);
        char *text = cJSON_PrintUnformatted(reply);
        printf("LABJSON %s\n", text);
        fflush(stdout);
        cJSON_free(text);
        cJSON_Delete(reply);
      }
      line.clear();
      overflow = false;
    } else if (c != '\r' && !overflow) {
      if (line.size() >= 4096) {
        line.clear();
        overflow = true;
      } else {
        line += char(c);
      }
    }
  }
}
#endif
void start_serial_diagnostics() {
#if CONFIG_LAB_SERIAL_DIAGNOSTICS
  xTaskCreate(console_task, "serial_diag", 6144, nullptr, 2, nullptr);
#endif
}
} // namespace lab

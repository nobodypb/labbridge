#include "auth_platform.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform.hpp"
#include <cstdio>
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#endif
namespace labbridge::esp {
namespace {
constexpr size_t maximum_request_bytes = 16384;
}
void console_respond(const std::string &id, Response response) {
  Json reply, headers;
  for (const auto &p : response.headers)
    headers.set(p.first, p.second);
  reply.set("id", id)
      .set("status", double(response.status))
      .set("body", response.body)
      .set("headers", headers);
  // One stdio call preserves complete LABJSON lines from console and worker.
  printf("LABJSON %s\n", reply.dump().c_str());
  fflush(stdout);
}
static void console(void *) {
  setvbuf(stdin, nullptr, _IONBF, 0);
  std::string line;
  bool overflow = false;
  for (;;) {
    int c = getchar();
    if (c == EOF) {
      clearerr(stdin);
      sleep_ms(10);
      continue;
    }
    if (c == '\n') {
      if (!overflow && !line.empty()) {
        std::string id;
        try {
          auto request = Json::parse(line);
          id = request.text("id");
          Headers headers;
          auto h = request.at("headers");
          if (cJSON_IsObject(h))
            for (auto p = h->child; p; p = p->next)
              if (cJSON_IsString(p))
                headers[p->string] = p->valuestring;
          auto method = request.text("method"), path = request.text("path");
          if (Auth::route(path) && (method == "POST" || method == "PUT"))
            usb_auth(method, path, request.item("body"), headers, id);
          else
            console_respond(id, dispatch(method, path, request.item("body"),
                                         headers, true));
        } catch (const std::exception &e) {
          console_respond(id, problem(400, e.what()));
        }
      }
      line.clear();
      overflow = false;
    } else if (c != '\r' && !overflow) {
      if (line.size() >= maximum_request_bytes) {
        line.clear();
        overflow = true;
      } else
        line += char(c);
    }
  }
}
void start_console() {
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
  // The default polling VFS can discard bytes when the hardware FIFO fills.
  // API replies require interrupt-driven buffering, including during host
  // stalls.
  usb_serial_jtag_driver_config_t config = {};
  config.tx_buffer_size = 4096;
  // A JSON-escaped 128-character Unicode password and its confirmation exceed
  // 2 KiB. Keep an entire allowed frame plus its delimiter while the console
  // task is descheduled; the ISR otherwise discards bytes from a full buffer.
  config.rx_buffer_size = maximum_request_bytes + 64;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
  usb_serial_jtag_vfs_use_driver();
#endif
  xTaskCreate(console, "usb_api", 12288, nullptr, 4, nullptr);
}
} // namespace labbridge::esp

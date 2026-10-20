#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform.hpp"
#include <cstdio>
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#endif
namespace labbridge::esp {
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
        Json reply;
        try {
          auto request = Json::parse(line);
          reply.set("id", request.text("id"));
          Headers headers;
          auto h = request.at("headers");
          if (cJSON_IsObject(h))
            for (auto p = h->child; p; p = p->next)
              if (cJSON_IsString(p))
                headers[p->string] = p->valuestring;
          auto response = dispatch(request.text("method"), request.text("path"),
                                   request.item("body"), headers, true);
          Json rh;
          for (auto &p : response.headers)
            rh.set(p.first, p.second);
          reply.set("status", double(response.status))
              .set("body", std::move(response.body))
              .set("headers", rh);
        } catch (const std::exception &e) {
          reply.set("status", 400.).set("body", problem(400, e.what()).body);
        }
        printf("LABJSON %s\n", reply.dump().c_str());
        fflush(stdout);
      }
      line.clear();
      overflow = false;
    } else if (c != '\r' && !overflow) {
      if (line.size() >= 16384) {
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
  config.rx_buffer_size = 2048;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
  usb_serial_jtag_vfs_use_driver();
#endif
  xTaskCreate(console, "usb_api", 12288, nullptr, 4, nullptr);
}
} // namespace labbridge::esp

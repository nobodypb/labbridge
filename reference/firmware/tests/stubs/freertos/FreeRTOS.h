#pragma once
#include <cstdint>
using TickType_t = uint32_t;
constexpr uint32_t portMAX_DELAY = 0xffffffff;
#define pdMS_TO_TICKS(x) (x)
void vTaskDelay(uint32_t);
inline int xTaskCreate(void (*)(void *), const char *, unsigned, void *,
                       unsigned, void *) {
  return 1;
}

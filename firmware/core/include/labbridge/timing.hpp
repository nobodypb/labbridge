#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace labbridge {
// RTOS delay conversion must round UP: a 5-ms worker on a 100-Hz scheduler
// otherwise becomes a high-priority busy loop that starves boot and safety.
constexpr uint32_t delay_ticks(uint32_t milliseconds, uint32_t tick_hz) {
  if (!tick_hz)
    throw std::invalid_argument("Tick rate must be positive");
  const uint64_t ticks = (uint64_t(milliseconds) * tick_hz + 999) / 1000;
  return uint32_t(std::max<uint64_t>(1, std::min<uint64_t>(ticks, 0xfffffffe)));
}
} // namespace labbridge

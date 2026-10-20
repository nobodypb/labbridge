#pragma once
using SemaphoreHandle_t = void *;
inline void xSemaphoreTakeRecursive(SemaphoreHandle_t, unsigned) {}
inline void xSemaphoreGiveRecursive(SemaphoreHandle_t) {}

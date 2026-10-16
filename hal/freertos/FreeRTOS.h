#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <thread>

typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
typedef uint32_t UBaseType_t;

#define portMAX_DELAY 0xFFFFFFFFUL
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdFAIL  0

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

#endif // MOCK_FREERTOS_H

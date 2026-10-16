#ifndef MOCK_FREERTOS_SEMPHR_H
#define MOCK_FREERTOS_SEMPHR_H

#include "FreeRTOS.h"
#include <mutex>

struct MockMutex {
    std::timed_mutex mtx;
};

typedef MockMutex* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    return new MockMutex();
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime) {
    if (!xSemaphore) return pdFAIL;
    if (xBlockTime == portMAX_DELAY) {
        xSemaphore->mtx.lock();
        return pdPASS;
    } else if (xBlockTime == 0) {
        return xSemaphore->mtx.try_lock() ? pdPASS : pdFAIL;
    } else {
        return xSemaphore->mtx.try_lock_for(std::chrono::milliseconds(xBlockTime)) ? pdPASS : pdFAIL;
    }
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    if (!xSemaphore) return pdFAIL;
    xSemaphore->mtx.unlock();
    return pdPASS;
}

#endif // MOCK_FREERTOS_SEMPHR_H

#ifndef MOCK_FREERTOS_TASK_H
#define MOCK_FREERTOS_TASK_H

#include "FreeRTOS.h"
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>

typedef void (*TaskFunction_t)(void *);
typedef void *TaskHandle_t;

inline void vTaskDelay(TickType_t ticks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

inline BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                              const char *const pcName,
                              const uint32_t usStackDepth,
                              void *const pvParameters,
                              UBaseType_t uxPriority,
                              TaskHandle_t *const pxCreatedTask) {
    (void)usStackDepth;
    (void)uxPriority;
    std::string taskName = pcName ? pcName : "UnnamedTask";
    
    // Launch thread detached or managed
    std::thread([pxTaskCode, pvParameters, taskName]() {
        try {
            pxTaskCode(pvParameters);
        } catch (const std::exception &e) {
            std::cerr << "[FreeRTOS Task " << taskName << "] Exception: " << e.what() << std::endl;
        }
    }).detach();

    if (pxCreatedTask) {
        *pxCreatedTask = (void*)1;
    }
    return pdPASS;
}

#endif // MOCK_FREERTOS_TASK_H

#ifndef MOCK_FREERTOS_QUEUE_H
#define MOCK_FREERTOS_QUEUE_H

#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <cstring>

struct MockQueue {
    size_t length;
    size_t itemSize;
    std::deque<std::vector<uint8_t>> items;
    std::mutex mtx;
    std::condition_variable cv_not_empty;
    std::condition_variable cv_not_full;
};

typedef MockQueue* QueueHandle_t;

inline QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    auto q = new MockQueue();
    q->length = uxQueueLength;
    q->itemSize = uxItemSize;
    return q;
}

inline BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait) {
    if (!xQueue || !pvItemToQueue) return pdFAIL;
    std::unique_lock<std::mutex> lock(xQueue->mtx);

    if (xTicksToWait == 0) {
        if (xQueue->items.size() >= xQueue->length) return pdFAIL;
    } else if (xTicksToWait == portMAX_DELAY) {
        xQueue->cv_not_full.wait(lock, [xQueue]() { return xQueue->items.size() < xQueue->length; });
    } else {
        if (!xQueue->cv_not_full.wait_for(lock, std::chrono::milliseconds(xTicksToWait),
                                          [xQueue]() { return xQueue->items.size() < xQueue->length; })) {
            return pdFAIL;
        }
    }

    std::vector<uint8_t> itemData(xQueue->itemSize);
    std::memcpy(itemData.data(), pvItemToQueue, xQueue->itemSize);
    xQueue->items.push_back(std::move(itemData));
    xQueue->cv_not_empty.notify_one();
    return pdPASS;
}

inline BaseType_t xQueueSendToFront(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait) {
    if (!xQueue || !pvItemToQueue) return pdFAIL;
    std::unique_lock<std::mutex> lock(xQueue->mtx);

    if (xTicksToWait == 0) {
        if (xQueue->items.size() >= xQueue->length) return pdFAIL;
    } else if (xTicksToWait == portMAX_DELAY) {
        xQueue->cv_not_full.wait(lock, [xQueue]() { return xQueue->items.size() < xQueue->length; });
    } else {
        if (!xQueue->cv_not_full.wait_for(lock, std::chrono::milliseconds(xTicksToWait),
                                          [xQueue]() { return xQueue->items.size() < xQueue->length; })) {
            return pdFAIL;
        }
    }

    std::vector<uint8_t> itemData(xQueue->itemSize);
    std::memcpy(itemData.data(), pvItemToQueue, xQueue->itemSize);
    xQueue->items.push_front(std::move(itemData));
    xQueue->cv_not_empty.notify_one();
    return pdPASS;
}

inline BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait) {
    if (!xQueue || !pvBuffer) return pdFAIL;
    std::unique_lock<std::mutex> lock(xQueue->mtx);

    if (xTicksToWait == 0) {
        if (xQueue->items.empty()) return pdFAIL;
    } else if (xTicksToWait == portMAX_DELAY) {
        xQueue->cv_not_empty.wait(lock, [xQueue]() { return !xQueue->items.empty(); });
    } else {
        if (!xQueue->cv_not_empty.wait_for(lock, std::chrono::milliseconds(xTicksToWait),
                                           [xQueue]() { return !xQueue->items.empty(); })) {
            return pdFAIL;
        }
    }

    std::memcpy(pvBuffer, xQueue->items.front().data(), xQueue->itemSize);
    xQueue->items.pop_front();
    xQueue->cv_not_full.notify_one();
    return pdPASS;
}

inline UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue) {
    if (!xQueue) return 0;
    std::lock_guard<std::mutex> lock(const_cast<QueueHandle_t>(xQueue)->mtx);
    return (UBaseType_t)xQueue->items.size();
}

#endif // MOCK_FREERTOS_QUEUE_H

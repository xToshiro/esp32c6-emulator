#ifndef MOCK_ESP_TASK_WDT_H
#define MOCK_ESP_TASK_WDT_H

#include <cstdint>

struct esp_task_wdt_config_t {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
};

inline void esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config) { (void)config; }
inline void esp_task_wdt_add(void *handle) { (void)handle; }
inline void esp_task_wdt_reset() {}

#endif // MOCK_ESP_TASK_WDT_H

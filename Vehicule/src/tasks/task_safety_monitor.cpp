/**
 * @file task_safety_monitor.cpp
 * @brief Safety monitor task - 50ms period, Priority 5 (highest)
 * @details Monitors system health, watchdog, timeouts
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../safety/watchdog.h"
#include "../safety/timeout_monitor.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_safety_monitor(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SAFETY_MONITOR);  // 50ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before starting monitoring
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    while (1) {
        // Feed watchdog (prevents system reset)
        watchdog_feed();
        
        // Check command timeout
        timeout_monitor_check();
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

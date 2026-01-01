/**
 * @file task_telemetry.cpp
 * @brief Telemetry task - 100ms period, Priority 1 (lowest)
 * @details Reports system status, motor states, sensor readings
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../shared/types.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_telemetry(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_TELEMETRY);  // 100ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_TELEMETRY] Telemetry task started");
    
    // Wait for setup to complete before starting
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    Serial.println("[TASK_TELEMETRY] Telemetry task ready");
    
    while (1) {
        // TODO: Read motor status from xMotorStatusQueue
        // TODO: Read sensor data
        // TODO: Format telemetry packet
        // TODO: Send via Serial or WiFi (if enabled)
        
        // For now, just maintain timing
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

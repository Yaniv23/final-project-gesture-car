/**
 * @file task_telemetry.cpp
 * @brief Telemetry task - 100ms period, Priority 1 (lowest)
 * @details Reports system status, motor states, sensor readings
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../shared/types.h"

void task_telemetry(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_TELEMETRY);  // 100ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_TELEMETRY] Telemetry task started");
    
    while (1) {
        // TODO: Read motor status from xMotorStatusQueue
        // TODO: Read sensor data
        // TODO: Format telemetry packet
        // TODO: Send via Serial or WiFi (if enabled)
        
        // For now, just maintain timing
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

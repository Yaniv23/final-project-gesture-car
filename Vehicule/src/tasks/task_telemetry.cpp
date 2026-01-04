/**
 * @file task_telemetry.cpp
 * @brief Telemetry task - 100ms period, Priority 1 (lowest)
 * @details Reports system status, motor states, sensor readings, temperature monitoring
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../shared/types.h"
#include "../safety/emergency_stop.h"
#include "../communication/espnow_handler.h"

// Temperature state enumeration
enum TempState {
    TEMP_NORMAL = 0,
    TEMP_WARNING = 1,
    TEMP_CRITICAL = 2
};

void task_telemetry(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_TELEMETRY);  // 100ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_TELEMETRY] Telemetry task started");
    
    // Temperature monitoring variables
    float current_temp = 0.0f;
    TempState temp_state = TEMP_NORMAL;
    TickType_t last_temp_read = 0;
    TickType_t temp_read_interval = pdMS_TO_TICKS(TEMP_READ_INTERVAL_NORMAL);  // Start with 3s
    bool last_estop_temp_state = false;
    
    Serial.println("[TASK_TELEMETRY] Temperature monitoring initialized");
    
    while (1) {
        TickType_t current_time = xTaskGetTickCount();
        
        // Check if it's time to read temperature (adaptive interval)
        if ((current_time - last_temp_read) >= temp_read_interval) {
            // Read temperature from ESP32 built-in sensor
            // temperatureRead() from Arduino ESP32 core returns temperature in Celsius
            // Note: This reads internal chip temperature, not ambient temperature
            current_temp = temperatureRead();
            last_temp_read = current_time;
            
            // Determine temperature state and adjust reading interval
            TempState new_state;
            TickType_t new_interval;
            
            if (current_temp < TEMP_NORMAL_MAX) {
                // Normal temperature
                new_state = TEMP_NORMAL;
                new_interval = pdMS_TO_TICKS(TEMP_READ_INTERVAL_NORMAL);  // 3 seconds
                
                // If transitioning from warning/critical to normal, clear temperature emergency stop
                if (temp_state != TEMP_NORMAL && emergency_stop_is_temp_active()) {
                    emergency_stop_clear_temp();
                    Serial.println("[TEMP] Temperature returned to normal - Emergency stop cleared");
                }
            } else if (current_temp < TEMP_CRITICAL_MIN) {
                // Warning temperature
                new_state = TEMP_WARNING;
                new_interval = pdMS_TO_TICKS(TEMP_READ_INTERVAL_ALERT);  // 300ms
            } else {
                // Critical temperature
                new_state = TEMP_CRITICAL;
                new_interval = pdMS_TO_TICKS(TEMP_READ_INTERVAL_ALERT);  // 300ms
                
                // Trigger emergency stop if not already active
                if (!emergency_stop_is_temp_active()) {
                    emergency_stop_trigger_temp();
                    Serial.print("[TEMP] CRITICAL temperature detected: ");
                    Serial.print(current_temp);
                    Serial.println("°C - Emergency stop triggered!");
                }
            }
            
            // Check if state changed
            if (new_state != temp_state) {
                temp_state = new_state;
                const char* state_str = (temp_state == TEMP_NORMAL) ? "NORMAL" :
                                       (temp_state == TEMP_WARNING) ? "WARNING" : "CRITICAL";
                Serial.print("[TEMP] State changed to: ");
                Serial.print(state_str);
                Serial.print(" (");
                Serial.print(current_temp);
                Serial.println("°C)");
            }
            
            // Update reading interval if changed
            if (new_interval != temp_read_interval) {
                temp_read_interval = new_interval;
                Serial.print("[TEMP] Reading interval changed to: ");
                Serial.print(temp_read_interval * portTICK_PERIOD_MS);
                Serial.println("ms");
            }
            
            // Send temperature via ESP-NOW if in warning or critical state
            if (temp_state == TEMP_WARNING || temp_state == TEMP_CRITICAL) {
                espnow_send_temperature(current_temp, (uint8_t)temp_state);
            }
            
            // Auto-resume: If temperature drops below critical threshold, clear emergency stop
            // This happens when state transitions from CRITICAL to WARNING or NORMAL
            if (temp_state != TEMP_CRITICAL && emergency_stop_is_temp_active()) {
                emergency_stop_clear_temp();
                Serial.println("[TEMP] Temperature dropped below critical - Auto-resuming");
            }
        }
        
        // Check for temperature emergency stop state changes
        bool current_estop_temp_state = emergency_stop_is_temp_active();
        if (current_estop_temp_state != last_estop_temp_state) {
            if (current_estop_temp_state) {
                Serial.println("[TEMP] Temperature emergency stop is ACTIVE");
            } else {
                Serial.println("[TEMP] Temperature emergency stop is CLEARED");
            }
            last_estop_temp_state = current_estop_temp_state;
        }
        
        // TODO: Read motor status from xMotorStatusQueue
        // TODO: Read sensor data
        // TODO: Format telemetry packet
        // TODO: Send via Serial or WiFi (if enabled)
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

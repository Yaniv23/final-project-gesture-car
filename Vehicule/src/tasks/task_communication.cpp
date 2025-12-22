/**
 * @file task_communication.cpp
 * @brief Communication task - 100ms period, Priority 2
 * @details Handles ESP-NOW communication and binary protocol
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../communication/espnow_handler.h"
#include "../communication/command_protocol.h"

void task_communication(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(10);  // 10ms period (faster for responsiveness)
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_COMM] Communication task started");
    
    // Initialize ESP-NOW
    if (!espnow_init()) {
        Serial.println("[ERROR] ESP-NOW init failed!");
        vTaskDelete(NULL);
        return;
    }
    
    uint8_t cmd_byte = 0;
    
    while (1) {
        // Read command from ESP-NOW queue (ISR puts commands here)
        if (xQueueReceive(xESPNowQueue, &cmd_byte, pdMS_TO_TICKS(100))) {
            // Validate command
            if (isValidCommand(cmd_byte)) {
                Serial.print("[COMM] Received command byte: 0x");
                Serial.println(cmd_byte, HEX);
                
                // Send to motor control task
                if (xQueueSend(xCommandQueue, &cmd_byte, 0) != pdTRUE) {
                    Serial.println("[COMM] Warning: Command queue full!");
                }
            } else {
                Serial.print("[COMM] Invalid command byte: 0x");
                Serial.println(cmd_byte, HEX);
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

/**
 * @file task_communication.cpp
 * @brief Communication task - Priority 2
 * @details Handles ESP-NOW communication and binary protocol
 *          Period: TASK_PERIOD_COMMUNICATION (100ms from config.h)
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../shared/types.h"
#include "../communication/espnow_handler.h"
#include "../communication/command_protocol.h"
#include "../safety/timeout_monitor.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_communication(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION);  // Use config value (100ms = 10 Hz)
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_COMM] Communication task started");
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    #if SIMULATION_MODE
    Serial.println("[TASK_COMM] Running in SIMULATION MODE - ESP-NOW disabled");
    Serial.println("[TASK_COMM] Commands can be sent directly to xCommandQueue for testing");
    #else
    // Initialize ESP-NOW only in normal mode
    if (!espnow_init(SIMULATION_MODE)) {
        Serial.println("[ERROR] ESP-NOW init failed!");
        vTaskDelete(NULL);
        return;
    }
    Serial.println("[TASK_COMM] Running in NORMAL MODE - waiting for ESP-NOW commands");
    #endif
    
    ESPNowRawMessage raw_msg;
    
    while (1) {
        #if SIMULATION_MODE
        // In simulation mode, just process commands from xCommandQueue
        // Test tasks can directly send to xCommandQueue
        // This task doesn't need to do anything except yield CPU time
        vTaskDelayUntil(&lastWakeTime, period);
        #else
        // Read command from ESP-NOW queue (ISR puts commands here)
        if (xQueueReceive(xESPNowQueue, &raw_msg, pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION))) {
            // Always print what we received (for debugging)
            Serial.print("[COMM] Received message - Length: ");
            Serial.print(raw_msg.length);
            Serial.print(" bytes, First byte: 0x");
            Serial.print(raw_msg.first_byte, HEX);
            Serial.print(" (");
            Serial.print(raw_msg.first_byte);
            Serial.print(")");
            
            if (raw_msg.length >= 2) {
                Serial.print(", Second byte: 0x");
                Serial.print(raw_msg.second_byte, HEX);
                Serial.print(" (");
                Serial.print(raw_msg.second_byte);
                Serial.print(")");
            }
            
            // Validate command
            if (raw_msg.length >= 1 && isValidCommand(raw_msg.first_byte)) {
                Serial.println(" - VALID command");
                
                // Send to motor control task
                uint8_t cmd_byte = raw_msg.first_byte;
                if (xQueueSend(xCommandQueue, &cmd_byte, 0) != pdTRUE) {
                    Serial.println("[COMM] Warning: Command queue full!");
                }
            } else {
                if (raw_msg.length != 1) {
                    Serial.print(" - WRONG LENGTH (expected 1 byte, got ");
                    Serial.print(raw_msg.length);
                    Serial.println(" bytes)");
                } else {
                    Serial.println(" - INVALID command byte (ignored)");
                }
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
        #endif
    }
}

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
#include "../control/mode_manager.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_communication(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION);  // Use config value (100ms = 10 Hz)
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    // ESP-NOW is initialized once in main.cpp during setup()
    ESPNowRawMessage raw_msg;
    
    while (1) {
        // Read command from ESP-NOW queue (ISR puts commands here)
        if (xQueueReceive(xESPNowQueue, &raw_msg, pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION))) {
            if (raw_msg.length >= 1) {
                uint8_t first_byte = raw_msg.first_byte;

                // -----------------------------------------------------------------
                // Handshake / control messages (NOT motion commands)
                // -----------------------------------------------------------------
                if (first_byte == CMD_HANDSHAKE_INIT) {
                    // Echo back an ACK with the protocol/status byte
                    espnow_send_handshake_ack(raw_msg.second_byte);
                    // Also send current mode status to sender
                    ModeManager& mode_mgr = ModeManager::getInstance();
                    uint8_t current_mode = (mode_mgr.isAutonomousMode()) ? 1 : 0;
                    espnow_send_mode_status(current_mode);
                    // Do NOT forward handshake frames to motor control
                } else if (first_byte == CMD_HEARTBEAT) {
                    // Optional future use: could feed timeout monitor here
                } else {
                    // -----------------------------------------------------------------
                    // Normal motion commands and mode control commands
                    // -----------------------------------------------------------------
                    // Check if it's a valid motion command (0x00-0x0C)
                    if (isValidCommand(first_byte)) {
                        uint8_t cmd_byte = first_byte;
                        if (xQueueSend(xCommandQueue, &cmd_byte, 0) != pdTRUE) {
                            Serial.println("[COMM] Warning: Command queue full!");
                        }
                    } 
                    // Check if it's a mode control command (0x20-0x22)
                    else if (first_byte == CMD_MODE_MANUAL || 
                             first_byte == CMD_MODE_AUTONOMOUS || 
                             first_byte == CMD_MODE_TOGGLE) {
                        uint8_t cmd_byte = first_byte;
                        xQueueSend(xCommandQueue, &cmd_byte, 0);
                        // Command dropped if queue full (non-blocking)
                    } else {
                        // Invalid command byte - ignored
                    }
                }
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

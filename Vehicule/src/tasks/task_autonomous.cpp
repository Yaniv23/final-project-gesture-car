/**
 * @file task_autonomous.cpp
 * @brief Autonomous navigation task - STUB (not implemented)
 * @details This task is a placeholder for future autonomous mode implementation.
 *          Currently does nothing - the vehicle just stops when in autonomous mode.
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../communication/command_protocol.h"
#include "../control/mode_manager.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_autonomous(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(AUTONOMOUS_TASK_PERIOD_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    // One-time message when entering autonomous mode
    bool message_printed = false;
    
    while (1) {
        // Check if we're in autonomous mode
        if (mode_mgr.isAutonomousMode()) {
            if (!message_printed) {
                Serial.println("[AUTO] Autonomous mode active - NOT IMPLEMENTED");
                Serial.println("[AUTO] Vehicle will remain stopped.");
                message_printed = true;
                
                // Send stop command to ensure vehicle doesn't move
                uint8_t cmd = CMD_STOP;
                xQueueSend(xCommandQueue, &cmd, 0);
            }
        } else {
            // Reset flag when leaving autonomous mode
            message_printed = false;
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

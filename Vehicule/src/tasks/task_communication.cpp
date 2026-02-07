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

extern volatile bool setupComplete;

void task_communication(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION);
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESPNowRawMessage raw_msg;

    while (1) {
        if (xQueueReceive(xESPNowQueue, &raw_msg, pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION))) {
            if (raw_msg.length >= 1) {
                uint8_t first_byte = raw_msg.first_byte;

                if (first_byte == CMD_HANDSHAKE_INIT) {
                    espnow_send_handshake_ack(raw_msg.second_byte);
                    ModeManager& mode_mgr = ModeManager::getInstance();
                    uint8_t current_mode = (mode_mgr.isAutonomousMode()) ? 1 : 0;
                    espnow_send_mode_status(current_mode);
                } else if (first_byte == CMD_HEARTBEAT) {
                } else {
                    if (isValidCommand(first_byte)) {
                        uint8_t cmd_byte = first_byte;
                        if (xQueueSend(xCommandQueue, &cmd_byte, 0) != pdTRUE) {
                        }
                    } else if (first_byte == CMD_MODE_MANUAL ||
                               first_byte == CMD_MODE_AUTONOMOUS ||
                               first_byte == CMD_MODE_TOGGLE) {
                        uint8_t cmd_byte = first_byte;
                        xQueueSend(xCommandQueue, &cmd_byte, 0);
                    } else {
                    }
                }
            }
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

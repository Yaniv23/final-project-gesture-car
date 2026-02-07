/**
 * @file task_motor_control.cpp
 * @brief Motor control task - 50ms period, Priority 4
 * @details Reads binary commands and executes motion functions
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../drivers/motor_driver.h"
#include "../control/motion_control.h"
#include "../control/mode_manager.h"
#include "../communication/command_protocol.h"
#include "../communication/espnow_handler.h"

extern MotorDriver motor_driver;
extern volatile bool setupComplete;

void task_motor_control(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL);
    TickType_t lastWakeTime = xTaskGetTickCount();
    uint8_t last_logged_cmd = 0xFF;

    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    motion_init(&motor_driver);
    ModeManager& mode_mgr = ModeManager::getInstance();

    uint8_t cmd_byte = 0;

    while (1) {
        if (xQueueReceive(xCommandQueue, &cmd_byte, pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL))) {
            if (cmd_byte == CMD_MODE_MANUAL) {
                mode_mgr.setMode(MODE_MANUAL);
                motion_stop();
                espnow_send_mode_status(0);
                last_logged_cmd = cmd_byte;
                continue;
            } else if (cmd_byte == CMD_MODE_AUTONOMOUS) {
                mode_mgr.setMode(MODE_AUTONOMOUS);
                motion_stop();
                espnow_send_mode_status(1);
                continue;
            } else if (cmd_byte == CMD_MODE_TOGGLE) {
                DrivingMode new_mode = mode_mgr.isManualMode() ? MODE_AUTONOMOUS : MODE_MANUAL;
                mode_mgr.setMode(new_mode);
                motion_stop();
                espnow_send_mode_status(new_mode == MODE_MANUAL ? 0 : 1);
                last_logged_cmd = cmd_byte;
                continue;
            }

            if (cmd_byte >= CMD_STOP && cmd_byte <= CMD_PIVOT_RIGHT) {
            }

            if (xSafetySemaphore != NULL &&
                xSemaphoreTake(xSafetySemaphore, 0) == pdTRUE) {
                switch (cmd_byte) {
                case CMD_STOP:
                    motion_stop();
                    break;
                case CMD_FORWARD:
                    motion_forward();
                    break;
                case CMD_BACKWARD:
                    motion_backward();
                    break;
                case CMD_SIDEWAY_LEFT:
                    motion_sideway_left();
                    break;
                case CMD_SIDEWAY_RIGHT:
                    motion_sideway_right();
                    break;
                case CMD_ROTATE_CW:
                    motion_rotate_cw();
                    break;
                case CMD_ROTATE_CCW:
                    motion_rotate_ccw();
                    break;
                case CMD_DIAGONAL_315:
                    motion_diagonal_315();
                    break;
                case CMD_DIAGONAL_45:
                    motion_diagonal_45();
                    break;
                case CMD_DIAGONAL_225:
                    motion_diagonal_225();
                    break;
                case CMD_DIAGONAL_135:
                    motion_diagonal_135();
                    break;
                case CMD_PIVOT_LEFT:
                    motion_pivot_left();
                    break;
                case CMD_PIVOT_RIGHT:
                    motion_pivot_right();
                    break;
                default:
                    motion_stop();
                    break;
                }

                xSemaphoreGive(xSafetySemaphore);
            } else {
                motion_stop();
            }
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

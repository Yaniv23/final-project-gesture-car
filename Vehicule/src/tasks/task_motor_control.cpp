/**
 * @file task_motor_control.cpp
 * @brief Motor control task - 50ms period, Priority 4
 * @details Reads binary commands and executes motion functions
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../drivers/motor_driver.h"
#include "../control/motion_control.h"
#include "../communication/command_protocol.h"

// Global motor driver instance (defined in main.cpp)
extern MotorDriver motor_driver;

void task_motor_control(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(50);  // 50ms period
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_MOTOR] Motor control task started");
    
    // Initialize motion control with MotorDriver
    motion_init(&motor_driver);
    
    uint8_t cmd_byte = 0;
    
    while (1) {
        // Try to get command from queue
        if (xQueueReceive(xCommandQueue, &cmd_byte, pdMS_TO_TICKS(100))) {
            // Execute command based on byte value
            switch (cmd_byte) {
                case CMD_STOP:
                    motion_stop();
                    Serial.println("[MOTOR] STOP");
                    break;
                case CMD_FORWARD:
                    motion_forward();
                    Serial.println("[MOTOR] FORWARD");
                    break;
                case CMD_BACKWARD:
                    motion_backward();
                    Serial.println("[MOTOR] BACKWARD");
                    break;
                case CMD_STRAFE_LEFT:
                    motion_strafe_left();
                    Serial.println("[MOTOR] STRAFE_LEFT");
                    break;
                case CMD_STRAFE_RIGHT:
                    motion_strafe_right();
                    Serial.println("[MOTOR] STRAFE_RIGHT");
                    break;
                case CMD_ROTATE_CW:
                    motion_rotate_cw();
                    Serial.println("[MOTOR] ROTATE_CW");
                    break;
                case CMD_ROTATE_CCW:
                    motion_rotate_ccw();
                    Serial.println("[MOTOR] ROTATE_CCW");
                    break;
                case CMD_DIAGONAL_FORWARD_LEFT:
                    motion_diagonal_forward_left();
                    Serial.println("[MOTOR] DIAGONAL_FORWARD_LEFT");
                    break;
                case CMD_DIAGONAL_FORWARD_RIGHT:
                    motion_diagonal_forward_right();
                    Serial.println("[MOTOR] DIAGONAL_FORWARD_RIGHT");
                    break;
                case CMD_DIAGONAL_BACKWARD_LEFT:
                    motion_diagonal_backward_left();
                    Serial.println("[MOTOR] DIAGONAL_BACKWARD_LEFT");
                    break;
                case CMD_DIAGONAL_BACKWARD_RIGHT:
                    motion_diagonal_backward_right();
                    Serial.println("[MOTOR] DIAGONAL_BACKWARD_RIGHT");
                    break;
                case CMD_PIVOT_LEFT:
                    motion_pivot_left();
                    Serial.println("[MOTOR] PIVOT_LEFT");
                    break;
                case CMD_PIVOT_RIGHT:
                    motion_pivot_right();
                    Serial.println("[MOTOR] PIVOT_RIGHT");
                    break;
                default:
                    Serial.print("[MOTOR] Unknown command byte: 0x");
                    Serial.println(cmd_byte, HEX);
                    motion_stop();  // Safety: stop on unknown command
                    break;
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

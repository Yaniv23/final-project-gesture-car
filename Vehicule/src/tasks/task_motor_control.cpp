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
#include "../safety/emergency_stop.h"

// Global motor driver instance (defined in main.cpp)
extern MotorDriver motor_driver;

void task_motor_control(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL);  // Use config value
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_MOTOR] Motor control task started");
    
    // Initialize motion control with MotorDriver
    motion_init(&motor_driver);
    
    uint8_t cmd_byte = 0;
    
    while (1) {
        // CRITICAL: Check emergency stop flag FIRST (before any command processing)
        // This guarantees immediate stop even if semaphore is held by another task
        // The flag is volatile and checked every loop iteration for guaranteed response
        if (emergency_stop_is_active()) {
            // Emergency stop is active - ensure motors are stopped
            // This is called every loop iteration to guarantee motors stay stopped
            motion_stop();
            
            // Only process STOP command to acknowledge (emergency stop must be cleared separately)
            // All other commands are blocked
            if (xQueueReceive(xCommandQueue, &cmd_byte, 0) == pdTRUE) {
                if (cmd_byte == CMD_STOP) {
                    // STOP command received - motors already stopped
                    Serial.println("[MOTOR] STOP (Emergency stop active - use emergency_stop_clear() to resume)");
                } else {
                    // Block all other commands during emergency stop
                    Serial.println("[MOTOR] Command blocked - Emergency stop active!");
                }
            }
            
            // Continue to next iteration (don't execute any motor commands)
            // Motors will be stopped again on next iteration if flag still active
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Emergency stop is NOT active - proceed with normal command processing
        // Try to get command from queue
        if (xQueueReceive(xCommandQueue, &cmd_byte, pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL))) {
            // Double-check emergency stop flag (race condition protection)
            if (emergency_stop_is_active()) {
                motion_stop();
                Serial.println("[MOTOR] Emergency stop detected - Command cancelled");
                vTaskDelayUntil(&lastWakeTime, period);
                continue;
            }
              
            // Check safety semaphore (secondary safety mechanism)
            // Semaphore is given initially (safe state)
            // If we CAN take semaphore, system is safe
            if (xSafetySemaphore != NULL && 
                xSemaphoreTake(xSafetySemaphore, 0) == pdTRUE) {
                // System is safe - execute command
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
                
                // Return semaphore after command execution
                xSemaphoreGive(xSafetySemaphore);
            } else {
                // Semaphore not available (shouldn't happen if flag check passed)
                // This is a secondary safety - block command execution
                Serial.println("[MOTOR] Warning: Safety semaphore unavailable - Command blocked");
                motion_stop();  // Safety: stop motors
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

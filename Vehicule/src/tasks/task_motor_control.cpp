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

// Global motor driver instance (defined in main.cpp)
extern MotorDriver motor_driver;

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_motor_control(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL);  // Use config value
    TickType_t lastWakeTime = xTaskGetTickCount();
    uint8_t last_logged_cmd = 0xFF;  // Track last logged command to avoid duplicate prints
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    // Initialize motion control with MotorDriver
    motion_init(&motor_driver);
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    uint8_t cmd_byte = 0;
    
    while (1) {
        // Try to get command from queue
        if (xQueueReceive(xCommandQueue, &cmd_byte, pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL))) {
            // Decide if we need to log this command (only when it changes)
            bool shouldLog = (cmd_byte != last_logged_cmd);
            
            // Handle mode control commands first
            if (cmd_byte == CMD_MODE_MANUAL) {
                mode_mgr.setMode(MODE_MANUAL);
                motion_stop();  // Stop motors when changing mode
                Serial.println("[MOTOR] Mode: MANUAL");
                last_logged_cmd = cmd_byte;
                continue;
            } else if (cmd_byte == CMD_MODE_AUTONOMOUS) {
                mode_mgr.setMode(MODE_AUTONOMOUS);
                motion_stop();  // Stop motors when changing mode
                Serial.println("[MOTOR] Mode: AUTONOMOUS");
                last_logged_cmd = cmd_byte;
                continue;
            } else if (cmd_byte == CMD_MODE_TOGGLE) {
                DrivingMode new_mode = mode_mgr.isManualMode() ? MODE_AUTONOMOUS : MODE_MANUAL;
                mode_mgr.setMode(new_mode);
                motion_stop();  // Stop motors when changing mode
                Serial.print("[MOTOR] Mode: ");
                Serial.println(new_mode == MODE_MANUAL ? "MANUAL" : "AUTONOMOUS");
                last_logged_cmd = cmd_byte;
                continue;
            }
            
            // Check if manual commands should be ignored in autonomous mode
            // Motion commands (CMD_STOP to CMD_PIVOT_RIGHT) work in both modes
            // but manual commands from ESP-NOW should be ignored in autonomous mode
            if (cmd_byte >= CMD_STOP && cmd_byte <= CMD_PIVOT_RIGHT) {
                // In autonomous mode, ignore manual commands (they come from ESP-NOW)
                // Autonomous task sends its own commands which should be processed
                // We can't distinguish source, so we allow all motion commands
                // The autonomous task only runs when in autonomous mode anyway
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
                    if (shouldLog) Serial.println("[MOTOR] STOP");
                    break;
                case CMD_FORWARD:
                    motion_forward();
                    if (shouldLog) Serial.println("[MOTOR] FORWARD");
                    break;
                case CMD_BACKWARD:
                    motion_backward();
                    if (shouldLog) Serial.println("[MOTOR] BACKWARD");
                    break;
                case CMD_SIDEWAY_LEFT:
                    motion_sideway_left();
                    if (shouldLog) Serial.println("[MOTOR] SIDEWAY_LEFT");
                    break;
                case CMD_SIDEWAY_RIGHT:
                    motion_sideway_right();
                    if (shouldLog) Serial.println("[MOTOR] SIDEWAY_RIGHT");
                    break;
                case CMD_ROTATE_CW:
                    motion_rotate_cw();
                    if (shouldLog) Serial.println("[MOTOR] ROTATE_CW");
                    break;
                case CMD_ROTATE_CCW:
                    motion_rotate_ccw();
                    if (shouldLog) Serial.println("[MOTOR] ROTATE_CCW");
                    break;
                case CMD_DIAGONAL_315:
                    motion_diagonal_315();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_315");
                    break;
                case CMD_DIAGONAL_45:
                    motion_diagonal_45();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_45");
                    break;
                case CMD_DIAGONAL_225:
                    motion_diagonal_225();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_225");
                    break;
                case CMD_DIAGONAL_135:
                    motion_diagonal_135();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_135");
                    break;
                case CMD_PIVOT_LEFT:
                    motion_pivot_left();
                    if (shouldLog) Serial.println("[MOTOR] PIVOT_LEFT");
                    break;
                case CMD_PIVOT_RIGHT:
                    motion_pivot_right();
                    if (shouldLog) Serial.println("[MOTOR] PIVOT_RIGHT");
                    break;
                default:
                    if (shouldLog) {
                        Serial.print("[MOTOR] Unknown command byte: 0x");
                        Serial.println(cmd_byte, HEX);
                    }
                    motion_stop();  // Safety: stop on unknown command
                    break;
                }
                
                // Update last logged command after successful handling
                last_logged_cmd = cmd_byte;
                
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

/**
 * @file task_autonomous.cpp
 * @brief Autonomous navigation task - 50ms period, Priority 3
 * @details Simple obstacle avoidance using front ultrasonic sensor
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../drivers/ultrasonic_driver.h"
#include "../communication/command_protocol.h"
#include "../control/mode_manager.h"

// Ultrasonic sensor instance for autonomous mode
static Ultrasonic ultrasonic_front;

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_autonomous(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(AUTONOMOUS_TASK_PERIOD_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    // Lazy initialization: sensor will be initialized only when entering autonomous mode
    bool sensor_initialized = false;
    
    // Navigation state machine
    enum NavigationState {
        STATE_FORWARD,
        STATE_TURN_LEFT,
        STATE_TURN_RIGHT,
        STATE_STOP
    };
    NavigationState state = STATE_FORWARD;
    
    // Turn duration counter
    uint32_t turn_duration_ms = 0;
    
    while (1) {
        // Check if we're in autonomous mode
        if (!mode_mgr.isAutonomousMode()) {
            // Not in autonomous mode, just wait
            // Reset sensor initialization flag when leaving autonomous mode
            if (sensor_initialized) {
                sensor_initialized = false;
                Serial.println("[AUTO] Left autonomous mode - sensor deactivated");
            }
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Lazy initialization: initialize sensor only when entering autonomous mode
        if (!sensor_initialized) {
            bool sensor_ok = ultrasonic_front.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
            if (!sensor_ok) {
                Serial.println("[AUTO] ERROR: Failed to initialize front ultrasonic sensor!");
                // Task will continue but won't be able to read sensor
            } else {
                Serial.println("[AUTO] Entered autonomous mode - Front ultrasonic sensor initialized");
            }
            sensor_initialized = true;
        }
        
        // Read front sensor
        float front_distance = ultrasonic_front.readDistanceCM();
        
        // Default command
        uint8_t command = CMD_STOP;
        
        // Navigation algorithm
        if (front_distance > 0.0f && front_distance < OBSTACLE_DISTANCE_THRESHOLD_CM) {
            // Obstacle detected ahead
            if (state == STATE_FORWARD) {
                // Stop and start turning
                command = CMD_STOP;
                state = STATE_TURN_RIGHT;  // Default: turn right
                turn_duration_ms = 0;
            } else if (state == STATE_TURN_RIGHT || state == STATE_TURN_LEFT) {
                // Currently turning
                turn_duration_ms += AUTONOMOUS_TASK_PERIOD_MS;
                if (turn_duration_ms < TURN_DURATION_MS) {
                    // Continue turning
                    command = (state == STATE_TURN_RIGHT) ? CMD_ROTATE_CW : CMD_ROTATE_CCW;
                } else {
                    // Turn duration completed, check again
                    float new_distance = ultrasonic_front.readDistanceCM();
                    if (new_distance > 0.0f && new_distance < OBSTACLE_DISTANCE_THRESHOLD_CM) {
                        // Still obstacle, try other direction
                        state = (state == STATE_TURN_RIGHT) ? STATE_TURN_LEFT : STATE_TURN_RIGHT;
                        command = (state == STATE_TURN_RIGHT) ? CMD_ROTATE_CW : CMD_ROTATE_CCW;
                        turn_duration_ms = 0;
                    } else {
                        // Obstacle cleared, resume forward
                        state = STATE_FORWARD;
                        command = CMD_FORWARD;
                        turn_duration_ms = 0;
                    }
                }
            }
        } else {
            // No obstacle detected - move forward
            if (state != STATE_FORWARD) {
                state = STATE_FORWARD;
                turn_duration_ms = 0;
            }
            command = CMD_FORWARD;
        }
        
        // Handle sensor error (distance < 0 or == 0 when it should be valid)
        if (front_distance < 0.0f) {
            // Sensor error - stop for safety
            command = CMD_STOP;
            Serial.println("[AUTO] Warning: Sensor error detected, stopping");
        }
        
        // Send command to motor control queue
        if (xQueueSend(xCommandQueue, &command, 0) != pdTRUE) {
            Serial.println("[AUTO] Warning: Command queue full!");
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

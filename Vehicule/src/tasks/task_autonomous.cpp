/**
 * @file task_autonomous.cpp
 * @brief Autonomous navigation task - 50ms period, Priority 3
 * @details Enhanced obstacle avoidance using servo-mounted ultrasonic sensor
 *          Scans left, center, and right to make intelligent navigation decisions
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../drivers/ultrasonic_driver.h"
#include "../drivers/servo_driver.h"
#include "../communication/command_protocol.h"
#include "../control/mode_manager.h"

// Sensor instances for autonomous mode
static Ultrasonic ultrasonic_sensor;
static ServoDriver servo;

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

// Scanning angles (degrees)
#define SCAN_LEFT_ANGLE    30
#define SCAN_CENTER_ANGLE  0
#define SCAN_RIGHT_ANGLE   30
#define SERVO_MOVE_DELAY_MS 150  // Time to wait for servo to reach position

// Navigation state machine
enum NavigationState {
    STATE_SCANNING,      // Scanning environment
    STATE_FORWARD,        // Moving forward
    STATE_TURNING,        // Turning to avoid obstacle
    STATE_STOPPED         // Stopped (safety)
};

// Scanning state
enum ScanState {
    SCAN_GOING_TO_LEFT,
    SCAN_MEASURING_LEFT,
    SCAN_GOING_TO_CENTER,
    SCAN_MEASURING_CENTER,
    SCAN_GOING_TO_RIGHT,
    SCAN_MEASURING_RIGHT,
    SCAN_DECIDING
};

void task_autonomous(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(AUTONOMOUS_TASK_PERIOD_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    // Lazy initialization: sensors will be initialized only when entering autonomous mode
    bool sensors_initialized = false;
    
    // Navigation state
    NavigationState nav_state = STATE_SCANNING;
    ScanState scan_state = SCAN_GOING_TO_CENTER;
    
    // Distance measurements
    float distance_left = -1.0f;
    float distance_center = -1.0f;
    float distance_right = -1.0f;
    
    // Timing for servo movement and scanning
    unsigned long servo_move_start_ms = 0;
    bool servo_moving = false;
    
    // Turn duration counter
    uint32_t turn_duration_ms = 0;
    uint8_t last_command = CMD_STOP;
    
    while (1) {
        // Check if we're in autonomous mode
        if (!mode_mgr.isAutonomousMode()) {
            // Not in autonomous mode, just wait
            // Reset sensor initialization flag when leaving autonomous mode
            if (sensors_initialized) {
                sensors_initialized = false;
                servo.stopSweep();
                Serial.println("[AUTO] Left autonomous mode - sensors deactivated");
            }
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Lazy initialization: initialize sensors only when entering autonomous mode
        if (!sensors_initialized) {
            bool sensor_ok = ultrasonic_sensor.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
            bool servo_ok = servo.init(SERVO_PIN);
            
            if (!sensor_ok || !servo_ok) {
                Serial.print("[AUTO] ERROR: Failed to initialize sensors! Sensor: ");
                Serial.print(sensor_ok ? "OK" : "FAIL");
                Serial.print(", Servo: ");
                Serial.println(servo_ok ? "OK" : "FAIL");
            } else {
                Serial.println("[AUTO] Entered autonomous mode - Sensors initialized");
                Serial.println("[AUTO] Starting environment scan...");
                // Reset navigation state to initial state
                nav_state = STATE_SCANNING;
                scan_state = SCAN_GOING_TO_CENTER;
                // Reset distance measurements
                distance_left = -1.0f;
                distance_center = -1.0f;
                distance_right = -1.0f;
                // Reset timing variables
                turn_duration_ms = 0;
                last_command = CMD_STOP;
                // Start at center position
                servo.setAngle(SCAN_CENTER_ANGLE);
                servo_move_start_ms = millis();
                servo_moving = true;
            }
            sensors_initialized = true;
        }
        
        // Default command
        uint8_t command = CMD_STOP;
        
        // Navigation state machine
        switch (nav_state) {
            case STATE_SCANNING: {
                // Scan environment: left, center, right
                unsigned long now = millis();
                
                switch (scan_state) {
                    case SCAN_GOING_TO_LEFT:
                        if (!servo_moving) {
                            servo.setAngle(SCAN_LEFT_ANGLE);
                            servo_move_start_ms = now;
                            servo_moving = true;
                        } else if (now - servo_move_start_ms >= SERVO_MOVE_DELAY_MS) {
                            scan_state = SCAN_MEASURING_LEFT;
                            servo_moving = false;
                        }
                        command = CMD_STOP;  // Stop while scanning
                        break;
                        
                    case SCAN_MEASURING_LEFT:
                        distance_left = ultrasonic_sensor.readDistanceCM();
                        if (distance_left < 0.0f) {
                            distance_left = 0.0f;  // Treat error as obstacle
                        }
                        Serial.print("[AUTO] Left distance: ");
                        Serial.print(distance_left);
                        Serial.println(" cm");
                        scan_state = SCAN_GOING_TO_CENTER;
                        servo_moving = false;
                        break;
                        
                    case SCAN_GOING_TO_CENTER:
                        if (!servo_moving) {
                            servo.setAngle(SCAN_CENTER_ANGLE);
                            servo_move_start_ms = now;
                            servo_moving = true;
                        } else if (now - servo_move_start_ms >= SERVO_MOVE_DELAY_MS) {
                            scan_state = SCAN_MEASURING_CENTER;
                            servo_moving = false;
                        }
                        command = CMD_STOP;
                        break;
                        
                    case SCAN_MEASURING_CENTER:
                        distance_center = ultrasonic_sensor.readDistanceCM();
                        if (distance_center < 0.0f) {
                            distance_center = 0.0f;
                        }
                        Serial.print("[AUTO] Center distance: ");
                        Serial.print(distance_center);
                        Serial.println(" cm");
                        scan_state = SCAN_GOING_TO_RIGHT;
                        servo_moving = false;
                        break;
                        
                    case SCAN_GOING_TO_RIGHT:
                        if (!servo_moving) {
                            servo.setAngle(SCAN_RIGHT_ANGLE);
                            servo_move_start_ms = now;
                            servo_moving = true;
                        } else if (now - servo_move_start_ms >= SERVO_MOVE_DELAY_MS) {
                            scan_state = SCAN_MEASURING_RIGHT;
                            servo_moving = false;
                        }
                        command = CMD_STOP;
                        break;
                        
                    case SCAN_MEASURING_RIGHT:
                        distance_right = ultrasonic_sensor.readDistanceCM();
                        if (distance_right < 0.0f) {
                            distance_right = 0.0f;
                        }
                        Serial.print("[AUTO] Right distance: ");
                        Serial.print(distance_right);
                        Serial.println(" cm");
                        scan_state = SCAN_DECIDING;
                        servo_moving = false;
                        break;
                        
                    case SCAN_DECIDING: {
                        // Decision logic: choose best direction
                        // Find the direction with maximum clear distance
                        float max_distance = distance_center;
                        uint8_t best_direction = 0;  // 0=center, 1=left, 2=right
                        
                        if (distance_left > max_distance) {
                            max_distance = distance_left;
                            best_direction = 1;
                        }
                        if (distance_right > max_distance) {
                            max_distance = distance_right;
                            best_direction = 2;
                        }
                        
                        Serial.print("[AUTO] Best direction: ");
                        Serial.print(best_direction == 0 ? "CENTER" : (best_direction == 1 ? "LEFT" : "RIGHT"));
                        Serial.print(" (distance: ");
                        Serial.print(max_distance);
                        Serial.println(" cm)");
                        
                        // Decision: if center is clear enough, go forward
                        // Otherwise, turn toward the best direction
                        if (distance_center >= OBSTACLE_DISTANCE_THRESHOLD_CM) {
                            // Center is clear - go forward
                            nav_state = STATE_FORWARD;
                            command = CMD_FORWARD;
                            Serial.println("[AUTO] Decision: FORWARD");
                        } else {
                            // Center blocked - turn toward best direction
                            nav_state = STATE_TURNING;
                            if (best_direction == 1) {
                                // Left is better
                                command = CMD_ROTATE_CCW;
                                Serial.println("[AUTO] Decision: TURN LEFT");
                            } else if (best_direction == 2) {
                                // Right is better
                                command = CMD_ROTATE_CW;
                                Serial.println("[AUTO] Decision: TURN RIGHT");
                            } else {
                                // All directions blocked - default to right
                                command = CMD_ROTATE_CW;
                                Serial.println("[AUTO] Decision: ALL BLOCKED - TURN RIGHT");
                            }
                            turn_duration_ms = 0;
                        }
                        
                        // Reset scan state for next cycle
                        scan_state = SCAN_GOING_TO_CENTER;
                        servo.setAngle(SCAN_CENTER_ANGLE);  // Return to center
                        break;
                    }
                }
                break;
            }
            
            case STATE_FORWARD: {
                // Moving forward - periodically check for obstacles
                // Use center reading (servo should be at center)
                float current_distance = ultrasonic_sensor.readDistanceCM();
                
                if (current_distance > 0.0f && current_distance < OBSTACLE_DISTANCE_THRESHOLD_CM) {
                    // Obstacle detected - stop and scan
                    nav_state = STATE_SCANNING;
                    command = CMD_STOP;
                    scan_state = SCAN_GOING_TO_LEFT;
                    Serial.println("[AUTO] Obstacle detected - stopping to scan");
                } else if (current_distance < 0.0f) {
                    // Sensor error - stop for safety
                    nav_state = STATE_STOPPED;
                    command = CMD_STOP;
                    Serial.println("[AUTO] ERROR: Sensor error - stopping");
                } else {
                    // Clear ahead - continue forward
                    command = CMD_FORWARD;
                    
                    // Periodically rescan (every 2 seconds of forward movement)
                    static uint32_t forward_time_ms = 0;
                    forward_time_ms += AUTONOMOUS_TASK_PERIOD_MS;
                    if (forward_time_ms >= 2000) {
                        // Time to rescan
                        nav_state = STATE_SCANNING;
                        scan_state = SCAN_GOING_TO_LEFT;
                        command = CMD_STOP;
                        forward_time_ms = 0;
                        Serial.println("[AUTO] Periodic rescan");
                    }
                }
                break;
            }
            
            case STATE_TURNING: {
                // Turning to avoid obstacle
                turn_duration_ms += AUTONOMOUS_TASK_PERIOD_MS;
                
                if (turn_duration_ms < TURN_DURATION_MS) {
                    // Continue turning
                    command = last_command;  // Keep same turn direction
                } else {
                    // Turn completed - scan again
                    nav_state = STATE_SCANNING;
                    scan_state = SCAN_GOING_TO_LEFT;
                    command = CMD_STOP;
                    turn_duration_ms = 0;
                    Serial.println("[AUTO] Turn completed - rescanning");
                }
                break;
            }
            
            case STATE_STOPPED: {
                // Stopped due to error - wait and try to recover
                command = CMD_STOP;
                static uint32_t stop_time_ms = 0;
                stop_time_ms += AUTONOMOUS_TASK_PERIOD_MS;
                if (stop_time_ms >= 1000) {
                    // Try to recover after 1 second
                    nav_state = STATE_SCANNING;
                    scan_state = SCAN_GOING_TO_CENTER;
                    stop_time_ms = 0;
                    Serial.println("[AUTO] Attempting recovery");
                }
                break;
            }
        }
        
        // Store last command for turning state
        if (command == CMD_ROTATE_CW || command == CMD_ROTATE_CCW) {
            last_command = command;
        }
        
        // Send command to motor control queue
        if (xQueueSend(xCommandQueue, &command, 0) != pdTRUE) {
            Serial.println("[AUTO] Warning: Command queue full!");
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

/**
 * @file task_autonomous.cpp
 * @brief Autonomous navigation task - 50ms period, Priority 3
 * @details Orchestrates navigation modules for autonomous obstacle avoidance
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

// Navigation modules
#include "../navigation/navigation_state_machine.h"
#include "../obstacle_detection/obstacle_scanner.h"
#include "../obstacle_detection/stuck_detector.h"
#include "../obstacle_detection/position_tracker.h"
#include "../recovery/recovery_strategies.h"

// Sensor instances for autonomous mode
static Ultrasonic ultrasonic_sensor;
static ServoDriver servo;

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

// Navigation modules (lazy initialization)
static ObstacleScanner* scanner = nullptr;
static PositionTracker* position_tracker = nullptr;
static StuckDetector* stuck_detector = nullptr;
static RecoveryStrategies* recovery = nullptr;
static NavigationStateMachine* nav_state_machine = nullptr;

void task_autonomous(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(AUTONOMOUS_TASK_PERIOD_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    // Lazy initialization flag
    bool sensors_initialized = false;
    
    while (1) {
        // Check if we're in autonomous mode
        if (!mode_mgr.isAutonomousMode()) {
            // Not in autonomous mode - deinitialize if needed
            if (sensors_initialized) {
                // Cleanup modules
                delete nav_state_machine;
                delete recovery;
                delete stuck_detector;
                delete position_tracker;
                delete scanner;
                
                nav_state_machine = nullptr;
                recovery = nullptr;
                stuck_detector = nullptr;
                position_tracker = nullptr;
                scanner = nullptr;
                
                servo.stopSweep();
                sensors_initialized = false;
                Serial.println("[AUTO] Left autonomous mode - modules deactivated");
            }
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Lazy initialization: initialize only when entering autonomous mode
        if (!sensors_initialized) {
            // Initialize sensors
            bool sensor_ok = ultrasonic_sensor.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
            bool servo_ok = servo.init(SERVO_PIN);
            
            if (!sensor_ok || !servo_ok) {
                Serial.print("[AUTO] ERROR: Failed to initialize sensors! Sensor: ");
                Serial.print(sensor_ok ? "OK" : "FAIL");
                Serial.print(", Servo: ");
                Serial.println(servo_ok ? "OK" : "FAIL");
            } else {
                Serial.println("[AUTO] Entered autonomous mode - Initializing modules...");
                
                // Initialize modules in dependency order
                scanner = new ObstacleScanner(ultrasonic_sensor, servo);
                position_tracker = new PositionTracker();
                stuck_detector = new StuckDetector(*position_tracker);
                recovery = new RecoveryStrategies();
                nav_state_machine = new NavigationStateMachine(*scanner, *stuck_detector, *recovery, *position_tracker);
                
                if (!scanner->init()) {
                    Serial.println("[AUTO] ERROR: Failed to initialize ObstacleScanner!");
                } else {
                    // Reset all modules
                    scanner->reset();
                    position_tracker->reset();
                    stuck_detector->reset();
                    recovery->reset();
                    nav_state_machine->reset();
                    
                    // Set servo to center position
                    scanner->setServoToCenter();
                    
                    sensors_initialized = true;
                    Serial.println("[AUTO] All modules initialized - ready for navigation");
                }
            }
        }
        
        // Main navigation loop - simple orchestration
        // Double-check mode before sending commands (safety measure)
        if (sensors_initialized && nav_state_machine != nullptr && mode_mgr.isAutonomousMode()) {
            // Update navigation state machine
            uint8_t command = nav_state_machine->update();
            
            // Final check: ensure we're still in autonomous mode before sending command
            if (mode_mgr.isAutonomousMode()) {
                // Send command to motor control queue
                if (xQueueSend(xCommandQueue, &command, 0) != pdTRUE) {
                    Serial.println("[AUTO] Warning: Command queue full!");
                }
            } else {
                // Mode changed during execution - stop immediately
                Serial.println("[AUTO] Mode changed to MANUAL during execution - stopping");
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

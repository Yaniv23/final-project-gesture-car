/**
 * @file task_sensor_fusion.cpp
 * @brief Sensor fusion task - 50ms period, Priority 3
 * @details Reads sensors (ultrasonic, IMU) and fuses data
 *          Based on updateServoSensor() from Vehicule_Controller.ino
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../control/mode_manager.h"

// NOTE: Servo and ultrasonic sensor instances removed
// They should ONLY be active in autonomous mode, handled by task_autonomous.cpp
// In manual mode, they must NOT be initialized or used

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_sensor_fusion(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SENSOR_FUSION);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    // IMPORTANT: Servo and ultrasonic sensor should ONLY be active in autonomous mode
    // In manual mode, they should NOT be initialized or used
    // The autonomous task (task_autonomous.cpp) handles sensor reading in autonomous mode
    
    while (1) {
        // Check current mode
        bool is_autonomous = mode_mgr.isAutonomousMode();
        bool is_manual = mode_mgr.isManualMode();
        
        // In manual mode: servo and ultrasonic sensor should NOT be active
        // Just wait - do not initialize or use servo/sensor
        if (is_manual) {
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // In autonomous mode: the autonomous task handles sensor reading
        // This task should be idle (energy saving)
        if (is_autonomous) {
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Fallback: just wait if mode is unknown
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

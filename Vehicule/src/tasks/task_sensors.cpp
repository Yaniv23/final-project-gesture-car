/**
 * @file task_sensors.cpp
 * @brief Sensor reading task - reads ultrasonic sensors continuously
 * @details Reads front and rear sensors every 60ms in autonomous mode only
 *          Updates shared SensorState structure for use by autonomous task
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/sensor_state.h"
#include "../control/mode_manager.h"
#include "../drivers/ultrasonic_driver.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_sensors(void *pvParameters) {
    // Wait for setup to complete
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Get mode manager instance
    ModeManager& mode_mgr = ModeManager::getInstance();

    // Front sensor is shared (owned by sensor_state); rear sensor is local to this task
    Ultrasonic* front_sensor = getFrontSensor();
    Ultrasonic rear_sensor;
    bool front_ok = (front_sensor != NULL);
    bool rear_ok = rear_sensor.init(ULTRASONIC_TRIG_REAR, ULTRASONIC_ECHO_REAR);

    // Log initialization status
    if (!front_ok) {
        Serial.println("[SENSORS] Front sensor not available (shared instance)");
    }
    if (!rear_ok) {
        Serial.println("[SENSORS] Rear sensor initialization failed");
    }
    if (front_ok && rear_ok) {
        Serial.println("[SENSORS] Sensors initialized successfully");
        // Give sensors time to stabilize after initialization
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Reading interval: 60 ms (SENSOR_READ_INTERVAL_MS). This is the single source of
    // truth for distance data; task_autonomous only consumes getSensorState(), never
    // reads the sensors directly.
    const TickType_t read_interval = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);
    
    // Delay when not in autonomous mode (to avoid busy waiting)
    const TickType_t idle_delay = pdMS_TO_TICKS(100);

    bool autonomous_active_logged = false;

    while (1) {
        // Check if we are in autonomous mode
        // Task is only active in autonomous mode
        if (!mode_mgr.isAutonomousMode()) {
            autonomous_active_logged = false;
            // Not in autonomous mode - wait and check again
            vTaskDelay(idle_delay);
            continue;
        }

        // Log once when we become active in autonomous mode
        if (!autonomous_active_logged && front_ok && rear_ok) {
            autonomous_active_logged = true;
            Serial.println("[SENSORS] Autonomous mode active - reading front/rear every 60 ms");
        }

        // We are in autonomous mode - read sensors
        // Check if sensors are initialized
        if (!front_ok || !rear_ok) {
            // Sensors failed to initialize - update state with invalid values
            updateSensorState(-1.0f, -1.0f);
            vTaskDelay(read_interval);
            continue;
        }

        // Read front sensor distance (filtered value from driver)
        // Use mutex to prevent conflicts with task_autonomous which also uses front sensor
        float front_distance = -1.0f;
        SemaphoreHandle_t front_mutex = getFrontSensorMutex();
        if (front_mutex != NULL && front_sensor != NULL && xSemaphoreTake(front_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            front_distance = front_sensor->readDistanceCM();
            xSemaphoreGive(front_mutex);
        }
        
        // Add delay between sensor readings to avoid interference
        // HC-SR04 sensors can interfere with each other if triggered too close together
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms delay between front and rear readings
        
        // Read rear sensor distance (filtered value from driver)
        float rear_distance = rear_sensor.readDistanceCM();

        // Debug: Log sensor readings periodically (every 10 readings = ~600ms)
        static int debug_counter = 0;
        if (++debug_counter >= 10) {
            debug_counter = 0;
            Serial.printf("[SENSORS] Front: %.1f cm, Rear: %.1f cm\n", front_distance, rear_distance);
        }

        // Update shared sensor state (thread-safe)
        if (!updateSensorState(front_distance, rear_distance)) {
            // Failed to update state (mutex issue - should not happen)
            Serial.println("[SENSORS] Warning: Failed to update sensor state");
        }

        // Wait before next reading
        vTaskDelay(read_interval);
    }
}

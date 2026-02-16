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

extern volatile bool setupComplete;

void task_sensors(void *pvParameters) {
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ModeManager& mode_mgr = ModeManager::getInstance();

    Ultrasonic* front_sensor = getFrontSensor();
    Ultrasonic rear_sensor;
    bool front_ok = (front_sensor != NULL);
    bool rear_ok = rear_sensor.init(ULTRASONIC_TRIG_REAR, ULTRASONIC_ECHO_REAR);

    if (front_ok && rear_ok) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Give sensors time to stabilize
    }

    // Single source of truth for distance; task_autonomous consumes getSensorState() only
    const TickType_t read_interval = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);
    const TickType_t idle_delay = pdMS_TO_TICKS(100);

    bool autonomous_active_logged = false;

    while (1) {
        if (!mode_mgr.isAutonomousMode()) {
            autonomous_active_logged = false;
            vTaskDelay(idle_delay);
            continue;
        }

        if (!autonomous_active_logged && front_ok && rear_ok) {
            autonomous_active_logged = true;
        }

        if (!front_ok || !rear_ok) {
            updateSensorState(-1.0f, -1.0f);
            vTaskDelay(read_interval);
            continue;
        }

        float front_distance = -1.0f;
        SemaphoreHandle_t front_mutex = getFrontSensorMutex();
        if (front_mutex != NULL && front_sensor != NULL && xSemaphoreTake(front_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            front_distance = front_sensor->readDistanceCM();
            xSemaphoreGive(front_mutex);
        }

        // 20ms between front and rear to avoid HC-SR04 interference
        vTaskDelay(pdMS_TO_TICKS(20));

        float rear_distance = rear_sensor.readDistanceCM();

        updateSensorState(front_distance, rear_distance);

        vTaskDelay(read_interval);
    }
}

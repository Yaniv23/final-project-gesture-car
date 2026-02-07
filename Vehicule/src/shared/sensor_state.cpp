/**
 * @file sensor_state.cpp
 * @brief Implementation of shared sensor state with thread-safe access
 */

#include "sensor_state.h"
#include "config.h"
#include "drivers/ultrasonic_driver.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static SensorState sensor_state = {
    .front_distance = -1.0f,
    .rear_distance = -1.0f,
    .timestamp = 0
};
static SemaphoreHandle_t sensor_state_mutex = NULL;
static SemaphoreHandle_t front_sensor_mutex = NULL;
static Ultrasonic front_sensor;

bool initSensorState() {
    sensor_state_mutex = xSemaphoreCreateMutex();
    if (sensor_state_mutex == NULL) {
        Serial.println("[ERROR] SensorState: Failed to create mutex");
        return false;
    }

    front_sensor_mutex = xSemaphoreCreateMutex();
    if (front_sensor_mutex == NULL) {
        Serial.println("[ERROR] SensorState: Failed to create front sensor mutex");
        return false;
    }

    if (!front_sensor.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO)) {
        Serial.println("[ERROR] SensorState: Failed to init front ultrasonic sensor");
    }

    sensor_state.front_distance = -1.0f;
    sensor_state.rear_distance = -1.0f;
    sensor_state.timestamp = xTaskGetTickCount();
    return true;
}

Ultrasonic* getFrontSensor() {
    return &front_sensor;
}

bool readFrontSensorRaw(float* out_cm) {
    if (out_cm == NULL || front_sensor_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(front_sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    *out_cm = front_sensor.readDistanceRaw();
    xSemaphoreGive(front_sensor_mutex);
    return true;
}

SemaphoreHandle_t getFrontSensorMutex() {
    return front_sensor_mutex;
}

bool getSensorState(SensorState* state) {
    if (state == NULL || sensor_state_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(sensor_state_mutex, portMAX_DELAY) == pdTRUE) {
        state->front_distance = sensor_state.front_distance;
        state->rear_distance = sensor_state.rear_distance;
        state->timestamp = sensor_state.timestamp;
        xSemaphoreGive(sensor_state_mutex);
        return true;
    }
    return false;
}

bool updateSensorState(float front_dist, float rear_dist) {
    if (sensor_state_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(sensor_state_mutex, portMAX_DELAY) == pdTRUE) {
        sensor_state.front_distance = front_dist;
        sensor_state.rear_distance = rear_dist;
        sensor_state.timestamp = xTaskGetTickCount();
        xSemaphoreGive(sensor_state_mutex);
        return true;
    }
    return false;
}

#ifndef SENSOR_STATE_H
#define SENSOR_STATE_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SensorState {
    float front_distance;
    float rear_distance;
    TickType_t timestamp;
};

bool getSensorState(SensorState* state);
bool updateSensorState(float front_dist, float rear_dist);
bool initSensorState();
SemaphoreHandle_t getFrontSensorMutex();

class Ultrasonic;
Ultrasonic* getFrontSensor();
bool readFrontSensorRaw(float* out_cm);

#endif

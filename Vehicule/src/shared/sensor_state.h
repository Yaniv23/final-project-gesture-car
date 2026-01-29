#ifndef SENSOR_STATE_H
#define SENSOR_STATE_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @file sensor_state.h
 * @brief Shared sensor state structure for inter-task communication
 * @details Stores front and rear ultrasonic sensor readings with thread-safe access
 */

/**
 * @brief Sensor state structure
 * @details Contains distance measurements from front and rear sensors
 *          Values are in centimeters, -1.0 indicates invalid/error reading
 */
struct SensorState {
    float front_distance;  // Front sensor distance in cm, -1.0 if invalid
    float rear_distance;   // Rear sensor distance in cm, -1.0 if invalid
    TickType_t timestamp;   // Last update timestamp (FreeRTOS ticks)
};

/**
 * @brief Get current sensor state (thread-safe)
 * @param state Pointer to SensorState structure to fill
 * @return true if successful, false if mutex unavailable
 */
bool getSensorState(SensorState* state);

/**
 * @brief Update sensor state (thread-safe)
 * @param front_dist Front sensor distance in cm (-1.0 if invalid)
 * @param rear_dist Rear sensor distance in cm (-1.0 if invalid)
 * @return true if successful, false if mutex unavailable
 */
bool updateSensorState(float front_dist, float rear_dist);

/**
 * @brief Initialize sensor state mutex
 * @details Must be called during system initialization
 * @return true if successful, false otherwise
 */
bool initSensorState();

/**
 * @brief Get mutex for front sensor access
 * @details Use this mutex to protect access to the front ultrasonic sensor
 *          when multiple tasks need to use it (e.g., task_sensors and task_autonomous)
 * @return SemaphoreHandle_t to the front sensor mutex, or NULL if not initialized
 */
SemaphoreHandle_t getFrontSensorMutex();

class Ultrasonic;

/**
 * @brief Get shared front ultrasonic sensor instance
 * @details Use with getFrontSensorMutex() when reading. Both task_sensors and
 *          task_autonomous use this instance.
 * @return Pointer to the front Ultrasonic instance, or NULL if not initialized
 */
Ultrasonic* getFrontSensor();

/**
 * @brief Read one raw distance from the front sensor (thread-safe)
 * @param out_cm Output: raw distance in cm, or -1.0 if error/timeout
 * @return true if read succeeded (mutex taken and released), false otherwise
 */
bool readFrontSensorRaw(float* out_cm);

#endif // SENSOR_STATE_H

#ifndef ULTRASONIC_DRIVER_H
#define ULTRASONIC_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <cstddef>
#include <array>

/**
 * @file ultrasonic_driver.h
 * @brief HC-SR04 Ultrasonic sensor driver with filtering
 * @details Based on Vehicule_Controller.ino readDistanceCM() implementation
 *          Includes moving average filter to eliminate noise and spikes
 */

class Ultrasonic {
public:
    /**
     * @brief Constructor
     */
    Ultrasonic();
    
    /**
     * @brief Initialize ultrasonic sensor
     * @param trig_pin Trigger pin (output)
     * @param echo_pin Echo pin (input)
     * @return true if successful
     */
    bool init(uint8_t trig_pin, uint8_t echo_pin);
    
    /**
     * @brief Read distance in centimeters (filtered)
     * @details Based on readDistanceCM() from Vehicule_Controller.ino
     *          Now includes moving average filter to eliminate spikes and noise
     *          Uses 5-sample buffer with spike detection (max deviation 50cm)
     * @return Filtered distance in cm, -1.0 if error or timeout
     */
    float readDistanceCM();
    
private:
    uint8_t trig_pin_;
    uint8_t echo_pin_;
    bool initialized_;
    float last_distance_;
    
    // HC-SR04 timing constants
    static constexpr unsigned long TRIG_PULSE_US = 10;      // 10 microsecond trigger pulse
    static constexpr unsigned long TRIG_SETTLE_US = 2;      // 2 microsecond settle time
    static constexpr unsigned long ECHO_TIMEOUT_US = 25000; // 25 ms timeout (~4.25 m max range)
    static constexpr float SPEED_OF_SOUND_CM_PER_US = 0.034;  // cm per microsecond
    static constexpr float DISTANCE_DIVISOR = 2.0;            // Divide by 2 (round trip)
    
    // Filter constants
    static constexpr size_t FILTER_BUFFER_SIZE = 5;          // 5-sample moving average
    static constexpr float MAX_DEVIATION_CM = 50.0f;         // Max deviation to reject spike
    
    // Filter state
    std::array<float, FILTER_BUFFER_SIZE> distance_buffer_;
    size_t buffer_index_;
    size_t valid_samples_count_;
    float last_filtered_distance_;
    
    /**
     * @brief Read raw distance from sensor hardware
     * @return Raw distance in cm, -1.0 if error or timeout
     */
    float readDistanceRawInternal();
    
    /**
     * @brief Calculate average of valid samples in buffer
     * @return Average distance in cm, -1.0 if no valid samples
     */
    float calculateAverage() const;
    
    /**
     * @brief Validate if sample is within acceptable deviation
     * @param raw Raw distance value
     * @param current_avg Current average of buffer
     * @return true if sample is valid (not a spike)
     */
    bool isValidSample(float raw, float current_avg) const;
};

#endif // ULTRASONIC_DRIVER_H


#ifndef ULTRASONIC_DRIVER_H
#define ULTRASONIC_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file ultrasonic_driver.h
 * @brief HC-SR04 Ultrasonic sensor driver
 * @details Based on Vehicule_Controller.ino readDistanceCM() implementation
 *          Reserved for future use in autonomous mode
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
     *          Now includes median filter to eliminate spikes
     * @return Filtered distance in cm, -1.0 if error or timeout
     */
    float readDistanceCM();
    
    /**
     * @brief Read raw distance without filtering (for compatibility/debugging)
     * @return Raw distance in cm, -1.0 if error or timeout
     */
    float readDistanceRaw();
    
    /**
     * @brief Check if obstacle is detected
     * @param threshold_cm Distance threshold in cm (default 20cm)
     * @return true if obstacle detected (distance < threshold and > 0)
     */
    bool isObstacle(float threshold_cm = 20.0);
    
    /**
     * @brief Get last measured distance
     * @return Last distance reading in cm
     */
    float getLastDistance() const;
    
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
    
    /**
     * @brief Read raw distance from sensor hardware
     * @return Raw distance in cm, -1.0 if error or timeout
     */
    float readDistanceRawInternal();
};

#endif // ULTRASONIC_DRIVER_H


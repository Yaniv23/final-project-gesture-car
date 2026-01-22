#ifndef SERVO_DRIVER_H
#define SERVO_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// Default rest interval between sweeps (1500ms)
#define DEFAULT_REST_INTERVAL_MS 1500

/**
 * @file servo_driver.h
 * @brief Servo motor driver for scanning/obstacle detection
 * @details Uses LEDC PWM directly (channel 4) to avoid conflicts with motors (channels 0-3)
 *          Based on Vehicule_Controller.ino servo implementation
 */

class ServoDriver {
public:
    /**
     * @brief Constructor
     */
    ServoDriver();
    
    /**
     * @brief Initialize servo on specified pin
     * @param pin GPIO pin number for servo control
     * @return true if successful, false otherwise
     */
    bool init(uint8_t pin);
    
    /**
     * @brief Set servo to specific angle
     * @param angle Angle in degrees (0-180)
     * @param wait_for_stable If true, wait for stabilization delay (default: false for non-blocking)
     * @details Updates current_angle_ and sets servo position
     *          If wait_for_stable is true, blocks until servo is stable (250ms)
     */
    void setAngle(int angle, bool wait_for_stable = false);
    
    /**
     * @brief Start servo sweep
     * @param min_angle Minimum angle in degrees
     * @param max_angle Maximum angle in degrees
     * @param step_deg Step size in degrees
     * @param interval_ms Time between steps in milliseconds
     * @param rest_interval_ms Rest interval between sweeps in milliseconds (0 = continuous)
     */
    void startSweep(int min_angle, int max_angle, int step_deg, unsigned long interval_ms, unsigned long rest_interval_ms = DEFAULT_REST_INTERVAL_MS);
    
    /**
     * @brief Update servo sweep (call periodically)
     * @details Based on updateServoSensor() from Vehicule_Controller.ino
     */
    void update();
    
    /**
     * @brief Check if servo is currently sweeping
     * @return true if sweeping, false if resting or stopped
     */
    bool isSweeping() const;
    
    /**
     * @brief Stop servo sweep
     */
    void stopSweep();
    
    /**
     * @brief Get current servo angle
     * @return Current angle in degrees
     */
    int getCurrentAngle() const;
    
    /**
     * @brief Check if servo is stable (not moving)
     * @return true if servo has been at current angle for stabilization time
     * @details Returns true if last angle change was more than SERVO_STABILIZATION_MS ago
     */
    bool isStable() const;
    
private:
    uint8_t pin_;
    uint8_t ledc_channel_;
    bool initialized_;
    
    // Sweep state variables (matching Vehicule_Controller.ino)
    int current_angle_;
    int min_angle_;
    int max_angle_;
    int step_deg_;
    unsigned long last_step_ms_;
    unsigned long step_interval_ms_;
    unsigned long sweep_finished_ms_;
    bool sweeping_;
    bool sweep_resting_;
    unsigned long rest_interval_ms_;
    
    // Stabilization tracking
    unsigned long last_angle_change_ms_;  // Timestamp of last angle change
    
    /**
     * @brief Convert angle (degrees) to PWM duty cycle
     * @param angle Angle in degrees (0-180)
     * @return PWM duty cycle value
     */
    uint32_t angleToDuty(int angle);
};

#endif // SERVO_DRIVER_H

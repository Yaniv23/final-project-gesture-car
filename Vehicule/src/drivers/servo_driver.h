#ifndef SERVO_DRIVER_H
#define SERVO_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Servo motor driver for scanning/obstacle detection
 * @details Uses LEDC PWM (channel 4) to avoid conflicts with motors (channels 0-3)
 */
class ServoDriver {
public:
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
     * @param wait_for_stable If true, wait for stabilization delay (default: false)
     */
    void setAngle(int angle, bool wait_for_stable = false);

private:
    uint8_t pin_;
    uint8_t ledc_channel_;
    bool initialized_;
    int current_angle_;

    uint32_t angleToDuty(int angle);
};

#endif // SERVO_DRIVER_H

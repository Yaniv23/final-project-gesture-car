#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>
#include "../config.h"

/**
 * @brief Motor driver for TB6612 using ESP32 LEDC and GPIO directly
 * @details Controls 4 motors (FL, FR, BL, BR) using ESP32 APIs
 *          Each motor has its own enable pin for independent speed control
 */
class MotorDriver {
public:
    /**
     * @brief Motor configuration structure
     * @note Each motor has its own enable pin for PWM speed control
     */
    struct MotorConfig {
        uint8_t in1_pin;      // Direction pin 1
        uint8_t in2_pin;      // Direction pin 2
        uint8_t en_pin;       // Enable pin (PWM) for this motor
    };

    /**
     * @brief Motor IDs
     */
    enum MotorID {
        MOTOR_FRONT_LEFT = 0,
        MOTOR_FRONT_RIGHT = 1,
        MOTOR_BACK_LEFT = 2,
        MOTOR_BACK_RIGHT = 3
    };

    /**
     * @brief Initialize motor driver with 4 motor configurations
     * @param motors Array of 4 MotorConfig structures (direction pins + enable pin)
     * @return true if initialization successful, false otherwise
     */
    bool init(const MotorConfig motors[4]);

    /**
     * @brief Set motor speed
     * @param motor_id Motor ID (0-3)
     * @param speed Speed value from -1023 (full reverse) to +1023 (full forward), 0 = stop
     */
    void setMotorSpeed(uint8_t motor_id, int16_t speed);

    /**
     * @brief Stop all motors immediately
     */
    void stopAll();

    /**
     * @brief Check if driver is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Constructor
     */
    MotorDriver();

private:
    MotorConfig motor_configs_[4];
    uint8_t ledc_channels_[4];  // One LEDC channel per motor
    bool initialized_;

    /**
     * @brief Set motor direction
     * @param motor_id Motor ID
     * @param forward true for forward, false for reverse
     */
    void setDirection(uint8_t motor_id, bool forward);
};

#endif // MOTOR_DRIVER_H

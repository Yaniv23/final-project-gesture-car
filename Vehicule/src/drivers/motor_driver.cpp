/**
 * @file motor_driver.cpp
 * @brief Motor driver implementation using ESP32 LEDC and GPIO APIs directly
 */

#include "motor_driver.h"
#include <Arduino.h>

MotorDriver::MotorDriver() : initialized_(false), common_pwm_pin_(0), common_ledc_channel_(0) {
    // Initialize all motor configs to invalid values
    for (int i = 0; i < 4; i++) {
        motor_configs_[i].in1_pin = 0;
        motor_configs_[i].in2_pin = 0;
    }
}

bool MotorDriver::init(const MotorConfig motors[4], uint8_t common_pwm_pin, uint8_t ledc_channel) {
    // Validate inputs
    if (common_pwm_pin == 0) {
        Serial.println("[ERROR] MotorDriver: Invalid common_pwm_pin (must be > 0)");
        return false;
    }
    if (ledc_channel > 15) {
        Serial.println("[ERROR] MotorDriver: Invalid ledc_channel (must be 0-15)");
        return false;
    }
    
    // Validate motor pins
    for (int i = 0; i < 4; i++) {
        if (motors[i].in1_pin == 0 || motors[i].in2_pin == 0) {
            Serial.print("[ERROR] MotorDriver: Invalid pin configuration for motor ");
            Serial.println(i);
            return false;
        }
    }
    
    // Copy motor configurations (direction pins only)
    for (int i = 0; i < 4; i++) {
        motor_configs_[i] = motors[i];
    }

    // Store common PWM pin and channel
    common_pwm_pin_ = common_pwm_pin;
    common_ledc_channel_ = ledc_channel;

    // Initialize GPIO pins for direction control for each motor
    for (int i = 0; i < 4; i++) {
        // Configure GPIO pins for direction control
        pinMode(motor_configs_[i].in1_pin, OUTPUT);
        pinMode(motor_configs_[i].in2_pin, OUTPUT);
        
        // Set initial direction (forward)
        digitalWrite(motor_configs_[i].in1_pin, HIGH);
        digitalWrite(motor_configs_[i].in2_pin, LOW);
    }

    // Configure LEDC for common PWM pin (controls speed of all motors)
    // Frequency: 5000 Hz, Resolution: 10-bit (0-1023)
    ledcSetup(common_ledc_channel_, 5000, 10);
    ledcAttachPin(common_pwm_pin_, common_ledc_channel_);
    
    // Initialize PWM to 0 (stopped)
    ledcWrite(common_ledc_channel_, 0);

    initialized_ = true;
    return true;
}

void MotorDriver::setMotorSpeed(uint8_t motor_id, int16_t speed) {
    if (!initialized_) {
        Serial.println("[ERROR] MotorDriver: Not initialized");
        return;
    }
    if (motor_id >= 4) {
        Serial.println("[ERROR] MotorDriver: Invalid motor_id");
        return;
    }

    // Clamp speed to valid range
    if (speed > 1023) speed = 1023;
    if (speed < -1023) speed = -1023;

    // Set direction based on speed sign
    if (speed > 0) {
        setDirection(motor_id, true);  // Forward
    } else if (speed < 0) {
        setDirection(motor_id, false); // Reverse
    } else {
        // Speed is 0 - set direction to forward (maintains consistent state)
        setDirection(motor_id, true);
    }
    // Note: Speed magnitude is controlled by common PWM pin
    // The common PWM is set separately to control all motors' speed simultaneously
}

void MotorDriver::stopAll() {
    if (!initialized_) {
        return;
    }

    // Stop all motors by setting common PWM to 0
    setCommonPWM(0);
}

void MotorDriver::setDirection(uint8_t motor_id, bool forward) {
    if (motor_id >= 4) {
        return;
    }

    if (forward) {
        digitalWrite(motor_configs_[motor_id].in1_pin, HIGH);
        digitalWrite(motor_configs_[motor_id].in2_pin, LOW);
    } else {
        digitalWrite(motor_configs_[motor_id].in1_pin, LOW);
        digitalWrite(motor_configs_[motor_id].in2_pin, HIGH);
    }
}

void MotorDriver::setCommonPWM(uint16_t duty) {
    if (!initialized_) {
        Serial.println("[ERROR] MotorDriver: Not initialized");
        return;
    }

    // Clamp duty to valid range (0-1023 for 10-bit resolution)
    if (duty > 1023) {
        duty = 1023;
    }

    // Set common PWM for all motors
    ledcWrite(common_ledc_channel_, duty);
}

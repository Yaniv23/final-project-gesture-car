/**
 * @file motor_driver.cpp
 * @brief Motor driver implementation using ESP32 LEDC and GPIO APIs directly
 * @details Each motor has its own enable pin for independent speed control
 */

#include "motor_driver.h"
#include <Arduino.h>

MotorDriver::MotorDriver() : initialized_(false) {
    for (int i = 0; i < 4; i++) {
        motor_configs_[i].in1_pin = 0;
        motor_configs_[i].in2_pin = 0;
        motor_configs_[i].en_pin = 0;
        ledc_channels_[i] = 0;
    }
}

bool MotorDriver::init(const MotorConfig motors[4]) {
    // Validate all motor configurations
    for (int i = 0; i < 4; i++) {
        if (motors[i].in1_pin == 0 || motors[i].in2_pin == 0 || motors[i].en_pin == 0) {
            Serial.print("[ERROR] MotorDriver: Invalid pin configuration for motor ");
            Serial.println(i);
            return false;
        }
    }
    
    // Copy motor configurations
    for (int i = 0; i < 4; i++) {
        motor_configs_[i] = motors[i];
        // Assign LEDC channel (0-3 for the 4 motors)
        ledc_channels_[i] = i;
    }

    // Initialize direction pins and enable pins
    for (int i = 0; i < 4; i++) {
        // Initialize direction pins
        pinMode(motor_configs_[i].in1_pin, OUTPUT);
        pinMode(motor_configs_[i].in2_pin, OUTPUT);
        digitalWrite(motor_configs_[i].in1_pin, LOW);
        digitalWrite(motor_configs_[i].in2_pin, LOW);
        
        // Initialize enable pin with LEDC PWM
        // Frequency: 5000 Hz, Resolution: 10 bits (0-1023)
        ledcSetup(ledc_channels_[i], 5000, 10);
        ledcAttachPin(motor_configs_[i].en_pin, ledc_channels_[i]);
        ledcWrite(ledc_channels_[i], 0);  // Start with motor stopped
    }

    initialized_ = true;
    Serial.println("[MotorDriver] Initialized with individual enable pins per motor");
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

    // Set direction and PWM based on speed
    if (speed > 0) {
        setDirection(motor_id, true);
        // Set PWM duty cycle for this motor's enable pin
        ledcWrite(ledc_channels_[motor_id], (uint16_t)speed);
    } else if (speed < 0) {
        setDirection(motor_id, false);
        // Set PWM duty cycle (use absolute value)
        ledcWrite(ledc_channels_[motor_id], (uint16_t)(-speed));
    } else {
        // Stop motor: set direction pins low and PWM to 0
        digitalWrite(motor_configs_[motor_id].in1_pin, LOW);
        digitalWrite(motor_configs_[motor_id].in2_pin, LOW);
        ledcWrite(ledc_channels_[motor_id], 0);
    }
}

void MotorDriver::stopAll() {
    if (!initialized_) {
        return;
    }

    // Stop all motors: set direction pins low and PWM to 0 for each motor
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(motor_configs_[i].in1_pin, LOW);
        digitalWrite(motor_configs_[i].in2_pin, LOW);
        ledcWrite(ledc_channels_[i], 0);
    }
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

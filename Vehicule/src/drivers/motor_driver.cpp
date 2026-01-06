/**
 * @file motor_driver.cpp
 * @brief Motor driver implementation using ESP32 LEDC and GPIO APIs directly
 */

#include "motor_driver.h"
#include <Arduino.h>

MotorDriver::MotorDriver() : initialized_(false), common_pwm_pin_(0), common_ledc_channel_(0) {
    for (int i = 0; i < 4; i++) {
        motor_configs_[i].in1_pin = 0;
        motor_configs_[i].in2_pin = 0;
    }
}

bool MotorDriver::init(const MotorConfig motors[4], uint8_t common_pwm_pin, uint8_t ledc_channel) {
    if (common_pwm_pin == 0) {
        Serial.println("[ERROR] MotorDriver: Invalid common_pwm_pin (must be > 0)");
        return false;
    }
    if (ledc_channel > 15) {
        Serial.println("[ERROR] MotorDriver: Invalid ledc_channel (must be 0-15)");
        return false;
    }
    
    for (int i = 0; i < 4; i++) {
        if (motors[i].in1_pin == 0 || motors[i].in2_pin == 0) {
            Serial.print("[ERROR] MotorDriver: Invalid pin configuration for motor ");
            Serial.println(i);
            return false;
        }
    }
    
    for (int i = 0; i < 4; i++) {
        motor_configs_[i] = motors[i];
    }

    common_pwm_pin_ = common_pwm_pin;
    common_ledc_channel_ = ledc_channel;

    for (int i = 0; i < 4; i++) {
        pinMode(motor_configs_[i].in1_pin, OUTPUT);
        pinMode(motor_configs_[i].in2_pin, OUTPUT);
        digitalWrite(motor_configs_[i].in1_pin, LOW);
        digitalWrite(motor_configs_[i].in2_pin, LOW);
    }

    ledcSetup(common_ledc_channel_, 5000, 10);
    ledcAttachPin(common_pwm_pin_, common_ledc_channel_);
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

    if (speed > 0) {
        setDirection(motor_id, true);
    } else if (speed < 0) {
        setDirection(motor_id, false);
    } else {
        digitalWrite(motor_configs_[motor_id].in1_pin, LOW);
        digitalWrite(motor_configs_[motor_id].in2_pin, LOW);
    }
}

void MotorDriver::stopAll() {
    if (!initialized_) {
        return;
    }

    setCommonPWM(0);
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(motor_configs_[i].in1_pin, LOW);
        digitalWrite(motor_configs_[i].in2_pin, LOW);
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

void MotorDriver::setCommonPWM(uint16_t duty) {
    if (!initialized_) {
        Serial.println("[ERROR] MotorDriver: Not initialized");
        return;
    }

    if (duty > 1023) {
        duty = 1023;
    }

    ledcWrite(common_ledc_channel_, duty);
}

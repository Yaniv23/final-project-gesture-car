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
    for (int i = 0; i < 4; i++) {
        if (motors[i].in1_pin == 0 || motors[i].in2_pin == 0 || motors[i].en_pin == 0) {
            return false;
        }
    }

    for (int i = 0; i < 4; i++) {
        motor_configs_[i] = motors[i];
        ledc_channels_[i] = i;
    }

    for (int i = 0; i < 4; i++) {
        pinMode(motor_configs_[i].in1_pin, OUTPUT);
        pinMode(motor_configs_[i].in2_pin, OUTPUT);
        digitalWrite(motor_configs_[i].in1_pin, LOW);
        digitalWrite(motor_configs_[i].in2_pin, LOW);
        ledcSetup(ledc_channels_[i], 5000, 10);
        ledcAttachPin(motor_configs_[i].en_pin, ledc_channels_[i]);
        ledcWrite(ledc_channels_[i], 0);
    }

    initialized_ = true;
    return true;
}

void MotorDriver::setMotorSpeed(uint8_t motor_id, int16_t speed) {
    if (!initialized_ || motor_id >= 4) {
        return;
    }

    if (speed > 1023) speed = 1023;
    if (speed < -1023) speed = -1023;

    if (speed > 0) {
        setDirection(motor_id, true);
        ledcWrite(ledc_channels_[motor_id], (uint16_t)speed);
    } else if (speed < 0) {
        setDirection(motor_id, false);
        ledcWrite(ledc_channels_[motor_id], (uint16_t)(-speed));
    } else {
        digitalWrite(motor_configs_[motor_id].in1_pin, LOW);
        digitalWrite(motor_configs_[motor_id].in2_pin, LOW);
        ledcWrite(ledc_channels_[motor_id], 0);
    }
}

void MotorDriver::stopAll() {
    if (!initialized_) {
        return;
    }
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

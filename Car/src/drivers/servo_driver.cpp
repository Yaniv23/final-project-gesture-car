#include "servo_driver.h"
#include <Arduino.h>
#include "../config.h"

#define DEFAULT_MIN_ANGLE 0

ServoDriver::ServoDriver()
    : pin_(0)
    , ledc_channel_(SERVO_LEDC_CHANNEL)
    , initialized_(false)
    , current_angle_(DEFAULT_MIN_ANGLE)
{
}

bool ServoDriver::init(uint8_t pin) {
    pin_ = pin;
    ledcSetup(ledc_channel_, SERVO_FREQUENCY, SERVO_RESOLUTION);
    ledcAttachPin(pin, ledc_channel_);
    initialized_ = true;
    current_angle_ = DEFAULT_MIN_ANGLE;
    setAngle(current_angle_);
    return true;
}

uint32_t ServoDriver::angleToDuty(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    uint32_t pulse_us = SERVO_MIN_PULSE_US +
        (angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180;
    uint32_t period_us = 1000000 / SERVO_FREQUENCY;
    uint32_t max_duty = (1 << SERVO_RESOLUTION) - 1;
    return (pulse_us * max_duty) / period_us;
}

void ServoDriver::setAngle(int angle, bool wait_for_stable) {
    if (!initialized_) {
        return;
    }
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    if (angle != current_angle_) {
        current_angle_ = angle;
        uint32_t duty = angleToDuty(angle);
        ledcWrite(ledc_channel_, duty);
        if (wait_for_stable) {
            delay(SERVO_STABILIZATION_MS);
        }
    }
}

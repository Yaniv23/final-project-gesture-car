/**
 * @file servo_driver.cpp
 * @brief Servo driver implementation
 * @details Based on Vehicule_Controller.ino servo sweep logic
 */

#include "servo_driver.h"
#include <Arduino.h>

// Default sweep configuration (matching Vehicule_Controller.ino)
#define DEFAULT_MIN_ANGLE 0
#define DEFAULT_MAX_ANGLE 60
#define DEFAULT_STEP_DEG 5
#define DEFAULT_STEP_INTERVAL_MS 300
#define DEFAULT_REST_INTERVAL_MS 1500

ServoDriver::ServoDriver() 
    : pin_(0)
    , initialized_(false)
    , current_angle_(DEFAULT_MIN_ANGLE)
    , min_angle_(DEFAULT_MIN_ANGLE)
    , max_angle_(DEFAULT_MAX_ANGLE)
    , step_deg_(DEFAULT_STEP_DEG)
    , last_step_ms_(0)
    , step_interval_ms_(DEFAULT_STEP_INTERVAL_MS)
    , sweep_finished_ms_(0)
    , sweeping_(false)
    , sweep_resting_(false)
    , rest_interval_ms_(DEFAULT_REST_INTERVAL_MS)
{
}

bool ServoDriver::init(uint8_t pin) {
    pin_ = pin;
    
    // Attach servo to pin (matching Vehicule_Controller.ino: scanServo.attach(SERVO_PIN))
    servo_.attach(pin);
    
    initialized_ = true;
    current_angle_ = min_angle_;
    last_step_ms_ = millis();
    
    return true;
}

void ServoDriver::setAngle(int angle) {
    if (!initialized_) {
        return;
    }
    
    // Clamp angle to valid range
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    current_angle_ = angle;
    servo_.write(angle);
}

void ServoDriver::startSweep(int min_angle, int max_angle, int step_deg, unsigned long interval_ms) {
    if (!initialized_) {
        return;
    }
    
    min_angle_ = min_angle;
    max_angle_ = max_angle;
    step_deg_ = step_deg;
    step_interval_ms_ = interval_ms;
    
    current_angle_ = min_angle_;
    last_step_ms_ = millis();
    sweep_finished_ms_ = millis();
    sweeping_ = true;
    sweep_resting_ = false;
}

void ServoDriver::update() {
    if (!initialized_ || !sweeping_) {
        return;
    }
    
    unsigned long now = millis();
    
    // Check if sweep is resting (matching Vehicule_Controller.ino logic)
    if (sweep_resting_) {
        if (now - sweep_finished_ms_ >= rest_interval_ms_) {
            // Rest period finished, start new sweep
            sweep_resting_ = false;
            current_angle_ = min_angle_;
            last_step_ms_ = 0;  // Force immediate update
        }
        return;
    }
    
    // Check if step interval has passed
    if (now - last_step_ms_ < step_interval_ms_) {
        return;
    }
    
    // Update servo position (matching Vehicule_Controller.ino: scanServo.write(currentServoAngle))
    last_step_ms_ = now;
    servo_.write(current_angle_);
    
    // Check if reached max angle
    if (current_angle_ >= max_angle_) {
        // Sweep finished, start resting period
        sweep_resting_ = true;
        sweep_finished_ms_ = now;
    } else {
        // Increment angle for next step
        current_angle_ += step_deg_;
    }
}

bool ServoDriver::isSweeping() const {
    return initialized_ && sweeping_ && !sweep_resting_;
}

void ServoDriver::stopSweep() {
    sweeping_ = false;
    sweep_resting_ = false;
}

int ServoDriver::getCurrentAngle() const {
    return current_angle_;
}

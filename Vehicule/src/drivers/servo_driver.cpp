/**
 * @file servo_driver.cpp
 * @brief Servo driver implementation using LEDC PWM directly
 * @details Uses LEDC channel 4 to avoid conflicts with motors (channels 0-3)
 *          Based on Vehicule_Controller.ino servo sweep logic
 */

#include "servo_driver.h"
#include <Arduino.h>
#include "../config.h"

// Default sweep configuration (matching Vehicule_Controller.ino)
#define DEFAULT_MIN_ANGLE 0
#define DEFAULT_MAX_ANGLE 60
#define DEFAULT_STEP_DEG 5
#define DEFAULT_STEP_INTERVAL_MS 300
#define DEFAULT_REST_INTERVAL_MS 1500

ServoDriver::ServoDriver() 
    : pin_(0)
    , ledc_channel_(SERVO_LEDC_CHANNEL)
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
    
    // Initialize LEDC for servo using dedicated channel 4 (motors use 0-3)
    // Servo requires 50 Hz frequency (20ms period) with 16-bit resolution
    ledcSetup(ledc_channel_, SERVO_FREQUENCY, SERVO_RESOLUTION);
    ledcAttachPin(pin, ledc_channel_);
    
    initialized_ = true;
    current_angle_ = min_angle_;
    last_step_ms_ = millis();
    
    // Set initial position to min angle
    setAngle(min_angle_);
    
    Serial.print("[ServoDriver] Initialized on pin ");
    Serial.print(pin);
    Serial.print(" using LEDC channel ");
    Serial.print(ledc_channel_);
    Serial.print(" (motors use channels 0-3)");
    Serial.println();
    
    return true;
}

uint32_t ServoDriver::angleToDuty(int angle) {
    // Clamp angle to valid range
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    // Convert angle (0-180 degrees) to pulse width (500-2500 microseconds)
    // Linear mapping: angle 0° -> 500us, angle 180° -> 2500us
    uint32_t pulse_us = SERVO_MIN_PULSE_US + 
                       (angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180;
    
    // Convert pulse width to duty cycle
    // Duty = (pulse_width_us / period_us) * max_duty
    // Period = 20ms = 20000us for 50Hz
    // Max duty = 2^16 - 1 = 65535 for 16-bit resolution
    uint32_t period_us = 1000000 / SERVO_FREQUENCY; // 20000us for 50Hz
    uint32_t max_duty = (1 << SERVO_RESOLUTION) - 1; // 65535 for 16-bit
    
    return (pulse_us * max_duty) / period_us;
}

void ServoDriver::setAngle(int angle) {
    if (!initialized_) {
        return;
    }
    
    // Clamp angle to valid range
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    current_angle_ = angle;
    
    // Convert angle to PWM duty cycle and write to LEDC
    uint32_t duty = angleToDuty(angle);
    ledcWrite(ledc_channel_, duty);
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
    
    // Set initial position
    setAngle(min_angle_);
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
    
    // Update servo position using LEDC PWM (channel 4, separate from motors)
    last_step_ms_ = now;
    setAngle(current_angle_);
    
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

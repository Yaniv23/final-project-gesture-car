/**
 * @file ultrasonic_driver.cpp
 * @brief Ultrasonic sensor driver implementation
 * @details Based on Vehicule_Controller.ino readDistanceCM() function
 */

#include "ultrasonic_driver.h"
#include <Arduino.h>

Ultrasonic::Ultrasonic()
    : trig_pin_(0)
    , echo_pin_(0)
    , initialized_(false)
    , last_distance_(-1.0)
{
}

bool Ultrasonic::init(uint8_t trig_pin, uint8_t echo_pin) {
    trig_pin_ = trig_pin;
    echo_pin_ = echo_pin;
    
    // Configure pins (matching Vehicule_Controller.ino setup)
    pinMode(trig_pin_, OUTPUT);
    pinMode(echo_pin_, INPUT);
    digitalWrite(trig_pin_, LOW);  // Start with trigger low
    
    initialized_ = true;
    last_distance_ = -1.0;
    
    return true;
}

float Ultrasonic::readDistanceRawInternal() {
    if (!initialized_) {
        return -1.0;
    }
    
    // Send trigger pulse (matching Vehicule_Controller.ino readDistanceCM())
    digitalWrite(trig_pin_, LOW);
    delayMicroseconds(TRIG_SETTLE_US);  // 2 microseconds
    
    digitalWrite(trig_pin_, HIGH);
    delayMicroseconds(TRIG_PULSE_US);  // 10 microseconds
    
    digitalWrite(trig_pin_, LOW);
    
    // Read echo pulse duration
    // pulseIn returns duration in microseconds, or 0 if timeout
    // NOTE: use explicit timeout to avoid long blocking that can starve the
    //       FreeRTOS idle task and trigger the ESP32 task watchdog.
    long duration = pulseIn(echo_pin_, HIGH, ECHO_TIMEOUT_US);
    
    // Calculate distance (matching Vehicule_Controller.ino formula)
    // distance = (duration * 0.034) / 2
    // duration is in microseconds, 0.034 cm/us is speed of sound
    // Divide by 2 because sound travels to object and back
    if (duration > 0) {
        float distance = (duration * SPEED_OF_SOUND_CM_PER_US) / DISTANCE_DIVISOR;
        return distance;
    } else {
        // Timeout or error
        return -1.0;
    }
}

float Ultrasonic::readDistanceCM() {
    // Read raw distance from sensor
    float raw_distance = readDistanceRawInternal();
    
    // Update last_distance_ with raw value if valid
    if (raw_distance >= 0.0f && raw_distance <= 400.0f) {
        last_distance_ = raw_distance;
        return raw_distance;
    } else {
        last_distance_ = -1.0;
        return -1.0;
    }
}



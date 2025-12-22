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

float Ultrasonic::readDistanceCM() {
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
    long duration = pulseIn(echo_pin_, HIGH);
    
    // Calculate distance (matching Vehicule_Controller.ino formula)
    // distance = (duration * 0.034) / 2
    // duration is in microseconds, 0.034 cm/us is speed of sound
    // Divide by 2 because sound travels to object and back
    if (duration > 0) {
        float distance = (duration * SPEED_OF_SOUND_CM_PER_US) / DISTANCE_DIVISOR;
        last_distance_ = distance;
        return distance;
    } else {
        // Timeout or error
        last_distance_ = -1.0;
        return -1.0;
    }
}

bool Ultrasonic::isObstacle(float threshold_cm) {
    float distance = readDistanceCM();
    
    // Check if obstacle detected (matching Vehicule_Controller.ino logic)
    // if (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_DISTANCE_CM)
    if (distance > 0.0 && distance < threshold_cm) {
        return true;
    }
    
    return false;
}

float Ultrasonic::getLastDistance() const {
    return last_distance_;
}

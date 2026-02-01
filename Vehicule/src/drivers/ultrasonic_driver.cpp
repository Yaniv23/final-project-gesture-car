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
    , buffer_index_(0)
    , valid_samples_count_(0)
    , last_filtered_distance_(-1.0f)
    , consecutive_invalid_count_(0)
    , consecutive_spike_count_(0)
{
    // Initialize filter buffer with invalid values
    distance_buffer_.fill(-1.0f);
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
    
    // Reset filter state
    buffer_index_ = 0;
    valid_samples_count_ = 0;
    last_filtered_distance_ = -1.0f;
    consecutive_invalid_count_ = 0;
    consecutive_spike_count_ = 0;
    distance_buffer_.fill(-1.0f);
    
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

float Ultrasonic::readDistanceRaw() {
    return readDistanceRawInternal();
}

float Ultrasonic::readDistanceCM() {
    // 1. Read raw distance from sensor hardware
    float raw_distance = readDistanceRawInternal();
    
    // 2. If value is invalid (< 0 or > 400cm): return last valid for a few reads, then -1
    //    so we don't hold a stale value forever (avoids "stuck" distance)
    if (raw_distance < 0.0f || raw_distance > 400.0f) {
        last_distance_ = -1.0f;
        consecutive_invalid_count_++;
        if (last_filtered_distance_ > 0.0f && last_filtered_distance_ <= 400.0f &&
            consecutive_invalid_count_ <= MAX_CONSECUTIVE_INVALID) {
            return last_filtered_distance_;
        }
        // Too many consecutive timeouts: clear stale value so next valid read starts fresh
        if (consecutive_invalid_count_ > MAX_CONSECUTIVE_INVALID) {
            last_filtered_distance_ = -1.0f;
        }
        return -1.0f;
    }
    consecutive_invalid_count_ = 0;
    
    // 3. Calculate current average (if buffer has valid samples)
    float current_avg = -1.0f;
    if (valid_samples_count_ > 0) {
        current_avg = calculateAverage();
        if (current_avg < 0.0f) {
            current_avg = raw_distance;
        }
    } else {
        current_avg = raw_distance;
    }
    
    // 4. Validate new value (reject spikes) — but after N consecutive rejections, force-accept
    //    so the filter can adapt when distance really changes (avoids "stuck" distance)
    if (valid_samples_count_ >= 2) {
        if (!isValidSample(raw_distance, current_avg)) {
            consecutive_spike_count_++;
            if (consecutive_spike_count_ <= MAX_CONSECUTIVE_SPIKE &&
                last_filtered_distance_ > 0.0f) {
                return last_filtered_distance_;
            }
            // Force-accept this sample so filter can catch up to new distance
        }
    }
    consecutive_spike_count_ = 0;
    
    // 5. Add valid value to circular buffer
    distance_buffer_[buffer_index_] = raw_distance;
    buffer_index_ = (buffer_index_ + 1) % FILTER_BUFFER_SIZE;
    
    // 6. Update valid samples count
    if (valid_samples_count_ < FILTER_BUFFER_SIZE) {
        valid_samples_count_++;
    }
    
    // 7. Calculate new filtered average
    float filtered_distance = calculateAverage();
    
    // 8. Update state
    last_distance_ = raw_distance;
    last_filtered_distance_ = filtered_distance;
    
    return filtered_distance;
}

float Ultrasonic::calculateAverage() const {
    if (valid_samples_count_ == 0) {
        return -1.0f;
    }
    
    // Calculate average of VALID samples only (exclude -1.0f values)
    float sum = 0.0f;
    int valid_count = 0;
    for (size_t i = 0; i < valid_samples_count_; ++i) {
        if (distance_buffer_[i] > 0.0f && distance_buffer_[i] <= 400.0f) {
            sum += distance_buffer_[i];
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        return -1.0f;
    }
    
    return sum / valid_count;
}

bool Ultrasonic::isValidSample(float raw, float current_avg) const {
    // Reject if deviation from average exceeds threshold (spike detection)
    float deviation = (raw > current_avg) ? (raw - current_avg) : (current_avg - raw);
    return deviation <= MAX_DEVIATION_CM;
}



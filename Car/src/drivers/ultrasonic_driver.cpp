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
    distance_buffer_.fill(-1.0f);
}

bool Ultrasonic::init(uint8_t trig_pin, uint8_t echo_pin) {
    trig_pin_ = trig_pin;
    echo_pin_ = echo_pin;
    pinMode(trig_pin_, OUTPUT);
    pinMode(echo_pin_, INPUT);
    digitalWrite(trig_pin_, LOW);
    initialized_ = true;
    last_distance_ = -1.0;
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
    digitalWrite(trig_pin_, LOW);
    delayMicroseconds(TRIG_SETTLE_US);
    digitalWrite(trig_pin_, HIGH);
    delayMicroseconds(TRIG_PULSE_US);
    digitalWrite(trig_pin_, LOW);
    long duration = pulseIn(echo_pin_, HIGH, ECHO_TIMEOUT_US);
    if (duration > 0) {
        return (duration * SPEED_OF_SOUND_CM_PER_US) / DISTANCE_DIVISOR;
    }
    return -1.0;
}

float Ultrasonic::readDistanceRaw() {
    return readDistanceRawInternal();
}

float Ultrasonic::readDistanceCM() {
    float raw_distance = readDistanceRawInternal();

    if (raw_distance < 0.0f || raw_distance > 400.0f) {
        last_distance_ = -1.0f;
        consecutive_invalid_count_++;
        if (last_filtered_distance_ > 0.0f && last_filtered_distance_ <= 400.0f &&
            consecutive_invalid_count_ <= MAX_CONSECUTIVE_INVALID) {
            return last_filtered_distance_;
        }
        if (consecutive_invalid_count_ > MAX_CONSECUTIVE_INVALID) {
            last_filtered_distance_ = -1.0f;
        }
        return -1.0f;
    }
    consecutive_invalid_count_ = 0;

    float current_avg = -1.0f;
    if (valid_samples_count_ > 0) {
        current_avg = calculateAverage();
        if (current_avg < 0.0f) {
            current_avg = raw_distance;
        }
    } else {
        current_avg = raw_distance;
    }

    if (valid_samples_count_ >= 2) {
        if (!isValidSample(raw_distance, current_avg)) {
            consecutive_spike_count_++;
            if (consecutive_spike_count_ <= MAX_CONSECUTIVE_SPIKE &&
                last_filtered_distance_ > 0.0f) {
                return last_filtered_distance_;
            }
        }
    }
    consecutive_spike_count_ = 0;

    distance_buffer_[buffer_index_] = raw_distance;
    buffer_index_ = (buffer_index_ + 1) % FILTER_BUFFER_SIZE;
    if (valid_samples_count_ < FILTER_BUFFER_SIZE) {
        valid_samples_count_++;
    }

    float filtered_distance = calculateAverage();
    last_distance_ = raw_distance;
    last_filtered_distance_ = filtered_distance;
    return filtered_distance;
}

float Ultrasonic::calculateAverage() const {
    if (valid_samples_count_ == 0) {
        return -1.0f;
    }
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
    float deviation = (raw > current_avg) ? (raw - current_avg) : (current_avg - raw);
    return deviation <= MAX_DEVIATION_CM;
}



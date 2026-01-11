/**
 * @file position_tracker.cpp
 * @brief Implementation of PositionTracker
 */

#include "position_tracker.h"
#include "../config.h"
#include <Arduino.h>

PositionTracker::PositionTracker() : index_(0), is_moving_(false) {
    reset();
}

void PositionTracker::update(float current_distance) {
    unsigned long now = millis();
    
    // Add new measurement (circular FIFO)
    index_ = (index_ + 1) % HISTORY_SIZE;
    distances_[index_] = current_distance;
    timestamps_[index_] = now;
    
    // Calculate variance to determine movement
    calculateVariance();
}

void PositionTracker::reset() {
    for (int i = 0; i < HISTORY_SIZE; i++) {
        distances_[i] = 0.0f;
        timestamps_[i] = 0;
    }
    index_ = 0;
    is_moving_ = false;
}

void PositionTracker::calculateVariance() {
    // Calculate mean of distances
    float mean = 0.0f;
    int valid_count = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (distances_[i] > 0) {
            mean += distances_[i];
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        is_moving_ = false;
        return;
    }
    
    mean /= valid_count;
    
    // Calculate variance
    float variance = 0.0f;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (distances_[i] > 0) {
            float diff = distances_[i] - mean;
            variance += diff * diff;
        }
    }
    variance /= valid_count;
    
    // If variance low = no movement (vehicle probably stuck)
    // If variance high = movement detected (vehicle advancing)
    is_moving_ = (variance > POSITION_VARIANCE_THRESHOLD);
}

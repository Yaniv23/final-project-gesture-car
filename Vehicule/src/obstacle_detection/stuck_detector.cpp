/**
 * @file stuck_detector.cpp
 * @brief Implementation of StuckDetector
 */

#include "stuck_detector.h"
#include "../config.h"
#include <Arduino.h>

StuckDetector::StuckDetector(PositionTracker& tracker)
    : tracker_(tracker), last_forward_attempt_ms_(0), last_position_change_ms_(0),
      last_best_distance_(0.0f), consecutive_failed_attempts_(0), is_stuck_(false),
      consecutive_stuck_count_(0) {
}

void StuckDetector::reset() {
    last_forward_attempt_ms_ = 0;
    last_position_change_ms_ = 0;
    last_best_distance_ = 0.0f;
    consecutive_failed_attempts_ = 0;
    is_stuck_ = false;
    consecutive_stuck_count_ = 0;
}

void StuckDetector::update(float current_distance, bool is_moving, uint8_t last_command) {
    updateStuckMemory(current_distance, is_moving);
}

void StuckDetector::updateStuckMemory(float current_distance, bool is_moving) {
    unsigned long now = millis();
    
    if (is_moving) {
        last_position_change_ms_ = now;
        consecutive_failed_attempts_ = 0;
    } else {
        // Not moving - check if we're stuck
        if (now - last_position_change_ms_ >= STUCK_TIME_THRESHOLD_MS) {
            consecutive_failed_attempts_++;
            if (consecutive_failed_attempts_ >= STUCK_THRESHOLD_ATTEMPTS) {
                is_stuck_ = true;
            }
        }
    }
    
    last_best_distance_ = current_distance;
}

bool StuckDetector::isStuck(const ScanResult* scan_result) {
    if (!STUCK_DETECTION_ENABLED) {
        return false;
    }
    
    return checkIfStuckAdvanced(scan_result);
}

bool StuckDetector::checkIfStuckAdvanced(const ScanResult* scan_result) {
    // Check 1: All directions blocked (improved existing logic)
    if (scan_result && scan_result->is_valid) {
        if (scan_result->distance_left < MIN_FREE_SPACE_CM &&
            scan_result->distance_front < MIN_FREE_SPACE_CM &&
            scan_result->distance_right < MIN_FREE_SPACE_CM) {
            consecutive_stuck_count_++;
            
            if (consecutive_stuck_count_ >= STUCK_BACKUP_COUNT) {
                return true;
            }
        } else {
            // At least one direction free - reset counter
            consecutive_stuck_count_ = 0;
        }
    }
    
    // Check 2: No real movement despite FORWARD command
    // This check is done in update() method
    
    // Combine both checks
    return (consecutive_stuck_count_ >= STUCK_BACKUP_COUNT) || is_stuck_;
}

#ifndef STUCK_DETECTOR_H
#define STUCK_DETECTOR_H

#include <stdint.h>
#include "../obstacle_detection/position_tracker.h"
#include "../obstacle_detection/obstacle_scanner.h"

/**
 * @file stuck_detector.h
 * @brief Detects if vehicle is stuck using multiple methods
 * @details Combines distance-based and movement-based detection
 */
class StuckDetector {
public:
    StuckDetector(PositionTracker& tracker);
    
    /**
     * @brief Update stuck detection (call every task period)
     * @param current_distance Current distance measurement
     * @param is_moving Whether vehicle is currently moving
     * @param last_command Last command sent (for movement check)
     */
    void update(float current_distance, bool is_moving, uint8_t last_command);
    
    /**
     * @brief Check if vehicle is stuck
     * @param scan_result Current scan result (optional)
     * @return true if stuck detected
     */
    bool isStuck(const ScanResult* scan_result = nullptr);
    
    /**
     * @brief Reset stuck detection memory
     */
    void reset();
    
    /**
     * @brief Get consecutive failed attempts count
     */
    int getFailedAttempts() const { return consecutive_failed_attempts_; }

private:
    PositionTracker& tracker_;
    
    // Stuck detection memory
    unsigned long last_forward_attempt_ms_;
    unsigned long last_position_change_ms_;
    float last_best_distance_;
    int consecutive_failed_attempts_;
    bool is_stuck_;
    int consecutive_stuck_count_;
    
    bool checkIfStuckAdvanced(const ScanResult* scan_result);
    void updateStuckMemory(float current_distance, bool is_moving);
};

#endif // STUCK_DETECTOR_H

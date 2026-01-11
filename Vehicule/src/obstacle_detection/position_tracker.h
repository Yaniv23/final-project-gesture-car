#ifndef POSITION_TRACKER_H
#define POSITION_TRACKER_H

#include <stdint.h>

/**
 * @file position_tracker.h
 * @brief Tracks position history to detect actual vehicle movement
 * @details Uses variance calculation on distance measurements to determine if vehicle is moving
 */
class PositionTracker {
public:
    PositionTracker();
    
    /**
     * @brief Update position history with new distance measurement
     * @param current_distance Current distance in cm
     */
    void update(float current_distance);
    
    /**
     * @brief Check if vehicle is actually moving
     * @return true if movement detected (variance > threshold)
     */
    bool isMoving() const { return is_moving_; }
    
    /**
     * @brief Reset position history
     */
    void reset();

private:
    static constexpr int HISTORY_SIZE = 5;
    float distances_[HISTORY_SIZE];
    unsigned long timestamps_[HISTORY_SIZE];
    int index_;
    bool is_moving_;
    
    void calculateVariance();
};

#endif // POSITION_TRACKER_H

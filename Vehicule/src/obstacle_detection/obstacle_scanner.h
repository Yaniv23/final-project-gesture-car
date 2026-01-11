#ifndef OBSTACLE_SCANNER_H
#define OBSTACLE_SCANNER_H

#include <stdint.h>
#include "../config.h"
#include "../drivers/ultrasonic_driver.h"
#include "../drivers/servo_driver.h"

/**
 * @file obstacle_scanner.h
 * @brief Obstacle scanner using servo-mounted ultrasonic sensor
 * @details Performs 3-direction scans (left, front, right) in a non-blocking manner
 */

struct ScanResult {
    float distance_left;    // Distance à gauche (45°)
    float distance_front;   // Distance devant (90°)
    float distance_right;   // Distance à droite (135°)
    unsigned long timestamp_ms;
    bool is_valid;
};

class ObstacleScanner {
public:
    ObstacleScanner(Ultrasonic& ultrasonic, ServoDriver& servo);
    
    /**
     * @brief Initialize scanner
     * @return true if successful
     */
    bool init();
    
    /**
     * @brief Perform 3-direction scan (non-blocking, stateful)
     * @param result Output scan result
     * @return true if scan complete, false if still in progress
     */
    bool scan3Directions(ScanResult& result);
    
    /**
     * @brief Get filtered distance measurement
     * @param samples Number of samples for filtering
     * @return Distance in cm, or -1.0 if invalid
     */
    float getFilteredDistance(int samples = FILTER_SAMPLES);
    
    /**
     * @brief Check if obstacle detected ahead
     * @param threshold_cm Critical distance threshold
     * @return true if obstacle detected
     */
    bool isObstacleAhead(float threshold_cm = CRITICAL_DISTANCE_CM);
    
    /**
     * @brief Reset scanner state
     */
    void reset();
    
    /**
     * @brief Set servo to center position (for forward movement)
     */
    void setServoToCenter();

private:
    Ultrasonic& ultrasonic_;
    ServoDriver& servo_;
    
    // Scan state
    bool scan_in_progress_;
    int scan_step_;  // 0=init, 1=left, 2=center, 3=right, 4=done
    unsigned long last_servo_move_ms_;
    
    float filteredDistance(int samples);
};

#endif // OBSTACLE_SCANNER_H

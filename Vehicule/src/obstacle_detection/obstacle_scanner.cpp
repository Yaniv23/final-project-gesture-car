/**
 * @file obstacle_scanner.cpp
 * @brief Implementation of ObstacleScanner
 */

#include "obstacle_scanner.h"
#include "../config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>

ObstacleScanner::ObstacleScanner(Ultrasonic& ultrasonic, ServoDriver& servo)
    : ultrasonic_(ultrasonic), servo_(servo), scan_in_progress_(false), 
      scan_step_(0), last_servo_move_ms_(0) {
}

bool ObstacleScanner::init() {
    reset();
    return true;
}

void ObstacleScanner::reset() {
    scan_in_progress_ = false;
    scan_step_ = 0;
    last_servo_move_ms_ = 0;
}

void ObstacleScanner::setServoToCenter() {
    servo_.setAngle(SCAN_CENTER_ANGLE);
    servo_.stopSweep();
}

float ObstacleScanner::filteredDistance(int samples) {
    float sum = 0.0f;
    int valid_count = 0;
    
    for (int i = 0; i < samples; i++) {
        float dist = ultrasonic_.readDistanceCM();
        // Filter invalid values (negative or > 400cm)
        if (dist >= 0.0f && dist <= 400.0f) {
            sum += dist;
            valid_count++;
        }
        if (i < samples - 1) {
            vTaskDelay(pdMS_TO_TICKS(FILTER_DELAY_MS));
        }
    }
    
    if (valid_count == 0) {
        return -1.0f;
    }
    
    return sum / valid_count;
}

float ObstacleScanner::getFilteredDistance(int samples) {
    return filteredDistance(samples);
}

bool ObstacleScanner::isObstacleAhead(float threshold_cm) {
    float distance = getFilteredDistance();
    return (distance >= 0.0f && distance < threshold_cm);
}

bool ObstacleScanner::scan3Directions(ScanResult& result) {
    unsigned long now = millis();
    
    if (!scan_in_progress_) {
        // Start new scan
        scan_in_progress_ = true;
        scan_step_ = 1; // Start with left
        last_servo_move_ms_ = 0;
    }
    
    // Perform scan over multiple task cycles to avoid blocking
    switch (scan_step_) {
        case 1: // Left (45°)
            if (last_servo_move_ms_ == 0) {
                servo_.setAngle(SCAN_LEFT_ANGLE);
                last_servo_move_ms_ = now;
            } else if (now - last_servo_move_ms_ >= SERVO_STABILIZATION_MS) {
                result.distance_left = filteredDistance(FILTER_SAMPLES);
                scan_step_ = 2;
                last_servo_move_ms_ = now;
            }
            return false; // Still in progress
            
        case 2: // Center (90°)
            if (now - last_servo_move_ms_ >= SERVO_STABILIZATION_MS) {
                servo_.setAngle(SCAN_CENTER_ANGLE);
                last_servo_move_ms_ = now;
                scan_step_ = 3;
            }
            return false; // Still in progress
            
        case 3: // Right (135°)
            if (now - last_servo_move_ms_ >= SERVO_STABILIZATION_MS) {
                servo_.setAngle(SCAN_RIGHT_ANGLE);
                last_servo_move_ms_ = now;
                scan_step_ = 4;
            }
            return false; // Still in progress
            
        case 4: // Measure right and finish
            if (now - last_servo_move_ms_ >= SERVO_STABILIZATION_MS) {
                result.distance_right = filteredDistance(FILTER_SAMPLES);
                // Measure front (center) - servo should already be at center from step 2
                // But we need to wait a bit more and measure
                servo_.setAngle(SCAN_CENTER_ANGLE);
                vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
                result.distance_front = filteredDistance(FILTER_SAMPLES);
                
                result.timestamp_ms = millis();
                result.is_valid = (result.distance_left > 0 && 
                                   result.distance_front > 0 && 
                                   result.distance_right > 0);
                
                // Reset for next scan
                scan_step_ = 0;
                scan_in_progress_ = false;
                return true; // Scan complete
            }
            return false; // Still in progress
    }
    
    return false; // Should not reach here
}

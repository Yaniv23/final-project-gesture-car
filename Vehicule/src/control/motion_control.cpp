/**
 * @file motion_control.cpp
 * @brief Motion control implementation
 * @details Based on Vehicule_Controller.ino motion functions
 */

#include "motion_control.h"
#include "../config.h"
#include <Arduino.h>

// Static MotorDriver pointer
static MotorDriver* motor_driver_ = nullptr;
static bool error_logged_ = false;  // Prevent error spam

// Helper function to check initialization
static bool checkInitialized() {
    if (motor_driver_ == nullptr) {
        if (!error_logged_) {
            Serial.println("[ERROR] MotionControl: Not initialized");
            error_logged_ = true;
        }
        return false;
    }
    if (!motor_driver_->isInitialized()) {
        if (!error_logged_) {
            Serial.println("[ERROR] MotionControl: MotorDriver not initialized");
            error_logged_ = true;
        }
        return false;
    }
    error_logged_ = false;  // Reset flag on success
    return true;
}

// Convert speed from 0-255 range to 0-1023 range for MotorDriver
// MotorDriver uses -1023 to +1023, so we scale MOTOR_SPEED_SLOW (150) accordingly
static int16_t getMotorSpeed() {
    // Convert MOTOR_SPEED_SLOW (0-255) to MotorDriver range (0-1023)
    // Formula: (MOTOR_SPEED_SLOW * MOTOR_PWM_MAX) / MOTOR_SPEED_MAX_8BIT
    return (MOTOR_SPEED_SLOW * MOTOR_PWM_MAX) / MOTOR_SPEED_MAX_8BIT;
}

// Get reduced motor speed (60% of normal speed) for non-forward/backward movements
// 40% slower = 60% of original speed
static int16_t getMotorSpeedReduced() {
    // Return 60% of normal speed (0.6 * getMotorSpeed())
    return (getMotorSpeed() * 6) / 10;  // Integer math: 60% = 6/10
}

void motion_init(MotorDriver* motor_driver) {
    if (motor_driver == nullptr) {
        Serial.println("[ERROR] MotionControl: motor_driver is null");
        return;
    }
    motor_driver_ = motor_driver;
}

void motion_stop() {
    if (!checkInitialized()) {
        return;
    }

    motor_driver_->stopAll();
}

void motion_forward() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: Forward now moves backward (negative speed)
    int16_t speed = -getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
}

void motion_backward() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: Backward now moves forward (positive speed)
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
}

void motion_sideway_left() {
    if (!checkInitialized()) {
        return;
    }

    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
}


void motion_sideway_right() {
    if (!checkInitialized()) {
        return;
    }
    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, -speed);
}
void motion_rotate_cw() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: FR: Forward, FL: Backward, BR: Forward, BL: Backward
    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, -speed);
}

void motion_rotate_ccw() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: FR: Backward, FL: Forward, BR: Backward, BL: Forward
    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
}

void motion_diagonal_315() {
    if (!checkInitialized()) {
        return;
    }

    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
}

void motion_diagonal_45() {
    if (!checkInitialized()) {
        return;
    }
    int16_t speed = -getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);

}

void motion_diagonal_225() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: FR: Forward, BL: Forward (others stopped)
    // Set active motors first, then stopped motors
    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    // Then stop the other motors
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
}

void motion_diagonal_135() {
    if (!checkInitialized()) {
        return;
    }
    int16_t speed = -getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
   
}

void motion_pivot_left() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: FR: Backward, FL: Forward (others stopped)
    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
}

void motion_pivot_right() {
    if (!checkInitialized()) {
        return;
    }

    // INVERTED: FR: Forward, FL: Backward (others stopped)
    int16_t speed = getMotorSpeedReduced();  // 40% slower for non-forward/backward movements
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
}

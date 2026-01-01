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

    // Use MotorDriver helper to stop all outputs and PWM
    motor_driver_->stopAll();
}

void motion_forward() {
    if (!checkInitialized()) {
        return;
    }

    // All wheels forward (matching Forward() from Vehicule_Controller.ino)
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_backward() {
    if (!checkInitialized()) {
        return;
    }

    // All wheels backward (matching Backward() from Vehicule_Controller.ino)
    int16_t speed = -getMotorSpeed();  // Negative for backward
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setCommonPWM((uint16_t)(-speed));
}

void motion_sideway_left() {
    if (!checkInitialized()) {
        return;
    }

    // Sideway left (previously Strafe left)
    // FR: Forward, FL: Backward, BR: Backward, BL: Forward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_sideway_right() {
    if (!checkInitialized()) {
        return;
    }

    // Sideway right (previously Strafe right)
    // FR: Backward, FL: Forward, BR: Forward, BL: Backward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, -speed);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_rotate_cw() {
    if (!checkInitialized()) {
        return;
    }

    // Rotate clockwise (matching rotate_cw() from Vehicule_Controller.ino)
    // FR: Backward, FL: Forward, BR: Backward, BL: Forward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_rotate_ccw() {
    if (!checkInitialized()) {
        return;
    }

    // Rotate counter-clockwise (matching rotate_ccw() from Vehicule_Controller.ino)
    // FR: Forward, FL: Backward, BR: Forward, BL: Backward
    // NOTE: Flipped BR to match expected table
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, -speed);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_diagonal_315() {
    if (!checkInitialized()) {
        return;
    }

    // Diagonal 315 degrees (forward-left)
    // FR: Forward, FL: Stop, BR: Stop, BL: Forward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_diagonal_45() {
    if (!checkInitialized()) {
        return;
    }

    // Diagonal 45 degrees (forward-right)
    // FR: Stop, FL: Forward, BR: Forward, BL: Stop
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_diagonal_225() {
    if (!checkInitialized()) {
        return;
    }

    // Diagonal 225 degrees (backward-left)
    // FR: Backward, FL: Stop, BR: Stop, BL: Backward
    int16_t speed = -getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    motor_driver_->setCommonPWM((uint16_t)(-speed));
}

void motion_diagonal_135() {
    if (!checkInitialized()) {
        return;
    }

    // Diagonal 135 degrees (backward-right)
    // FR: Stop, FL: Backward, BR: Backward, BL: Stop
    int16_t speed = -getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
    motor_driver_->setCommonPWM((uint16_t)(-speed));
}

void motion_pivot_left() {
    if (!checkInitialized()) {
        return;
    }

    // Pivot left (matching pivot_left() from Vehicule_Controller.ino)
    // FR: Forward, FL: Backward (others stopped)
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_pivot_right() {
    if (!checkInitialized()) {
        return;
    }

    // Pivot right (matching pivot_right() from Vehicule_Controller.ino)
    // FR: Backward, FL: Forward (others stopped)
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);
    motor_driver_->setCommonPWM((uint16_t)speed);
}

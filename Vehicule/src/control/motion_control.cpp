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
    
    // Stop all motors (matching stop_motors() from Vehicule_Controller.ino)
    motor_driver_->stopAll();
}

void motion_forward() {
    if (!checkInitialized()) {
        return;
    }
    
    // Forward_FR(), Forward_FL(), Forward_BR(), Forward_BL()
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_backward() {
    if (!checkInitialized()) {
        return;
    }
    
    // All wheels backward (matching Backward() from Vehicule_Controller.ino)
    // Backward_FR(), Backward_FL(), Backward_BR(), Backward_BL()
    int16_t speed = -getMotorSpeed();  // Negative for backward
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    // Set common PWM speed for all motors (use absolute value)
    motor_driver_->setCommonPWM((uint16_t)(-speed));
}

void motion_strafe_left() {
    if (!checkInitialized()) {
        return;
    }
    
    // Strafe left (matching Sideway_Left() from Vehicule_Controller.ino)
    // FR: Forward, FL: Backward, BR: Backward, BL: Forward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_strafe_right() {
    if (!checkInitialized()) {
        return;
    }
    
    // Strafe right (matching Sideway_Right() from Vehicule_Controller.ino)
    // FR: Backward, FL: Forward, BR: Forward, BL: Backward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, -speed);
    // Set common PWM speed for all motors
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
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_rotate_ccw() {
    if (!checkInitialized()) {
        return;
    }
    
    // Rotate counter-clockwise (matching rotate_ccw() from Vehicule_Controller.ino)
    // FR: Forward, FL: Backward, BR: Forward, BL: Backward
    int16_t speed = getMotorSpeed();
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, -speed);
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_diagonal_forward_left() {
    if (!checkInitialized()) {
        return;
    }
    
    // Diagonal forward left (matching diagonal_forward_left() from Vehicule_Controller.ino)
    // FR: Forward, BL: Forward (others stopped)
    int16_t speed = getMotorSpeed();
    // Set directions first (don't call stopAll - it sets PWM to 0)
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, 0);  // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);   // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_diagonal_forward_right() {
    if (!checkInitialized()) {
        return;
    }
    
    // Diagonal forward right (matching diagonal_forward_right() from Vehicule_Controller.ino)
    // FL: Forward, BR: Forward (others stopped)
    int16_t speed = getMotorSpeed();
    // Set directions first (don't call stopAll - it sets PWM to 0)
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, 0);  // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);   // Stop
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_diagonal_backward_left() {
    if (!checkInitialized()) {
        return;
    }
    
    // Diagonal backward left (matching diagonal_backward_left() from Vehicule_Controller.ino)
    // FR: Backward, BL: Backward (others stopped)
    int16_t speed = -getMotorSpeed();  // Negative for backward
    // Set directions first (don't call stopAll - it sets PWM to 0)
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, 0);  // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);   // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, speed);
    // Set common PWM speed for all motors (use absolute value)
    motor_driver_->setCommonPWM((uint16_t)(-speed));
}

void motion_diagonal_backward_right() {
    if (!checkInitialized()) {
        return;
    }
    
    // Diagonal backward right (matching diagonal_backward_right() from Vehicule_Controller.ino)
    // FL: Backward, BR: Backward (others stopped)
    int16_t speed = -getMotorSpeed();  // Negative for backward
    // Set directions first (don't call stopAll - it sets PWM to 0)
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, 0);  // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);   // Stop
    // Set common PWM speed for all motors (use absolute value)
    motor_driver_->setCommonPWM((uint16_t)(-speed));
}

void motion_pivot_left() {
    if (!checkInitialized()) {
        return;
    }
    
    // Pivot left (matching pivot_left() from Vehicule_Controller.ino)
    // FR: Forward, FL: Backward (others stopped)
    int16_t speed = getMotorSpeed();
    // Set directions first (don't call stopAll - it sets PWM to 0)
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);  // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);  // Stop
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

void motion_pivot_right() {
    if (!checkInitialized()) {
        return;
    }
    
    // Pivot right (matching pivot_right() from Vehicule_Controller.ino)
    // FR: Backward, FL: Forward (others stopped)
    int16_t speed = getMotorSpeed();
    // Set directions first (don't call stopAll - it sets PWM to 0)
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_RIGHT, -speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_FRONT_LEFT, speed);
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_RIGHT, 0);  // Stop
    motor_driver_->setMotorSpeed(MotorDriver::MOTOR_BACK_LEFT, 0);  // Stop
    // Set common PWM speed for all motors
    motor_driver_->setCommonPWM((uint16_t)speed);
}

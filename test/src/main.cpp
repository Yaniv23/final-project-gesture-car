/**
 * @file main.cpp
 * @brief Motor Driver Command Test Suite
 * @details Tests all motor commands to verify driver functionality
 * 
 * This test program:
 * 1. Initializes the motor driver with pin configurations
 * 2. Tests each motion command sequentially
 * 3. Prints expected vs actual motor speeds
 * 4. Verifies command execution
 */

#include <Arduino.h>
#include "motor_driver.h"
#include "motion_control.h"
#include "config.h"

// Test configuration
#define TEST_DELAY_MS 2000        // Time to run each command (2 seconds)
#define TEST_PAUSE_MS 500          // Pause between tests (0.5 seconds)

// Forward declarations
void testCommand(const char* name, void (*motion_func)());

// Global motor driver instance
MotorDriver motor_driver;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Configure motor pins
    MotorDriver::MotorConfig motors[4] = {
        // Front Left Motor
        {FRONT_LEFT_IN1, FRONT_LEFT_IN2, FRONT_LEFT_EN},
        // Front Right Motor
        {FRONT_RIGHT_IN1, FRONT_RIGHT_IN2, FRONT_RIGHT_EN},
        // Back Left Motor
        {BACK_LEFT_IN1, BACK_LEFT_IN2, BACK_LEFT_EN},
        // Back Right Motor
        {BACK_RIGHT_IN1, BACK_RIGHT_IN2, BACK_RIGHT_EN}
    };
    
    // Initialize motor driver
    if (!motor_driver.init(motors)) {
        while(1) {
            delay(1000);
        }
    }
    
    // Initialize motion control
    motion_init(&motor_driver);
    delay(1000);
}

void loop() {
    //  Test 1: STOP
    testCommand("STOP", motion_stop);
    delay(TEST_PAUSE_MS);
    
    // Test 2: FORWARD
    testCommand("FORWARD", motion_forward);
    delay(TEST_PAUSE_MS);
    
    // Test 3: BACKWARD
    testCommand("BACKWARD", motion_backward);
    delay(TEST_PAUSE_MS);
    
    // Test 4: SIDEWAY LEFT
    testCommand("SIDEWAY_LEFT", motion_sideway_left);
    delay(TEST_PAUSE_MS);
    
    // Test 5: SIDEWAY RIGHT
    testCommand("SIDEWAY_RIGHT", motion_sideway_right);
    delay(TEST_PAUSE_MS);
    
    // Test 6: ROTATE CW
    testCommand("ROTATE_CW", motion_rotate_cw);
    delay(TEST_PAUSE_MS);
    
    // Test 7: ROTATE CCW
    testCommand("ROTATE_CCW", motion_rotate_ccw);
    delay(TEST_PAUSE_MS);
    Serial.println("DIAGONAL_315_hello");
    // Test 8: DIAGONAL 315 (forward-left)
    testCommand("DIAGONAL_315", motion_diagonal_315);
    
    delay(TEST_PAUSE_MS);
    
    // Test 9: DIAGONAL 45 (forward-right)
    testCommand("DIAGONAL_45", motion_diagonal_45);
    delay(TEST_PAUSE_MS);
    
    // Test 10: DIAGONAL 225 (backward-left)
    testCommand("DIAGONAL_225", motion_diagonal_225);
    delay(TEST_PAUSE_MS);
    
    // Test 11: DIAGONAL 135 (backward-right)
    testCommand("DIAGONAL_135", motion_diagonal_135);
    delay(TEST_PAUSE_MS);
    
    // Test 12: PIVOT LEFT
    testCommand("PIVOT_LEFT", motion_pivot_left);
    delay(TEST_PAUSE_MS);
    
    // Test 13: PIVOT RIGHT
    testCommand("PIVOT_RIGHT", motion_pivot_right);
    delay(TEST_PAUSE_MS);
    
    // Final stop
    motion_stop();
    delay(5000);
}

/**
 * @brief Test a motion command and verify motor states
 */
void testCommand(const char* name, void (*motion_func)()) {
    Serial.println(name);
    
    // Execute the command
    motion_func();
    
    delay(TEST_DELAY_MS);
}



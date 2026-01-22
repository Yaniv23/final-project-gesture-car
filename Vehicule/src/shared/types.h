#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file types.h
 * @brief Shared data structures for inter-task communication
 */

// ============================================================================
// Motion Structures
// ============================================================================

/**
 * @brief Body velocity structure (for future kinematic control)
 */
struct BodyVelocity {
    float vx;      // m/s forward
    float vy;      // m/s right
    float omega;   // rad/s CCW
};

/**
 * @brief Wheel velocities structure (for future kinematic control)
 */
struct WheelVelocities {
    float front_left;
    float front_right;
    float back_left;
    float back_right;
};

/**
 * @brief Motor IDs
 */
enum MotorID {
    MOTOR_FRONT_LEFT = 0,
    MOTOR_FRONT_RIGHT = 1,
    MOTOR_BACK_LEFT = 2,
    MOTOR_BACK_RIGHT = 3
};

// ============================================================================
// Communication Structures
// ============================================================================

/**
 * @brief Raw ESP-NOW message structure
 */
struct ESPNowRawMessage {
    uint8_t first_byte;   // First byte of received message
    uint8_t second_byte;  // Second byte (0 if length < 2)
    uint8_t length;       // Total message length
};

#endif // SHARED_TYPES_H

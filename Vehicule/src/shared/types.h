#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

/**
 * @file types.h
 * @brief Shared data structures for inter-task communication
 * @details Used by both Person 1 (infrastructure) and Person 2 (control)
 */

/**
 * @brief Body velocity structure (used in motion commands)
 */
struct BodyVelocity {
    float vx;      // m/s forward
    float vy;      // m/s right
    float omega;   // rad/s CCW (counter-clockwise)
};

/**
 * @brief Wheel velocities structure
 */
struct WheelVelocities {
    float front_left;   // rad/s
    float front_right;  // rad/s
    float back_left;    // rad/s
    float back_right;   // rad/s
};

/**
 * @brief Motor IDs (used by both infrastructure and control)
 */
enum MotorID {
    MOTOR_FRONT_LEFT = 0,
    MOTOR_FRONT_RIGHT = 1,
    MOTOR_BACK_LEFT = 2,
    MOTOR_BACK_RIGHT = 3
};

/**
 * @brief Raw ESP-NOW message structure (for debugging)
 */
struct ESPNowRawMessage {
    uint8_t first_byte;   // First byte of received message
    uint8_t second_byte;  // Second byte (0 if message length < 2)
    uint8_t length;       // Total length of received message
};

#endif // SHARED_TYPES_H

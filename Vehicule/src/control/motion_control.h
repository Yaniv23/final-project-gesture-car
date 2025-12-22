#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include "../drivers/motor_driver.h"

/**
 * @file motion_control.h
 * @brief High-level motion control functions
 * @details Based on Vehicule_Controller.ino motion functions, uses MotorDriver
 */

/**
 * @brief Initialize motion control with MotorDriver
 * @param motor_driver Pointer to MotorDriver instance
 */
void motion_init(MotorDriver* motor_driver);

/**
 * @brief Stop all motors
 * @details Based on stop_motors() from Vehicule_Controller.ino
 */
void motion_stop();

/**
 * @brief Move forward
 * @details Based on Forward() from Vehicule_Controller.ino
 */
void motion_forward();

/**
 * @brief Move backward
 * @details Based on Backward() from Vehicule_Controller.ino
 */
void motion_backward();

/**
 * @brief Strafe left
 * @details Based on Sideway_Left() from Vehicule_Controller.ino
 */
void motion_strafe_left();

/**
 * @brief Strafe right
 * @details Based on Sideway_Right() from Vehicule_Controller.ino
 */
void motion_strafe_right();

/**
 * @brief Rotate clockwise
 * @details Based on rotate_cw() from Vehicule_Controller.ino
 */
void motion_rotate_cw();

/**
 * @brief Rotate counter-clockwise
 * @details Based on rotate_ccw() from Vehicule_Controller.ino
 */
void motion_rotate_ccw();

/**
 * @brief Diagonal forward left
 * @details Based on diagonal_forward_left() from Vehicule_Controller.ino
 */
void motion_diagonal_forward_left();

/**
 * @brief Diagonal forward right
 * @details Based on diagonal_forward_right() from Vehicule_Controller.ino
 */
void motion_diagonal_forward_right();

/**
 * @brief Diagonal backward left
 * @details Based on diagonal_backward_left() from Vehicule_Controller.ino
 */
void motion_diagonal_backward_left();

/**
 * @brief Diagonal backward right
 * @details Based on diagonal_backward_right() from Vehicule_Controller.ino
 */
void motion_diagonal_backward_right();

/**
 * @brief Pivot left (rotate around left side)
 * @details Based on pivot_left() from Vehicule_Controller.ino
 */
void motion_pivot_left();

/**
 * @brief Pivot right (rotate around right side)
 * @details Based on pivot_right() from Vehicule_Controller.ino
 */
void motion_pivot_right();

#endif // MOTION_CONTROL_H

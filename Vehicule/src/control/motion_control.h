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
 * @brief Sideway left
 * @details Based on Sideway_Left() from Vehicule_Controller.ino
 */
void motion_sideway_left();

/**
 * @brief Sideway right
 * @details Based on Sideway_Right() from Vehicule_Controller.ino
 */
void motion_sideway_right();

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
 * @brief Diagonal 315 degrees (forward-left)
 * @details Based on diagonal_forward_left() from Vehicule_Controller.ino
 */
void motion_diagonal_315();

/**
 * @brief Diagonal 45 degrees (forward-right)
 * @details Based on diagonal_forward_right() from Vehicule_Controller.ino
 */
void motion_diagonal_45();

/**
 * @brief Diagonal 225 degrees (backward-left)
 * @details Based on diagonal_backward_left() from Vehicule_Controller.ino
 */
void motion_diagonal_225();

/**
 * @brief Diagonal 135 degrees (backward-right)
 * @details Based on diagonal_backward_right() from Vehicule_Controller.ino
 */
void motion_diagonal_135();

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

#endif

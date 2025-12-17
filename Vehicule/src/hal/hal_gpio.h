/**
 * @file hal_gpio.h
 * @brief Hardware Abstraction Layer for GPIO (General Purpose Input/Output)
 * @details Platform-independent interface for GPIO operations
 * 
 * This HAL provides an abstraction for digital I/O operations, allowing the
 * code to be portable across different platforms.
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO pin direction
 */
typedef enum {
    HAL_GPIO_INPUT = 0,   ///< Input pin
    HAL_GPIO_OUTPUT = 1   ///< Output pin
} HAL_GPIO_Direction;

/**
 * @brief GPIO pin state
 */
typedef enum {
    HAL_GPIO_LOW = 0,     ///< Low state (0V)
    HAL_GPIO_HIGH = 1     ///< High state (3.3V/5V depending on platform)
} HAL_GPIO_State;

/**
 * @brief GPIO pull-up/pull-down configuration
 */
typedef enum {
    HAL_GPIO_FLOATING = 0,  ///< No pull-up/pull-down
    HAL_GPIO_PULLUP = 1,    ///< Internal pull-up resistor
    HAL_GPIO_PULLDOWN = 2   ///< Internal pull-down resistor
} HAL_GPIO_PullMode;

/**
 * @brief Initialize a GPIO pin
 * @param pin GPIO pin number
 * @param direction Pin direction (INPUT or OUTPUT)
 * @param pull_mode Pull-up/pull-down configuration (for input pins)
 * @return true if initialization successful, false otherwise
 * 
 * @note For output pins, pull_mode is typically ignored
 * @note For input pins, pull_mode helps with floating inputs
 */
bool HAL_GPIO_Init(uint8_t pin, HAL_GPIO_Direction direction, HAL_GPIO_PullMode pull_mode);

/**
 * @brief Set GPIO pin state (for output pins)
 * @param pin GPIO pin number
 * @param state Pin state (HIGH or LOW)
 * @return true if successful, false if pin not initialized or not an output
 */
bool HAL_GPIO_Write(uint8_t pin, HAL_GPIO_State state);

/**
 * @brief Read GPIO pin state (for input pins)
 * @param pin GPIO pin number
 * @return Pin state (HIGH or LOW), or LOW if pin not initialized
 */
HAL_GPIO_State HAL_GPIO_Read(uint8_t pin);

/**
 * @brief Toggle GPIO pin state (for output pins)
 * @param pin GPIO pin number
 * @return true if successful, false otherwise
 */
bool HAL_GPIO_Toggle(uint8_t pin);

/**
 * @brief Deinitialize a GPIO pin (free resources, set to high-impedance)
 * @param pin GPIO pin number
 * @return true if successful, false otherwise
 */
bool HAL_GPIO_Deinit(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif // HAL_GPIO_H


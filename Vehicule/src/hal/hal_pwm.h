/**
 * @file hal_pwm.h
 * @brief Hardware Abstraction Layer for PWM (Pulse Width Modulation)
 * @details Platform-independent interface for PWM operations
 * 
 * This HAL provides an abstraction for PWM signals, allowing the code to be
 * portable across different platforms. The ESP32 implementation uses the
 * Arduino analogWrite() function which maps to the ESP32 LEDC peripheral.
 */

#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize PWM on a specific pin
 * @param pin GPIO pin number
 * @param frequency PWM frequency in Hz (ESP32 default: 500Hz)
 * @param resolution PWM resolution in bits (ESP32: 1-16 bits, default: 8 bits = 0-255)
 * @return true if initialization successful, false otherwise
 * 
 * @note On ESP32, this sets up the LEDC channel for the pin
 * @note Frequency and resolution are hints - actual values depend on platform
 */
bool HAL_PWM_Init(uint8_t pin, uint32_t frequency, uint8_t resolution);

/**
 * @brief Set PWM duty cycle on a pin
 * @param pin GPIO pin number (must be initialized first)
 * @param duty_cycle Duty cycle value (0 to max_value based on resolution)
 *                   - For 8-bit resolution: 0-255
 *                   - For 10-bit resolution: 0-1023
 * @return true if successful, false if pin not initialized or invalid value
 * 
 * @note For motor control, typical range is 0-1023 (10-bit) or 0-255 (8-bit)
 * @note Setting duty_cycle to 0 effectively stops PWM output
 */
bool HAL_PWM_SetDuty(uint8_t pin, uint16_t duty_cycle);

/**
 * @brief Get current PWM duty cycle
 * @param pin GPIO pin number
 * @return Current duty cycle value, or 0 if pin not initialized
 */
uint16_t HAL_PWM_GetDuty(uint8_t pin);

/**
 * @brief Stop PWM output on a pin (set to 0)
 * @param pin GPIO pin number
 * @return true if successful, false otherwise
 */
bool HAL_PWM_Stop(uint8_t pin);

/**
 * @brief Deinitialize PWM on a pin (free resources)
 * @param pin GPIO pin number
 * @return true if successful, false otherwise
 */
bool HAL_PWM_Deinit(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif // HAL_PWM_H


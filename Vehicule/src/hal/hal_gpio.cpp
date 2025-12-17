/**
 * @file hal_gpio.cpp
 * @brief ESP32 implementation of HAL GPIO interface
 * @details Uses ESP32 Arduino framework's pinMode(), digitalWrite(), digitalRead()
 */

#include "hal_gpio.h"
#include <Arduino.h>

bool HAL_GPIO_Init(uint8_t pin, HAL_GPIO_Direction direction, HAL_GPIO_PullMode pull_mode) {
    // Convert HAL direction to Arduino pinMode
    uint8_t arduino_mode;
    
    if (direction == HAL_GPIO_INPUT) {
        // Input mode with pull configuration
        switch (pull_mode) {
            case HAL_GPIO_PULLUP:
                arduino_mode = INPUT_PULLUP;
                break;
            case HAL_GPIO_PULLDOWN:
                arduino_mode = INPUT_PULLDOWN;
                break;
            case HAL_GPIO_FLOATING:
            default:
                arduino_mode = INPUT;
                break;
        }
    } else {
        // Output mode (pull_mode ignored)
        arduino_mode = OUTPUT;
    }
    
    pinMode(pin, arduino_mode);
    return true;
}

bool HAL_GPIO_Write(uint8_t pin, HAL_GPIO_State state) {
    digitalWrite(pin, (state == HAL_GPIO_HIGH) ? HIGH : LOW);
    return true;
}

HAL_GPIO_State HAL_GPIO_Read(uint8_t pin) {
    int value = digitalRead(pin);
    return (value == HIGH) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
}

bool HAL_GPIO_Toggle(uint8_t pin) {
    HAL_GPIO_State current = HAL_GPIO_Read(pin);
    HAL_GPIO_State new_state = (current == HAL_GPIO_HIGH) ? HAL_GPIO_LOW : HAL_GPIO_HIGH;
    return HAL_GPIO_Write(pin, new_state);
}

bool HAL_GPIO_Deinit(uint8_t pin) {
    // Set pin to input (high-impedance) to deinitialize
    pinMode(pin, INPUT);
    return true;
}


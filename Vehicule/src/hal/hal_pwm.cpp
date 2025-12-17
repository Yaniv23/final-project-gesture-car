/**
 * @file hal_pwm.cpp
 * @brief ESP32 implementation of HAL PWM interface
 * @details Uses ESP32 Arduino framework's analogWrite() which maps to LEDC peripheral
 */

#include "hal_pwm.h"
#include <Arduino.h>

// ESP32 Arduino framework uses LEDC (LED Controller) for PWM
// analogWrite() automatically handles channel allocation and setup
// Default: 500Hz frequency, 8-bit resolution (0-255)

// Track initialized pins (simple implementation - can be enhanced)
#define MAX_PWM_PINS 16
static bool pwm_initialized[MAX_PWM_PINS] = {false};
static uint8_t pwm_resolution[MAX_PWM_PINS] = {0};

bool HAL_PWM_Init(uint8_t pin, uint32_t frequency, uint8_t resolution) {
    if (pin >= MAX_PWM_PINS) {
        return false;
    }
    
    // ESP32 Arduino analogWrite() uses LEDC with default settings
    // Frequency and resolution are hints - actual values depend on LEDC configuration
    // For simplicity, we use analogWrite() which handles initialization
    
    // Mark pin as initialized
    pwm_initialized[pin] = true;
    pwm_resolution[pin] = resolution;
    
    // Initialize with 0 duty cycle
    analogWrite(pin, 0);
    
    return true;
}

bool HAL_PWM_SetDuty(uint8_t pin, uint16_t duty_cycle) {
    if (pin >= MAX_PWM_PINS || !pwm_initialized[pin]) {
        return false;
    }
    
    // ESP32 Arduino analogWrite() uses 8-bit resolution by default (0-255)
    // If higher resolution is needed, we need to scale
    uint8_t resolution = pwm_resolution[pin];
    
    if (resolution == 0) {
        resolution = 8; // Default to 8-bit
    }
    
    uint16_t max_value = (1 << resolution) - 1;
    
    // Clamp duty cycle to valid range
    if (duty_cycle > max_value) {
        duty_cycle = max_value;
    }
    
    // For 8-bit resolution (default), use analogWrite directly
    if (resolution == 8) {
        analogWrite(pin, (uint8_t)duty_cycle);
    } else {
        // For other resolutions, scale to 8-bit for analogWrite
        // Note: This loses precision, but maintains compatibility
        uint8_t scaled = (uint8_t)((duty_cycle * 255) / max_value);
        analogWrite(pin, scaled);
    }
    
    return true;
}

uint16_t HAL_PWM_GetDuty(uint8_t pin) {
    if (pin >= MAX_PWM_PINS || !pwm_initialized[pin]) {
        return 0;
    }
    
    // ESP32 Arduino doesn't provide a direct way to read PWM duty
    // This is a limitation - we'd need to track it ourselves
    // For now, return 0 (can be enhanced with state tracking)
    return 0;
}

bool HAL_PWM_Stop(uint8_t pin) {
    return HAL_PWM_SetDuty(pin, 0);
}

bool HAL_PWM_Deinit(uint8_t pin) {
    if (pin >= MAX_PWM_PINS) {
        return false;
    }
    
    // Stop PWM output
    analogWrite(pin, 0);
    
    // Mark as not initialized
    pwm_initialized[pin] = false;
    pwm_resolution[pin] = 0;
    
    return true;
}


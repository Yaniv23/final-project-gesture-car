/**
 * @file hal_timer.cpp
 * @brief ESP32 implementation of HAL Timer interface
 * @details Uses ESP32 Arduino framework's timing functions and ESP32 watchdog
 */

#include "hal_timer.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

// Watchdog state tracking
static bool watchdog_initialized = false;
static uint32_t watchdog_timeout_ms = 0;

uint32_t HAL_Timer_GetMillis(void) {
    return (uint32_t)millis();
}

uint64_t HAL_Timer_GetMicros(void) {
    return (uint64_t)micros();
}

void HAL_Timer_DelayMs(uint32_t ms) {
    delay(ms);
}

void HAL_Timer_DelayUs(uint32_t us) {
    delayMicroseconds(us);
}

bool HAL_Timer_WatchdogInit(const HAL_Timer_WatchdogConfig* config) {
    if (config == NULL) {
        return false;
    }
    
    if (!config->enable) {
        return HAL_Timer_WatchdogDisable();
    }
    
    // ESP32 uses Task Watchdog Timer (TWDT)
    // Convert milliseconds to seconds (ESP32 TWDT uses seconds)
    uint32_t timeout_seconds = (config->timeout_ms + 999) / 1000; // Round up
    
    // Minimum timeout is 1 second
    if (timeout_seconds < 1) {
        timeout_seconds = 1;
    }
    
    // Initialize task watchdog
    esp_err_t err = esp_task_wdt_init(timeout_seconds, true);
    if (err != ESP_OK) {
        return false;
    }
    
    // Add current task to watchdog
    err = esp_task_wdt_add(NULL);
    if (err != ESP_OK) {
        return false;
    }
    
    watchdog_initialized = true;
    watchdog_timeout_ms = config->timeout_ms;
    
    return true;
}

bool HAL_Timer_WatchdogFeed(void) {
    if (!watchdog_initialized) {
        return false;
    }
    
    // Reset the watchdog timer
    esp_err_t err = esp_task_wdt_reset();
    return (err == ESP_OK);
}

bool HAL_Timer_WatchdogDisable(void) {
    if (watchdog_initialized) {
        // Remove current task from watchdog
        esp_task_wdt_delete(NULL);
        
        // Deinitialize watchdog (optional - can leave it running)
        // esp_task_wdt_deinit();
        
        watchdog_initialized = false;
        watchdog_timeout_ms = 0;
    }
    
    return true;
}


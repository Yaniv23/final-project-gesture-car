#include "mode_manager.h"
#include <Arduino.h>

ModeManager* ModeManager::instance_ = nullptr;

ModeManager::ModeManager() 
    : current_mode_(MODE_MANUAL) {
    mode_mutex_ = xSemaphoreCreateMutex();
    if (mode_mutex_ == NULL) {
        // Error handling - mutex creation failed
        // In production, this should be handled more gracefully
    }
    // Log initial mode (will be printed when Serial is ready)
    // Note: Serial may not be initialized yet, so we log in getInstance() instead
}

ModeManager& ModeManager::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new ModeManager();
        // Log initial mode (Serial should be ready by the time this is called)
        Serial.println("[MODE] ModeManager initialized - Default mode: MANUAL");
    }
    return *instance_;
}

DrivingMode ModeManager::getCurrentMode() const {
    if (xSemaphoreTake(mode_mutex_, portMAX_DELAY) == pdTRUE) {
        DrivingMode mode = current_mode_;
        xSemaphoreGive(mode_mutex_);
        return mode;
    }
    return MODE_MANUAL; // Default fallback
}

bool ModeManager::setMode(DrivingMode mode) {
    if (xSemaphoreTake(mode_mutex_, portMAX_DELAY) == pdTRUE) {
        current_mode_ = mode;
        xSemaphoreGive(mode_mutex_);
        return true;
    }
    return false;
}

bool ModeManager::isManualMode() const {
    return getCurrentMode() == MODE_MANUAL;
}

bool ModeManager::isAutonomousMode() const {
    return getCurrentMode() == MODE_AUTONOMOUS;
}

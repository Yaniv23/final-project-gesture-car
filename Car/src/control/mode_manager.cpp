#include "mode_manager.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

ModeManager* ModeManager::instance_ = nullptr;

ModeManager::ModeManager()
    : current_mode_(MODE_AUTONOMOUS),
      task_handle_autonomous_(NULL),
      task_handle_sensors_(NULL) {
    mode_mutex_ = xSemaphoreCreateMutex();
    if (mode_mutex_ == NULL) {
        // Error handling - mutex creation failed
        // In production, this should be handled more gracefully
    }
}

ModeManager& ModeManager::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new ModeManager();
    }
    return *instance_;
}

void ModeManager::registerTaskHandles(TaskHandle_t autonomous, TaskHandle_t sensors) {
    task_handle_autonomous_ = autonomous;
    task_handle_sensors_ = sensors;
}

DrivingMode ModeManager::getCurrentMode() const {
    if (xSemaphoreTake(mode_mutex_, portMAX_DELAY) == pdTRUE) {
        DrivingMode mode = current_mode_;
        xSemaphoreGive(mode_mutex_);
        return mode;
    }
    return MODE_MANUAL; // Default fallback
}

void ModeManager::applyTaskActivationForMode(DrivingMode new_mode, DrivingMode old_mode) {
    if (task_handle_autonomous_ == NULL || task_handle_sensors_ == NULL) {
        return;
    }
    if (new_mode == MODE_MANUAL && old_mode != MODE_MANUAL) {
        vTaskSuspend(task_handle_autonomous_);
        vTaskSuspend(task_handle_sensors_);
    } else if (new_mode == MODE_AUTONOMOUS && old_mode != MODE_AUTONOMOUS) {
        // Resume sensors first so they start updating SensorState before autonomous runs
        vTaskResume(task_handle_sensors_);
        vTaskDelay(pdMS_TO_TICKS(150));  // Let sensor task do 1–2 reads and update SensorState
        vTaskResume(task_handle_autonomous_);
        Serial.println("[MODE] Autonomous: Sensors and Autonomous tasks resumed");
    }
}

bool ModeManager::setMode(DrivingMode mode) {
    if (xSemaphoreTake(mode_mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    DrivingMode old_mode = current_mode_;
    current_mode_ = mode;
    xSemaphoreGive(mode_mutex_);

    applyTaskActivationForMode(mode, old_mode);
    return true;
}

bool ModeManager::isManualMode() const {
    return getCurrentMode() == MODE_MANUAL;
}

bool ModeManager::isAutonomousMode() const {
    return getCurrentMode() == MODE_AUTONOMOUS;
}

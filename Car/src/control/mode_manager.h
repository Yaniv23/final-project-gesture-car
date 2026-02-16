#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

enum DrivingMode {
    MODE_MANUAL = 0,
    MODE_AUTONOMOUS = 1
};

class ModeManager {
public:
    static ModeManager& getInstance();

    /// Register task handles so ModeManager can suspend/resume them on mode change.
    /// Call once from main after creating the Sensors and Autonomous tasks.
    void registerTaskHandles(TaskHandle_t autonomous, TaskHandle_t sensors);

    DrivingMode getCurrentMode() const;
    bool setMode(DrivingMode mode);
    bool isManualMode() const;
    bool isAutonomousMode() const;

private:
    ModeManager();
    ~ModeManager() = default;
    ModeManager(const ModeManager&) = delete;
    ModeManager& operator=(const ModeManager&) = delete;

    void applyTaskActivationForMode(DrivingMode new_mode, DrivingMode old_mode);

    DrivingMode current_mode_;
    mutable SemaphoreHandle_t mode_mutex_;
    TaskHandle_t task_handle_autonomous_;
    TaskHandle_t task_handle_sensors_;
    static ModeManager* instance_;
};

#endif // MODE_MANAGER_H

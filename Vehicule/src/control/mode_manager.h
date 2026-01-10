#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum DrivingMode {
    MODE_MANUAL = 0,
    MODE_AUTONOMOUS = 1
};

class ModeManager {
public:
    static ModeManager& getInstance();
    
    DrivingMode getCurrentMode() const;
    bool setMode(DrivingMode mode);
    bool isManualMode() const;
    bool isAutonomousMode() const;
    
private:
    ModeManager();
    ~ModeManager() = default;
    ModeManager(const ModeManager&) = delete;
    ModeManager& operator=(const ModeManager&) = delete;
    
    DrivingMode current_mode_;
    mutable SemaphoreHandle_t mode_mutex_;
    static ModeManager* instance_;
};

#endif // MODE_MANAGER_H

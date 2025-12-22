/**
 * @file emergency_stop.cpp
 * @brief Emergency stop implementation (stub for now)
 */

#include "emergency_stop.h"
#include "../shared/queues.h"

static bool estop_active_ = false;

bool emergency_stop_init() {
    estop_active_ = false;
    return true;
}

void emergency_stop_trigger() {
    estop_active_ = true;
    // Take safety semaphore (prevent motor movement)
    if (xSafetySemaphore != NULL) {
        xSemaphoreTake(xSafetySemaphore, 0);
    }
}

void emergency_stop_clear() {
    estop_active_ = false;
    // Give safety semaphore (allow motor movement)
    if (xSafetySemaphore != NULL) {
        xSemaphoreGive(xSafetySemaphore);
    }
}

bool emergency_stop_is_active() {
    return estop_active_;
}

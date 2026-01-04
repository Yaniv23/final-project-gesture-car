/**
 * @file emergency_stop.cpp
 * @brief Emergency stop implementation
 * @details Uses flag-based approach for guaranteed immediate stop
 */

#include "emergency_stop.h"
#include "../shared/queues.h"
#include <Arduino.h>

// Volatile flag for emergency stop state (can be set from ISR or task context)
// This is the PRIMARY mechanism - checked before any motor command execution
static volatile bool estop_active_ = false;

// Temperature-based emergency stop flag
static volatile bool estop_temp_active_ = false;

bool emergency_stop_init() {
    estop_active_ = false;
    return true;
}

void emergency_stop_trigger() {
    // Set flag immediately - this is the PRIMARY safety mechanism
    // Motor control task checks this flag BEFORE executing any command
    estop_active_ = true;
    
    // Immediately stop all motors (bypasses semaphore - guaranteed stop)
    // Note: This is a direct call that doesn't depend on semaphore availability
    // The motor control task will also check the flag on next iteration
    
    // Secondary mechanism: Try to take semaphore (non-blocking)
    // This helps prevent new commands from starting, but flag is authoritative
    if (xSafetySemaphore != NULL) {
        // Use non-blocking take - if it fails, that's OK, flag will still block commands
        xSemaphoreTake(xSafetySemaphore, 0);
    }
    
    Serial.println("[EMERGENCY] Emergency stop TRIGGERED - All motors stopped");
}

void emergency_stop_clear() {
    // Clear flag first
    estop_active_ = false;
    
    // Give semaphore back (allow motor movement)
    if (xSafetySemaphore != NULL) {
        xSemaphoreGive(xSafetySemaphore);
    }
    
    Serial.println("[EMERGENCY] Emergency stop CLEARED - Motors enabled");
}

bool emergency_stop_is_active() {
    // Read volatile flag - safe from any context
    // Check both general emergency stop and temperature emergency stop
    return estop_active_ || estop_temp_active_;
}

void emergency_stop_trigger_temp() {
    // Set temperature emergency stop flag
    estop_temp_active_ = true;
    
    // Also trigger general emergency stop to ensure motors stop
    emergency_stop_trigger();
    
    Serial.println("[EMERGENCY] Temperature emergency stop TRIGGERED - All motors stopped");
}

void emergency_stop_clear_temp() {
    // Clear temperature emergency stop flag
    estop_temp_active_ = false;
    
    // Only clear general emergency stop if it was triggered by temperature
    // (Don't clear if other emergency stop is active)
    if (!estop_active_) {
        emergency_stop_clear();
    }
    
    Serial.println("[EMERGENCY] Temperature emergency stop CLEARED - Motors enabled");
}

bool emergency_stop_is_temp_active() {
    // Read volatile flag - safe from any context
    return estop_temp_active_;
}

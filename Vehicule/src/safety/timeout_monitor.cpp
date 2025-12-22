/**
 * @file timeout_monitor.cpp
 * @brief Timeout monitor implementation (stub for now)
 */

#include "timeout_monitor.h"

static uint32_t timeout_ms_ = 500;
static uint32_t last_command_time_ = 0;

bool timeout_monitor_init(uint32_t timeout_ms) {
    timeout_ms_ = timeout_ms;
    last_command_time_ = millis();
    return true;
}

void timeout_monitor_reset() {
    last_command_time_ = millis();
}

bool timeout_monitor_check() {
    // TODO: Check if timeout exceeded
    return false;  // No timeout
}

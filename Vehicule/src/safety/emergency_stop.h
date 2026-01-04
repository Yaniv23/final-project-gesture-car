#ifndef EMERGENCY_STOP_H
#define EMERGENCY_STOP_H

#include <stdbool.h>

/**
 * @file emergency_stop.h
 * @brief Emergency stop system
 */

bool emergency_stop_init();
void emergency_stop_trigger();
void emergency_stop_clear();
bool emergency_stop_is_active();

// Temperature-based emergency stop
void emergency_stop_trigger_temp();
void emergency_stop_clear_temp();
bool emergency_stop_is_temp_active();

#endif // EMERGENCY_STOP_H

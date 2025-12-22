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

#endif // EMERGENCY_STOP_H

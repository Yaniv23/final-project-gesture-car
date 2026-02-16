#ifndef TIMEOUT_MONITOR_H
#define TIMEOUT_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file timeout_monitor.h
 * @brief Command timeout monitoring
 */

bool timeout_monitor_init(uint32_t timeout_ms);
void timeout_monitor_reset();
bool timeout_monitor_check();

#endif // TIMEOUT_MONITOR_H

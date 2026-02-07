#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file watchdog.h
 * @brief Hardware watchdog timer interface
 */

bool watchdog_init(uint32_t timeout_ms);
void watchdog_feed();

#endif // WATCHDOG_H

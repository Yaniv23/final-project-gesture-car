/**
 * @file hal_timer.h
 * @brief Hardware Abstraction Layer for Timers and Watchdog
 * @details Platform-independent interface for timing operations
 * 
 * This HAL provides an abstraction for timing operations including:
 * - System time (milliseconds since boot)
 * - Delays (blocking and non-blocking)
 * - Watchdog timer (safety mechanism)
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get system time in milliseconds since boot
 * @return Milliseconds since system boot (wraps around after ~49 days)
 * 
 * @note This is a monotonic clock (always increasing)
 * @note Use for timing intervals, not absolute time
 */
uint32_t HAL_Timer_GetMillis(void);

/**
 * @brief Get system time in microseconds since boot
 * @return Microseconds since system boot
 * 
 * @note Higher precision than GetMillis()
 * @note Useful for precise timing measurements
 */
uint64_t HAL_Timer_GetMicros(void);

/**
 * @brief Blocking delay in milliseconds
 * @param ms Milliseconds to delay
 * 
 * @warning This is a blocking call - use carefully in FreeRTOS tasks
 * @note In FreeRTOS, prefer vTaskDelay() for non-blocking delays
 */
void HAL_Timer_DelayMs(uint32_t ms);

/**
 * @brief Blocking delay in microseconds
 * @param us Microseconds to delay
 * 
 * @note Precision depends on platform (ESP32: ~1us precision)
 * @warning Very short delays may not be accurate
 */
void HAL_Timer_DelayUs(uint32_t us);

/**
 * @brief Watchdog timer configuration
 */
typedef struct {
    uint32_t timeout_ms;  ///< Watchdog timeout in milliseconds
    bool enable;             ///< Enable/disable watchdog
} HAL_Timer_WatchdogConfig;

/**
 * @brief Initialize and start watchdog timer
 * @param config Watchdog configuration
 * @return true if successful, false otherwise
 * 
 * @note Watchdog must be fed periodically to prevent system reset
 * @note Typical timeout: 5-10 seconds for embedded systems
 */
bool HAL_Timer_WatchdogInit(const HAL_Timer_WatchdogConfig* config);

/**
 * @brief Feed (reset) the watchdog timer
 * @return true if successful, false if watchdog not initialized
 * 
 * @note Must be called before timeout expires to prevent reset
 * @note Should be called from main control loop or safety task
 */
bool HAL_Timer_WatchdogFeed(void);

/**
 * @brief Disable watchdog timer
 * @return true if successful, false otherwise
 */
bool HAL_Timer_WatchdogDisable(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_TIMER_H


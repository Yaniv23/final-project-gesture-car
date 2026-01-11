#ifndef SHARED_QUEUES_H
#define SHARED_QUEUES_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "types.h"

/**
 * @file queues.h
 * @brief FreeRTOS queues and semaphores for inter-task communication
 * @details Shared between communication, control, and telemetry tasks
 */

// Queue: ESP-NOW ISR → Communication Task
// Holds ESPNowRawMessage from ESP-NOW callback (includes first byte + length for debugging)
extern QueueHandle_t xESPNowQueue;

// Queue: Communication Task → Motor Control Task
// Holds uint8_t command bytes (validated commands)
extern QueueHandle_t xCommandQueue;

// Semaphore: Safety Monitor → Motor Control Task
// Semaphore available (can be taken) = safe to move; semaphore taken (unavailable) = emergency stop active
extern SemaphoreHandle_t xSafetySemaphore;

/**
 * @brief Initialize all shared queues and semaphores
 * @return true if successful, false otherwise
 */
bool initSharedQueues();

#endif // SHARED_QUEUES_H

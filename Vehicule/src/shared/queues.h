#ifndef SHARED_QUEUES_H
#define SHARED_QUEUES_H

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include "types.h"

/**
 * @file queues.h
 * @brief FreeRTOS queues and semaphores for inter-task communication
 * @details Shared between communication, control, and telemetry tasks
 */

// Queue: ESP-NOW ISR → Communication Task
// Holds uint8_t command bytes from ESP-NOW callback
extern QueueHandle_t xESPNowQueue;

// Queue: Communication Task → Motor Control Task
// Holds uint8_t command bytes (validated commands)
extern QueueHandle_t xCommandQueue;

// Queue: Communication Task → Motor Control Task (legacy - for BodyVelocity)
// Holds BodyVelocity commands from PC/gesture system
extern QueueHandle_t xMotionCommandQueue;

// Queue: Motor Control Task → Telemetry Task (optional)
// Holds WheelVelocities + PWM values for status reporting
extern QueueHandle_t xMotorStatusQueue;

// Semaphore: Safety Monitor → Motor Control Task
// Take = safe to move, Give = emergency stop
extern SemaphoreHandle_t xSafetySemaphore;

/**
 * @brief Initialize all shared queues and semaphores
 * @return true if successful, false otherwise
 */
bool initSharedQueues();

#endif // SHARED_QUEUES_H

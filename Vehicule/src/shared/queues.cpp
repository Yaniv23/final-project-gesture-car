/**
 * @file queues.cpp
 * @brief Implementation of shared FreeRTOS queues and semaphores
 */

#include "queues.h"

// Queue: ESP-NOW ISR → Communication Task
QueueHandle_t xESPNowQueue = NULL;

// Queue: Communication Task → Motor Control Task
QueueHandle_t xCommandQueue = NULL;

// Queue: Communication Task → Motor Control Task (legacy - for BodyVelocity)
QueueHandle_t xMotionCommandQueue = NULL;

// Queue: Motor Control Task → Telemetry Task
QueueHandle_t xMotorStatusQueue = NULL;

// Semaphore: Safety Monitor → Motor Control Task
SemaphoreHandle_t xSafetySemaphore = NULL;

bool initSharedQueues() {
    // Create ESP-NOW queue (ISR context, holds uint8_t command bytes)
    // Queue size: 5 commands
    xESPNowQueue = xQueueCreate(5, sizeof(uint8_t));
    if (xESPNowQueue == NULL) {
        return false;
    }
    
    // Create command queue (Communication Task → Motor Control Task)
    // Queue size: 5 commands, holds uint8_t command bytes
    xCommandQueue = xQueueCreate(5, sizeof(uint8_t));
    if (xCommandQueue == NULL) {
        return false;
    }

    // Create motion command queue (holds BodyVelocity) - legacy, may not be used
    // Queue size: 5 commands, each is sizeof(BodyVelocity) = 12 bytes
    xMotionCommandQueue = xQueueCreate(5, sizeof(BodyVelocity));
    if (xMotionCommandQueue == NULL) {
        return false;
    }

    // Create motor status queue (holds WheelVelocities)
    // Queue size: 3 status updates
    xMotorStatusQueue = xQueueCreate(3, sizeof(WheelVelocities));
    if (xMotorStatusQueue == NULL) {
        return false;
    }

    // Create safety semaphore (binary semaphore)
    // Initially given (safe to move)
    xSafetySemaphore = xSemaphoreCreateBinary();
    if (xSafetySemaphore == NULL) {
        return false;
    }
    
    // Give semaphore initially (system starts in safe state)
    xSemaphoreGive(xSafetySemaphore);

    return true;
}

/**
 * @file queues.cpp
 * @brief Implementation of shared FreeRTOS queues and semaphores
 */

#include "queues.h"
#include <Arduino.h>

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
    // Create ESP-NOW queue (ISR context, holds ESPNowRawMessage for debugging)
    // Queue size: 5 messages
    xESPNowQueue = xQueueCreate(5, sizeof(ESPNowRawMessage));
    if (xESPNowQueue == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xESPNowQueue");
        return false;
    }
    
    // Create command queue (Communication Task → Motor Control Task)
    // Queue size: 5 commands, holds uint8_t command bytes
    xCommandQueue = xQueueCreate(5, sizeof(uint8_t));
    if (xCommandQueue == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xCommandQueue");
        return false;
    }

    // Create motion command queue (holds BodyVelocity) - reserved for future use
    // Queue size: 5 commands, each is sizeof(BodyVelocity) = 12 bytes
    // NOTE: Currently unused - reserved for future kinematics/velocity control implementation
    xMotionCommandQueue = xQueueCreate(5, sizeof(BodyVelocity));
    if (xMotionCommandQueue == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xMotionCommandQueue");
        return false;
    }

    // Create motor status queue (holds WheelVelocities)
    // Queue size: 3 status updates
    xMotorStatusQueue = xQueueCreate(3, sizeof(WheelVelocities));
    if (xMotorStatusQueue == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xMotorStatusQueue");
        return false;
    }

    // Create safety semaphore (binary semaphore)
    // Initially given (safe to move)
    xSafetySemaphore = xSemaphoreCreateBinary();
    if (xSafetySemaphore == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xSafetySemaphore");
        return false;
    }
    
    // Give semaphore initially (system starts in safe state)
    xSemaphoreGive(xSafetySemaphore);

    return true;
}

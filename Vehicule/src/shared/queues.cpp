/**
 * @file queues.cpp
 * @brief Implementation of shared FreeRTOS queues and semaphores
 */

#include "queues.h"
#include "sensor_state.h"
#include <Arduino.h>

// Queue: ESP-NOW ISR → Communication Task
QueueHandle_t xESPNowQueue = NULL;

// Queue: Communication Task → Motor Control Task
QueueHandle_t xCommandQueue = NULL;

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

    // Create safety semaphore (binary semaphore)
    // Initially given (safe to move)
    xSafetySemaphore = xSemaphoreCreateBinary();
    if (xSafetySemaphore == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xSafetySemaphore");
        return false;
    }
    
    // Give semaphore initially (system starts in safe state)
    xSemaphoreGive(xSafetySemaphore);
    
    // Initialize sensor state (mutex for thread-safe sensor readings)
    if (!initSensorState()) {
        Serial.println("[ERROR] Queues: Failed to initialize sensor state");
        return false;
    }
    
    return true;
}

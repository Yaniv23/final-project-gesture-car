/**
 * @file queues.cpp
 * @brief Implementation of shared FreeRTOS queues and semaphores
 */

#include "queues.h"
#include "sensor_state.h"
#include <Arduino.h>

QueueHandle_t xESPNowQueue = NULL;
QueueHandle_t xCommandQueue = NULL;
SemaphoreHandle_t xSafetySemaphore = NULL;

bool initSharedQueues() {
    xESPNowQueue = xQueueCreate(5, sizeof(ESPNowRawMessage));
    if (xESPNowQueue == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xESPNowQueue");
        return false;
    }

    xCommandQueue = xQueueCreate(5, sizeof(uint8_t));
    if (xCommandQueue == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xCommandQueue");
        return false;
    }

    xSafetySemaphore = xSemaphoreCreateBinary();
    if (xSafetySemaphore == NULL) {
        Serial.println("[ERROR] Queues: Failed to create xSafetySemaphore");
        return false;
    }
    xSemaphoreGive(xSafetySemaphore);

    if (!initSensorState()) {
        Serial.println("[ERROR] Queues: Failed to initialize sensor state");
        return false;
    }
    return true;
}

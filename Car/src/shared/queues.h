#ifndef SHARED_QUEUES_H
#define SHARED_QUEUES_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "types.h"

extern QueueHandle_t xESPNowQueue;
extern QueueHandle_t xCommandQueue;
extern SemaphoreHandle_t xSafetySemaphore;

bool initSharedQueues();

#endif

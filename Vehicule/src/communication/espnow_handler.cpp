/**
 * @file espnow_handler.cpp
 * @brief ESP-NOW handler implementation for binary command protocol
 * @details Based on Vehicule_Controller.ino ESP-NOW implementation
 */

#include <Arduino.h>
#include "espnow_handler.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "../shared/queues.h"

/**
 * @brief ESP-NOW receive callback (called from ISR context)
 * @param recvInfo Reception information (MAC address, etc.)
 * @param data Received data buffer
 * @param len Length of received data
 * @note This is called from ISR context - keep it minimal!
 */
void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
    // Expect exactly 1 byte (binary command)
    if (len != sizeof(uint8_t)) {
        // Invalid packet size - ignore
        // Note: Can't use Serial.println here (ISR context)
        return;
    }
    
    uint8_t cmd_byte = data[0];
    
    // Validate command
    if (!isValidCommand(cmd_byte)) {
        return;
    }
    
    // Send to queue from ISR context
    // This is safe because we're using xQueueSendFromISR
    if (xESPNowQueue != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(xESPNowQueue, &cmd_byte, &xHigherPriorityTaskWoken);

        // If queue is full, command is dropped (but that's OK - we'll get the next one)
        // Yield if a higher priority task was woken
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

bool espnow_init() {
    // Set WiFi to station mode (matching Vehicule_Controller.ino)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();  // Disconnect from any previous connection
    delay(200);  // Give WiFi time to initialize
    
    // Print MAC address using esp_wifi_get_mac (matching Vehicule_Controller.ino)
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Initialize ESP-NOW (idempotent - can be called multiple times)
    esp_err_t init_result = esp_now_init();
    if (init_result == ESP_ERR_ESPNOW_EXIST) {
        // ESP-NOW already initialized - this is OK
        return true;
    } else if (init_result != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW: Initialization failed");
        return false;
    }
    
    // Register receive callback (idempotent - can be called multiple times)
    esp_now_register_recv_cb(onESPNowReceive);
    
    return true;
}

bool espnow_get_mac_string(char* mac_str, size_t len) {
    if (mac_str == NULL || len < 18) {
        return false;
    }
    
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) {
        return false;
    }
    
    snprintf(mac_str, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return true;
}

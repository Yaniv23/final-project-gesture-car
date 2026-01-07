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
#include "../shared/types.h"
#include "../config.h"

// Connection status flag (set when first message is received)
static volatile bool espnow_connected = false;

/**
 * @brief ESP-NOW receive callback (called from ISR context)
 * @param recvInfo Reception information (MAC address, etc.)
 * @param data Received data buffer
 * @param len Length of received data
 * @note This is called from ISR context - keep it minimal!
 */
void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
    // Mark connection as established when first message is received
    if (!espnow_connected && len > 0) {
        espnow_connected = true;
    }
    
    // Always queue the received data for debugging, regardless of length
    // The communication task will validate and print all details
    if (xESPNowQueue != NULL && len > 0) {
        ESPNowRawMessage msg;
        msg.first_byte = data[0];
        msg.second_byte = (len > 1) ? data[1] : 0;
        msg.length = (len > 255) ? 255 : len;  // Cap at 255 for uint8_t
        
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // Queue the message (communication task will handle validation and printing)
        xQueueSendFromISR(xESPNowQueue, &msg, &xHigherPriorityTaskWoken);

        // If queue is full, command is dropped (but that's OK - we'll get the next one)
        // Yield if a higher priority task was woken
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

bool espnow_init(bool simulation_mode) {
    // In simulation mode, skip WiFi initialization entirely (non-blocking)
    if (simulation_mode) {
        Serial.println("\n[SIM_MODE] 🧪 SIMULATION MODE ACTIVE");
        Serial.println("[SIM_MODE] ESP-NOW receiver NOT required");
        Serial.println("[SIM_MODE] Skipping WiFi initialization (simulation mode)\n");
        return true;
    }
    
    // Normal mode: Initialize WiFi and ESP-NOW
    Serial.println("🔧 ESP32 set to STA mode");
    
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

bool espnow_is_connected() {
    return espnow_connected;
}

bool espnow_wait_for_connection(uint32_t timeout_ms) {
    // In simulation mode, return immediately
    #if SIMULATION_MODE
    return true;
    #endif
    
    // Reset connection flag
    espnow_connected = false;
    
    uint32_t start_time = millis();
    uint32_t last_status_time = start_time;
    const uint32_t status_interval_ms = 2000;  // Print status every 2 seconds
    
    Serial.println("[ESP-NOW] Waiting for connection from sender...");
    
    while (millis() - start_time < timeout_ms) {
        if (espnow_connected) {
            Serial.println("[ESP-NOW] ✓ Connection established!");
            return true;
        }
        
        // Print status updates periodically
        uint32_t current_time = millis();
        if (current_time - last_status_time >= status_interval_ms) {
            uint32_t elapsed = current_time - start_time;
            uint32_t remaining = (timeout_ms > elapsed) ? (timeout_ms - elapsed) : 0;
            Serial.print("[ESP-NOW] Waiting... (");
            Serial.print(elapsed / 1000);
            Serial.print("s elapsed, ");
            Serial.print(remaining / 1000);
            Serial.println("s remaining)");
            last_status_time = current_time;
        }
        
        delay(100);  // Poll every 100ms
    }
    
    Serial.println("[ESP-NOW] ✗ Connection timeout - no message received");
    return false;
}

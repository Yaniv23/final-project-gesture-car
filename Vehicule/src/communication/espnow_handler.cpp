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
#include <string.h>
#include "../shared/queues.h"

// Store sender MAC address (set when we receive first command)
static uint8_t sender_mac[6] = {0};
static bool sender_mac_set = false;

/**
 * @brief ESP-NOW receive callback (called from ISR context)
 * @param recvInfo Reception information (MAC address, etc.)
 * @param data Received data buffer
 * @param len Length of received data
 * @note This is called from ISR context - keep it minimal!
 */
void onESPNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
    // Store sender MAC address (for sending temperature back)
    // This is safe to do from ISR context (just copying 6 bytes)
    if (!sender_mac_set && mac_addr != NULL) {
        memcpy(sender_mac, mac_addr, 6);
        sender_mac_set = true;
    }
    
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
    Serial.println("🔧 ESP32 set to STA mode");
    
    // Print MAC address using esp_wifi_get_mac (matching Vehicule_Controller.ino)
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    Serial.print("📡 MAC Address: ");
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println(macStr);
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW: Initialization failed");
        return false;
    }
    
    // Register receive callback
    esp_now_register_recv_cb(onESPNowReceive);
    Serial.println("🟢 Ready to receive ESP-NOW messages");
    Serial.println("🟢 Binary command protocol initialized");
    Serial.println("\n📡 Waiting for ESP-NOW commands...");
    Serial.println("   (Send binary commands from sender ESP32)\n");
    
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

bool espnow_send_temperature(float temperature, uint8_t state) {
    // Check if sender MAC is known
    if (!sender_mac_set) {
        // Sender MAC not yet known - can't send
        return false;
    }
    
    // Create temperature message
    temp_message temp_msg;
    temp_msg.temperature = temperature;
    temp_msg.state = state;
    
    // Add sender as peer if not already added
    esp_now_peer_info_t peerInfo;
    if (esp_now_get_peer(sender_mac, &peerInfo) != ESP_OK) {
        // Peer not found, add it
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, sender_mac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("[ESP-NOW] Failed to add sender peer for temperature");
            return false;
        }
    }
    
    // Send temperature message
    esp_err_t result = esp_now_send(sender_mac, (uint8_t*)&temp_msg, sizeof(temp_msg));
    
    if (result == ESP_OK) {
        return true;
    } else {
        Serial.print("[ESP-NOW] Failed to send temperature: ");
        Serial.println(result);
        return false;
    }
}

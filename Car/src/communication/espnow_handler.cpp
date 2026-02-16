/**
 * @file espnow_handler.cpp
 * @brief ESP-NOW handler implementation for binary command protocol
 * @details Based on legacy Vehicle_Controller.ino ESP-NOW implementation
 */

#include <Arduino.h>
#include "espnow_handler.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include "../shared/queues.h"
#include "../shared/types.h"
#include "../config.h"

// Connection status flag (set when first message is received)
static volatile bool espnow_connected = false;

// MAC address of the last known sender (filled on first received packet)
static uint8_t sender_mac[6] = {0};
static volatile bool sender_mac_known = false;
static bool sender_peer_added = false;

// Mutex for protecting critical sections when accessing sender_mac
static portMUX_TYPE sender_mac_mutex = portMUX_INITIALIZER_UNLOCKED;

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

    // Cache the sender MAC on first valid packet so we can reply later
    // Note: We write sender_mac_known AFTER memcpy to ensure atomicity
    // The flag acts as a guard - once true, sender_mac is stable
    if (!sender_mac_known && mac_addr != nullptr) {
        // Copy MAC address atomically (6 bytes, but we're in ISR so no task can interrupt)
        memcpy(sender_mac, mac_addr, 6);
        // Memory barrier to ensure write completes before flag is set
        __sync_synchronize();
        sender_mac_known = true;
        // Sender MAC learned (one-time event)
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

// ESP-NOW WiFi channel - MUST match on both sender and vehicle
#define ESPNOW_WIFI_CHANNEL 1

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
    
    // Set WiFi to station mode (matching legacy Vehicle_Controller.ino)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();  // Disconnect from any previous connection
    
    // CRITICAL for battery operation: Disable WiFi sleep mode
    // WiFi sleep can cause connection issues and packet loss
    WiFi.setSleep(false);
    Serial.println("[ESP-NOW] WiFi sleep mode disabled");
    
    // Extended delay for WiFi initialization on battery power
    // Battery-powered ESP32 needs more time for power stabilization
    delay(500);  // Increased from 200ms for better battery compatibility
    
    // Verify WiFi is ready before proceeding
    uint8_t mac[6];
    esp_err_t mac_result = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (mac_result != ESP_OK) {
        Serial.println("[WARNING] WiFi may not be fully initialized");
        // Continue anyway - ESP-NOW might still work
    }
    
    // Force WiFi channel - CRITICAL for ESP-NOW reliability
    // Both sender and vehicle MUST be on the same channel
    esp_err_t channel_result = esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (channel_result != ESP_OK) {
        Serial.print("[ERROR] Failed to set WiFi channel: ");
        Serial.println(channel_result);
        return false;
    }
    Serial.print("[ESP-NOW] WiFi channel set to: ");
    Serial.println(ESPNOW_WIFI_CHANNEL);
    
    // Print MAC address using esp_wifi_get_mac (matching legacy Vehicle_Controller.ino)
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print("[ESP-NOW] Vehicle MAC: ");
    Serial.println(macStr);
    
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
    
    Serial.println("[ESP-NOW] ✅ Initialized successfully");
    return true;
}

bool espnow_send_bytes(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return false;
    }

    // Check if sender MAC is known (read volatile flag)
    if (!sender_mac_known) {
        return false;
    }

    // Copy MAC address to local variable in critical section to prevent race condition
    // This ensures we read a consistent snapshot even if ISR updates it
    uint8_t local_mac[6];
    taskENTER_CRITICAL(&sender_mac_mutex);
    memcpy(local_mac, sender_mac, 6);
    taskEXIT_CRITICAL(&sender_mac_mutex);

    // Lazily add sender as a peer the first time we try to send back
    if (!sender_peer_added) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, local_mac, 6);
        peerInfo.channel = ESPNOW_WIFI_CHANNEL;  // Use fixed channel for reliability
        peerInfo.encrypt = false;  // No encryption for now

        esp_err_t peer_err = esp_now_add_peer(&peerInfo);
        if (peer_err != ESP_OK && peer_err != ESP_ERR_ESPNOW_EXIST) {
            return false;
        }
        sender_peer_added = true;
    }

    esp_err_t res = esp_now_send(local_mac, data, len);
    if (res != ESP_OK) {
        Serial.print("[ESP-NOW] esp_now_send failed, error: ");
        Serial.println(res);
        return false;
    }
    
    return true;
}

bool espnow_send_handshake_ack(uint8_t status_byte) {
    uint8_t frame[2];
    frame[0] = CMD_HANDSHAKE_ACK;
    frame[1] = status_byte;
    return espnow_send_bytes(frame, sizeof(frame));
}

bool espnow_send_mode_status(uint8_t mode) {
    uint8_t frame[2];
    frame[0] = CMD_MODE_STATUS;
    frame[1] = mode;  // 0 = MODE_MANUAL, 1 = MODE_AUTONOMOUS
    return espnow_send_bytes(frame, sizeof(frame));
}

bool espnow_is_connected() {
    return espnow_connected;
}

bool espnow_wait_for_connection(uint32_t timeout_ms) {
    // Reset connection flag
    espnow_connected = false;
    
    uint32_t start_time = millis();
    
    while (millis() - start_time < timeout_ms) {
        if (espnow_connected) {
            return true;
        }
        delay(100);  // Poll every 100ms
    }
    return false;
}

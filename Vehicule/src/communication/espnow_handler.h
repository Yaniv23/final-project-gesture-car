#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <esp_now.h>
#include <stdint.h>
#include <stdbool.h>
#include "command_protocol.h"

/**
 * @file espnow_handler.h
 * @brief ESP-NOW communication handler for binary command protocol
 * @details Receives single-byte commands via ESP-NOW and forwards to FreeRTOS queue
 */

/**
 * @brief Initialize ESP-NOW communication
 * @return true if successful, false otherwise
 */
bool espnow_init();

/**
 * @brief Get MAC address as string (for debugging)
 * @param mac_str Output buffer (must be at least 18 bytes)
 * @return true if successful
 */
bool espnow_get_mac_string(char* mac_str, size_t len);

/**
 * @brief Temperature message structure for ESP-NOW
 */
struct temp_message {
    float temperature;  // Temperature in Celsius
    uint8_t state;      // 0=normal, 1=warning, 2=critical
};

/**
 * @brief Send temperature data via ESP-NOW to sender
 * @param temperature Temperature value in Celsius
 * @param state Temperature state (0=normal, 1=warning, 2=critical)
 * @return true if sent successfully, false otherwise
 */
bool espnow_send_temperature(float temperature, uint8_t state);

#endif // ESPNOW_HANDLER_H

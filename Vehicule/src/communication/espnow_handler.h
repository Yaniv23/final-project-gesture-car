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

#endif // ESPNOW_HANDLER_H

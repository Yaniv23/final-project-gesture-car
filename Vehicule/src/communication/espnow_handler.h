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
 * @param simulation_mode If true, skip ESP-NOW init (for testing without receiver)
 * @return true if successful, false otherwise
 */
bool espnow_init(bool simulation_mode);

/**
 * @brief Send raw bytes to the last known sender over ESP-NOW
 *
 * @param data Pointer to data buffer
 * @param len  Number of bytes to send
 * @return true if the frame was queued for transmission, false otherwise
 *
 * @note This helper only works after at least one packet has been received
 *       from the sender (so that its MAC address is known). If no sender
 *       has been seen yet, this function returns false.
 */
bool espnow_send_bytes(const uint8_t* data, size_t len);

/**
 * @brief Convenience helper to send a handshake ACK frame
 *
 * Frame format: { CMD_HANDSHAKE_ACK, status_byte }
 *
 * @param status_byte Optional status / protocol version byte to echo back
 * @return true if the frame was queued for transmission, false otherwise
 */
bool espnow_send_handshake_ack(uint8_t status_byte);

/**
 * @brief Send current mode status to sender
 *
 * Frame format: { CMD_MODE_STATUS, mode_byte }
 * mode_byte: 0 = MODE_MANUAL, 1 = MODE_AUTONOMOUS
 *
 * @param mode Current driving mode (0 or 1)
 * @return true if the frame was queued for transmission, false otherwise
 */
bool espnow_send_mode_status(uint8_t mode);

/**
 * @brief Check if ESP-NOW connection is established
 * @return true if connected (first message received), false otherwise
 */
bool espnow_is_connected();

/**
 * @brief Wait for ESP-NOW connection to be established
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return true if connected, false if timeout expired
 * @note In simulation mode, returns true immediately
 */
bool espnow_wait_for_connection(uint32_t timeout_ms);

#endif // ESPNOW_HANDLER_H

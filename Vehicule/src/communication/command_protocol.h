#ifndef COMMAND_PROTOCOL_H
#define COMMAND_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file command_protocol.h
 * @brief Binary command protocol for ESP-NOW communication
 * @details Uses single-byte commands instead of strings for efficiency
 */

// Simple 1-byte command protocol
// Each command is a single byte (0-255)
// This is easier to understand and implement than string parsing

enum CommandByte {
    CMD_STOP = 0x00,
    CMD_FORWARD = 0x01,
    CMD_BACKWARD = 0x02,
    CMD_SIDEWAY_LEFT = 0x03,
    CMD_SIDEWAY_RIGHT = 0x04,
    CMD_ROTATE_CW = 0x05,
    CMD_ROTATE_CCW = 0x06,
    CMD_DIAGONAL_315 = 0x07,  
    CMD_DIAGONAL_45 = 0x08,   
    CMD_DIAGONAL_225 = 0x09, 
    CMD_DIAGONAL_135 = 0x0A,  
    CMD_PIVOT_LEFT = 0x0B,
    CMD_PIVOT_RIGHT = 0x0C,
    // Reserved: 0x0D - 0xFF for future commands
    CMD_INVALID = 0xFF  // Invalid command marker
};

/**
 * @brief Special high-value control bytes used for ESP-NOW link management
 *
 * These are NOT motion commands and must never be forwarded to the motor
 * control task. They are handled at the communication/transport layer.
 *
 * Range is chosen above normal motion commands (0x00–0x0C) so they are
 * trivially distinguishable and always fail isValidCommand().
 */
static const uint8_t CMD_HANDSHAKE_INIT  = 0xF0;  ///< Sender → vehicle: request handshake
static const uint8_t CMD_HANDSHAKE_ACK   = 0xF1;  ///< Vehicle → sender: acknowledge handshake
static const uint8_t CMD_HEARTBEAT       = 0xF2;  ///< (Optional) future heartbeat/status frames

/**
 * @brief Validate if a command byte is valid
 * @param cmd_byte Command byte to validate
 * @return true if valid, false otherwise
 */
bool isValidCommand(uint8_t cmd_byte);


// End of command_protocol.h

#endif // COMMAND_PROTOCOL_H

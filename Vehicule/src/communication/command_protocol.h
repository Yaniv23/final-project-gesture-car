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
    CMD_STRAFE_LEFT = 0x03,
    CMD_STRAFE_RIGHT = 0x04,
    CMD_ROTATE_CW = 0x05,
    CMD_ROTATE_CCW = 0x06,
    CMD_DIAGONAL_FORWARD_LEFT = 0x07,
    CMD_DIAGONAL_FORWARD_RIGHT = 0x08,
    CMD_DIAGONAL_BACKWARD_LEFT = 0x09,
    CMD_DIAGONAL_BACKWARD_RIGHT = 0x0A,
    CMD_PIVOT_LEFT = 0x0B,
    CMD_PIVOT_RIGHT = 0x0C,
    // Reserved: 0x0D - 0xFF for future commands
    CMD_INVALID = 0xFF  // Invalid command marker
};

/**
 * @brief Validate if a command byte is valid
 * @param cmd_byte Command byte to validate
 * @return true if valid, false otherwise
 */
bool isValidCommand(uint8_t cmd_byte);




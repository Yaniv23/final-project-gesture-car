/**
 * @file command_protocol.cpp
 * @brief Implementation of binary command protocol
 */

#include "command_protocol.h"

bool isValidCommand(uint8_t cmd_byte) {
    // Valid commands are 0x00 to 0x0C
    return (cmd_byte <= CMD_PIVOT_RIGHT);
}



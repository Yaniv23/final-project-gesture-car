#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

enum MotorID {
    MOTOR_FRONT_LEFT = 0,
    MOTOR_FRONT_RIGHT = 1,
    MOTOR_BACK_LEFT = 2,
    MOTOR_BACK_RIGHT = 3
};

struct ESPNowRawMessage {
    uint8_t first_byte;
    uint8_t second_byte;
    uint8_t length;
};

#endif

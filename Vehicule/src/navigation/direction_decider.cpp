/**
 * @file direction_decider.cpp
 * @brief Implementation of DirectionDecider
 */

#include "direction_decider.h"
#include "../config.h"

Direction DirectionDecider::decide(const ScanResult& scan_result) {
    if (!scan_result.is_valid) {
        return DIR_U_TURN; // Invalid scan - safest option
    }
    
    return chooseBestDirection(scan_result.distance_left, 
                                scan_result.distance_front, 
                                scan_result.distance_right);
}

Direction DirectionDecider::chooseBestDirection(float dist_left, float dist_front, float dist_right) {
    // Priority: Gauche > Droite > Avant > Demi-tour
    
    if (dist_left > MIN_FREE_SPACE_CM && dist_left > dist_right) {
        return DIR_LEFT;
    }
    
    if (dist_right > MIN_FREE_SPACE_CM && dist_right >= dist_left) {
        return DIR_RIGHT;
    }
    
    if (dist_front > MIN_FREE_SPACE_CM) {
        return DIR_FORWARD;
    }
    
    // All blocked - U-turn
    return DIR_U_TURN;
}

uint8_t DirectionDecider::directionToCommand(Direction direction) {
    switch (direction) {
        case DIR_LEFT:
            return CMD_ROTATE_CCW;
        case DIR_RIGHT:
            return CMD_ROTATE_CW;
        case DIR_FORWARD:
            return CMD_FORWARD;
        case DIR_U_TURN:
            // U-turn will be handled specially in state machine
            return CMD_BACKWARD;
        default:
            return CMD_STOP;
    }
}

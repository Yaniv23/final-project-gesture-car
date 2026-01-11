#ifndef DIRECTION_DECIDER_H
#define DIRECTION_DECIDER_H

#include "../obstacle_detection/obstacle_scanner.h"
#include "../communication/command_protocol.h"

/**
 * @file direction_decider.h
 * @brief Decides best direction based on scan results
 * @details Priority: Left > Right > Forward > U-Turn
 */

enum Direction {
    DIR_LEFT = 0,
    DIR_RIGHT = 1,
    DIR_FORWARD = 2,
    DIR_U_TURN = 3
};

class DirectionDecider {
public:
    /**
     * @brief Decide best direction based on scan result
     * @param scan_result Scan result from ObstacleScanner
     * @return Direction chosen (LEFT, RIGHT, FORWARD, U_TURN)
     */
    Direction decide(const ScanResult& scan_result);
    
    /**
     * @brief Convert direction to command byte
     * @param direction Direction enum
     * @return Command byte for motor control
     */
    static uint8_t directionToCommand(Direction direction);

private:
    // Priority: Gauche > Droite > Avant > Demi-tour
    Direction chooseBestDirection(float dist_left, float dist_front, float dist_right);
};

#endif // DIRECTION_DECIDER_H

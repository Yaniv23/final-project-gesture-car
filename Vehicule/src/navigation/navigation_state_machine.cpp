/**
 * @file navigation_state_machine.cpp
 * @brief Implementation of NavigationStateMachine
 */

#include "navigation_state_machine.h"
#include "../config.h"
#include "../communication/command_protocol.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

NavigationStateMachine::NavigationStateMachine(ObstacleScanner& scanner, 
                                               StuckDetector& stuck_detector,
                                               RecoveryStrategies& recovery,
                                               PositionTracker& position_tracker)
    : scanner_(scanner), stuck_detector_(stuck_detector), recovery_(recovery),
      position_tracker_(position_tracker), current_state_(STATE_FORWARD),
      state_start_time_ms_(0), action_direction_(-1), action_initiated_(false),
      recovery_initiated_(false), recovery_start_ms_(0), recovery_step_(0),
      current_strategy_(RECOVERY_BACKUP_TURN), recovery_attempt_count_(0),
      last_sent_command_(CMD_STOP), last_forward_command_time_(0),
      last_servo_update_ms_(0), adaptive_turn_initiated_(false),
      adaptive_turn_start_ms_(0), last_check_ms_(0), adaptive_turn_cmd_(CMD_STOP),
      backup_initiated_(false), backup_start_ms_(0) {
}

void NavigationStateMachine::reset() {
    current_state_ = STATE_FORWARD;
    state_start_time_ms_ = millis();
    action_direction_ = -1;
    action_initiated_ = false;
    recovery_initiated_ = false;
    recovery_step_ = 0;
    recovery_attempt_count_ = 0;
    last_sent_command_ = CMD_STOP;
    last_forward_command_time_ = 0;
    last_servo_update_ms_ = millis();
    adaptive_turn_initiated_ = false;
    backup_initiated_ = false;
    
    scanner_.reset();
    stuck_detector_.reset();
    recovery_.reset();
    position_tracker_.reset();
}

uint8_t NavigationStateMachine::update() {
    switch (current_state_) {
        case STATE_FORWARD:
            return handleStateForward();
        case STATE_SCAN:
            return handleStateScan();
        case STATE_DECISION:
            return handleStateDecision();
        case STATE_ACTION:
            return handleStateAction();
        case STATE_BACKING_UP:
            return handleStateBackingUp();
        case STATE_STUCK_PIVOTING:
            return handleStateStuckPivoting();
        case STATE_STOPPED:
            return handleStateStopped();
        default:
            return CMD_STOP;
    }
}

uint8_t NavigationStateMachine::handleStateForward() {
    unsigned long now = millis();
    uint8_t command = CMD_STOP;
    
    // Ensure servo is at center position
    scanner_.setServoToCenter();
    
    // Measure distance only if servo has stabilized
    if (now - last_servo_update_ms_ >= SERVO_STABILIZATION_MS) {
        float distance = scanner_.getFilteredDistance();
        
        // Update position history for movement detection
        if (distance >= 0.0f && distance <= 400.0f) {
            position_tracker_.update(distance);
        }
        
        // Check for obstacle ahead
        if (distance >= 0.0f && distance < CRITICAL_DISTANCE_CM) {
            // Obstacle detected - stop and trigger scan
            command = CMD_STOP;
            current_state_ = STATE_SCAN;
            Serial.print("[AUTO] Obstacle detected at ");
            Serial.print(distance);
            Serial.println(" cm - triggering scan");
            last_sent_command_ = CMD_STOP;
            return command;
        }
        
        last_servo_update_ms_ = now;
    }
    
    // Update stuck detector with current state
    bool is_moving = position_tracker_.isMoving();
    float current_distance = scanner_.getFilteredDistance();
    stuck_detector_.update(current_distance, is_moving, last_sent_command_);
    
    // Check real movement if FORWARD command active for X ms
    if (last_sent_command_ == CMD_FORWARD) {
        if (now - last_forward_command_time_ > STUCK_MOVEMENT_CHECK_MS) {
            if (!is_moving) {
                Serial.print("[AUTO] No movement detected - attempts: ");
                Serial.println(stuck_detector_.getFailedAttempts());
                
                if (stuck_detector_.getFailedAttempts() >= STUCK_THRESHOLD_ATTEMPTS) {
                    Serial.println("[AUTO] Stuck detected: no movement despite FORWARD command");
                    current_state_ = STATE_STUCK_PIVOTING;
                    command = CMD_STOP;
                    last_sent_command_ = CMD_STOP;
                    return command;
                }
            }
        }
    }
    
    // Check if stuck using advanced detection
    if (stuck_detector_.isStuck(&last_scan_)) {
        // Vehicle is stuck - enter stuck pivoting state
        current_state_ = STATE_STUCK_PIVOTING;
        command = CMD_STOP;
        last_sent_command_ = CMD_STOP;
        Serial.println("[AUTO] Stuck detected - entering pivot mode");
        return command;
    }
    
    // Path is clear - continue forward
    command = CMD_FORWARD;
    if (command == CMD_FORWARD) {
        last_sent_command_ = CMD_FORWARD;
        last_forward_command_time_ = now;
    } else {
        last_sent_command_ = command;
    }
    
    return command;
}

uint8_t NavigationStateMachine::handleStateScan() {
    uint8_t command = CMD_STOP;
    
    // Perform non-blocking scan
    bool scan_complete = scanner_.scan3Directions(last_scan_);
    
    if (scan_complete) {
        if (last_scan_.is_valid) {
            Serial.print("[AUTO] Scan complete - L:");
            Serial.print(last_scan_.distance_left);
            Serial.print(" F:");
            Serial.print(last_scan_.distance_front);
            Serial.print(" R:");
            Serial.println(last_scan_.distance_right);
            current_state_ = STATE_DECISION;
        } else {
            // Invalid scan - retry
            Serial.println("[AUTO] Invalid scan - retrying");
            scanner_.reset();
        }
    }
    
    return command;
}

uint8_t NavigationStateMachine::handleStateDecision() {
    uint8_t command = CMD_STOP;
    
    if (last_scan_.is_valid) {
        Direction direction = direction_decider_.decide(last_scan_);
        action_direction_ = static_cast<int>(direction);
        
        Serial.print("[AUTO] Decision: ");
        switch (direction) {
            case DIR_LEFT:
                Serial.println("TURN LEFT");
                break;
            case DIR_RIGHT:
                Serial.println("TURN RIGHT");
                break;
            case DIR_FORWARD:
                Serial.println("CONTINUE FORWARD");
                break;
            case DIR_U_TURN:
                Serial.println("U-TURN");
                break;
        }
        
        // Transition to ACTION state
        current_state_ = STATE_ACTION;
        action_initiated_ = false;
    } else {
        // Invalid scan - retry
        Serial.println("[AUTO] Invalid scan - retrying");
        current_state_ = STATE_SCAN;
    }
    
    return command;
}

uint8_t NavigationStateMachine::handleStateAction() {
    unsigned long now = millis();
    uint8_t command = CMD_STOP;
    
    if (!action_initiated_) {
        // Get direction from DECISION state
        if (action_direction_ == -1) {
            // Fallback - go to forward state
            current_state_ = STATE_FORWARD;
            action_initiated_ = false;
            return command;
        }
        
        action_initiated_ = true;
        state_start_time_ms_ = now;
        
        // Execute movement
        if (action_direction_ == DIR_U_TURN) {
            // U-turn: backup first
            command = CMD_BACKWARD;
            Serial.println("[AUTO] Executing U-turn: backing up first");
        } else {
            Direction dir = static_cast<Direction>(action_direction_);
            command = DirectionDecider::directionToCommand(dir);
            Serial.print("[AUTO] Executing movement: ");
            Serial.println(action_direction_);
        }
    } else {
        if (action_direction_ == DIR_U_TURN) {
            // U-turn: backup then turn
            if (now - state_start_time_ms_ < STUCK_BACKUP_DURATION_MS / 2) {
                command = CMD_BACKWARD;
            } else if (now - state_start_time_ms_ < STUCK_BACKUP_DURATION_MS / 2 + TURN_DURATION_MS) {
                command = CMD_ROTATE_CW; // Turn right for U-turn
            } else {
                // U-turn complete
                command = CMD_STOP;
                action_initiated_ = false;
                action_direction_ = -1;
                current_state_ = STATE_FORWARD;
                Serial.println("[AUTO] U-turn complete");
            }
        } else if (action_direction_ == DIR_LEFT || action_direction_ == DIR_RIGHT) {
            // Turning left or right - use adaptive turning
            if (!adaptive_turn_initiated_) {
                // Start adaptive turn
                adaptive_turn_cmd_ = (action_direction_ == DIR_LEFT) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
                command = adaptive_turn_cmd_;
                adaptive_turn_initiated_ = true;
                adaptive_turn_start_ms_ = now;
                last_check_ms_ = now;
                Serial.println("[AUTO] Starting adaptive turn");
            } else {
                // Check periodically if path clear during rotation
                if (now - last_check_ms_ >= ADAPTIVE_TURN_CHECK_INTERVAL_MS) {
                    last_check_ms_ = now;
                    
                    // Quick measurement (3 samples instead of 5 to reduce latency)
                    float distance = scanner_.getFilteredDistance();
                    
                    // If clear path found, stop rotation immediately
                    if (distance >= MIN_FREE_SPACE_CM && distance > 0) {
                        command = CMD_STOP;
                        adaptive_turn_initiated_ = false;
                        action_initiated_ = false;
                        action_direction_ = -1;
                        current_state_ = STATE_FORWARD;
                        Serial.print("[AUTO] Adaptive turn: path clear at ");
                        Serial.print(distance);
                        Serial.println(" cm");
                        return command;
                    }
                }
                
                // Check timeout
                if (now - adaptive_turn_start_ms_ >= ADAPTIVE_TURN_MAX_DURATION_MS) {
                    // Timeout - stop rotation (safety)
                    command = CMD_STOP;
                    adaptive_turn_initiated_ = false;
                    action_initiated_ = false;
                    action_direction_ = -1;
                    current_state_ = STATE_FORWARD;
                    Serial.println("[AUTO] Adaptive turn timeout - continuing forward");
                } else {
                    // Continue rotation
                    command = adaptive_turn_cmd_;
                }
            }
        } else {
            // Forward - just continue briefly then return to FORWARD
            if (now - state_start_time_ms_ < 200) {
                command = CMD_FORWARD;
            } else {
                command = CMD_STOP;
                action_initiated_ = false;
                action_direction_ = -1;
                current_state_ = STATE_FORWARD;
            }
        }
    }
    
    return command;
}

uint8_t NavigationStateMachine::handleStateBackingUp() {
    uint8_t command = CMD_STOP;
    
    if (!backup_initiated_) {
        // Start backup
        backup_start_ms_ = millis();
        backup_initiated_ = true;
        Serial.println("[AUTO] Starting safe backup");
    }
    
    // Check if backup duration completed
    if (millis() - backup_start_ms_ >= BACKUP_TIME_MS) {
        // Backup completed - continue forward
        command = CMD_STOP;
        current_state_ = STATE_FORWARD;
        backup_initiated_ = false;
        Serial.println("[AUTO] Backup completed - resuming forward");
    } else {
        // Continue backing up
        command = CMD_BACKWARD;
    }
    
    return command;
}

uint8_t NavigationStateMachine::handleStateStuckPivoting() {
    unsigned long now = millis();
    uint8_t command = CMD_STOP;
    
    if (!recovery_initiated_) {
        // Choose and start recovery strategy
        current_strategy_ = recovery_.chooseStrategy(recovery_attempt_count_);
        recovery_initiated_ = true;
        recovery_start_ms_ = now;
        recovery_step_ = 0;
        recovery_attempt_count_++;
        
        Serial.print("[AUTO] Recovery attempt #");
        Serial.println(recovery_attempt_count_);
    }
    
    // Execute strategy (non-blocking)
    bool strategy_complete = recovery_.execute(current_strategy_, command, 
                                               recovery_start_ms_, recovery_step_,
                                               &scanner_);
    
    if (strategy_complete) {
        // Strategy complete - scan to find path
        current_state_ = STATE_SCAN;
        recovery_initiated_ = false;
        recovery_step_ = 0;
        Serial.println("[AUTO] Recovery complete - scanning for clear path");
        return command;
    }
    
    // If too many attempts, try long backup and reset
    if (recovery_attempt_count_ >= RECOVERY_MAX_ATTEMPTS) {
        Serial.println("[AUTO] Max recovery attempts reached - trying long backup");
        current_strategy_ = RECOVERY_BACKUP_LONG;
        recovery_step_ = 0;
        recovery_start_ms_ = now;
        recovery_attempt_count_ = 0; // Reset for next time
    }
    
    return command;
}

uint8_t NavigationStateMachine::handleStateStopped() {
    uint8_t command = CMD_STOP;
    
    // Stopped due to error - wait and try to recover
    static uint32_t stop_time_ms = 0;
    stop_time_ms += AUTONOMOUS_TASK_PERIOD_MS;
    if (stop_time_ms >= 1000) {
        // Try to recover after 1 second
        current_state_ = STATE_FORWARD;
        stop_time_ms = 0;
        reset();
        Serial.println("[AUTO] Attempting recovery");
    }
    
    return command;
}

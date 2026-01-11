#ifndef NAVIGATION_STATE_MACHINE_H
#define NAVIGATION_STATE_MACHINE_H

#include <stdint.h>
#include "../drivers/ultrasonic_driver.h"
#include "../drivers/servo_driver.h"
#include "../obstacle_detection/obstacle_scanner.h"
#include "../obstacle_detection/stuck_detector.h"
#include "../obstacle_detection/position_tracker.h"
#include "../recovery/recovery_strategies.h"
#include "../navigation/direction_decider.h"

/**
 * @file navigation_state_machine.h
 * @brief Navigation state machine for autonomous obstacle avoidance
 * @details Orchestrates all navigation modules in a state machine pattern
 */

enum NavigationState {
    STATE_FORWARD,          // Avance avec mesure devant uniquement
    STATE_SCAN,             // Scan 3 directions déclenché
    STATE_DECISION,         // Décision basée sur scan
    STATE_ACTION,           // Exécution mouvement choisi
    STATE_BACKING_UP,        // Recul sécurisé
    STATE_STUCK_PIVOTING,   // Pivot sur place pour trouver chemin
    STATE_STOPPED           // Stopped (safety)
};

class NavigationStateMachine {
public:
    NavigationStateMachine(ObstacleScanner& scanner, 
                          StuckDetector& stuck_detector,
                          RecoveryStrategies& recovery,
                          PositionTracker& position_tracker);
    
    /**
     * @brief Update state machine (call every task period)
     * @return Command to send to motor control queue
     */
    uint8_t update();
    
    /**
     * @brief Reset state machine to initial state
     */
    void reset();
    
    /**
     * @brief Get current state
     */
    NavigationState getState() const { return current_state_; }
    
    /**
     * @brief Check if state machine is active
     */
    bool isActive() const { return current_state_ != STATE_STOPPED; }

private:
    NavigationState current_state_;
    ObstacleScanner& scanner_;
    StuckDetector& stuck_detector_;
    RecoveryStrategies& recovery_;
    PositionTracker& position_tracker_;
    DirectionDecider direction_decider_;
    
    // State-specific data
    unsigned long state_start_time_ms_;
    int action_direction_;
    bool action_initiated_;
    
    // Scan result storage
    ScanResult last_scan_;
    
    // Recovery state
    bool recovery_initiated_;
    unsigned long recovery_start_ms_;
    int recovery_step_;
    RecoveryStrategy current_strategy_;
    int recovery_attempt_count_;
    
    // Forward state tracking
    uint8_t last_sent_command_;
    unsigned long last_forward_command_time_;
    unsigned long last_servo_update_ms_;
    
    // Adaptive turn state
    bool adaptive_turn_initiated_;
    unsigned long adaptive_turn_start_ms_;
    unsigned long last_check_ms_;
    uint8_t adaptive_turn_cmd_;
    
    // Backup state
    bool backup_initiated_;
    unsigned long backup_start_ms_;
    
    // State handlers
    uint8_t handleStateForward();
    uint8_t handleStateScan();
    uint8_t handleStateDecision();
    uint8_t handleStateAction();
    uint8_t handleStateBackingUp();
    uint8_t handleStateStuckPivoting();
    uint8_t handleStateStopped();
};

#endif // NAVIGATION_STATE_MACHINE_H

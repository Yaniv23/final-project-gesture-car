#ifndef RECOVERY_STRATEGIES_H
#define RECOVERY_STRATEGIES_H

#include <stdint.h>

/**
 * @file recovery_strategies.h
 * @brief Recovery strategies for stuck vehicle situations
 * @details Implements 5 different recovery strategies to avoid loops
 */

enum RecoveryStrategy {
    RECOVERY_BACKUP_TURN,      // Reculer puis tourner aléatoirement
    RECOVERY_PIVOT_360,        // Rotation complète 360°
    RECOVERY_BACKUP_LONG,      // Recul long
    RECOVERY_RANDOM_TURN,      // Tourner aléatoirement avec angle variable
    RECOVERY_WALL_FOLLOW       // Essayer suivi de mur (simplifié: scan seulement)
};

class RecoveryStrategies {
public:
    RecoveryStrategies();
    
    /**
     * @brief Choose recovery strategy based on attempt number
     * @param attempt_number Attempt number (for variation)
     * @return Strategy chosen
     */
    RecoveryStrategy chooseStrategy(int attempt_number);
    
    /**
     * @brief Execute recovery strategy (non-blocking, stateful)
     * @param strategy Strategy to execute
     * @param command Output command to send
     * @param recovery_start_ms Reference to recovery start time (modified)
     * @param recovery_step Reference to recovery step (modified)
     * @param scanner Optional scanner for WALL_FOLLOW strategy
     * @return true if strategy complete, false if still in progress
     */
    bool execute(RecoveryStrategy strategy, uint8_t& command,
                 unsigned long& recovery_start_ms, int& recovery_step,
                 class ObstacleScanner* scanner = nullptr);
    
    /**
     * @brief Reset recovery memory
     */
    void reset();
    
    /**
     * @brief Get attempt count
     */
    int getAttemptCount() const { return attempt_count_; }

private:
    int attempt_count_;
    RecoveryStrategy last_strategy_;
    unsigned long last_recovery_time_;
    
    // Strategy-specific execution
    bool executeBackupTurn(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step);
    bool executePivot360(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step);
    bool executeBackupLong(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step);
    bool executeRandomTurn(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step);
    bool executeWallFollow(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step, class ObstacleScanner* scanner);
};

#endif // RECOVERY_STRATEGIES_H

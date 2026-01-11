/**
 * @file recovery_strategies.cpp
 * @brief Implementation of RecoveryStrategies
 */

#include "recovery_strategies.h"
#include "../config.h"
#include "../communication/command_protocol.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

RecoveryStrategies::RecoveryStrategies()
    : attempt_count_(0), last_strategy_(RECOVERY_BACKUP_TURN), last_recovery_time_(0) {
}

void RecoveryStrategies::reset() {
    attempt_count_ = 0;
    last_strategy_ = RECOVERY_BACKUP_TURN;
    last_recovery_time_ = 0;
}

RecoveryStrategy RecoveryStrategies::chooseStrategy(int attempt_number) {
    RecoveryStrategy strategies[] = {
        RECOVERY_BACKUP_TURN,
        RECOVERY_PIVOT_360,
        RECOVERY_BACKUP_LONG,
        RECOVERY_RANDOM_TURN,
        RECOVERY_WALL_FOLLOW
    };
    
    // Vary strategy according to attempt number (modulo to avoid repetition)
    int index = (attempt_number % 5);
    return strategies[index];
}

bool RecoveryStrategies::execute(RecoveryStrategy strategy, uint8_t& command,
                                  unsigned long& recovery_start_ms, int& recovery_step,
                                  ObstacleScanner* scanner) {
    unsigned long now = millis();
    
    switch (strategy) {
        case RECOVERY_BACKUP_TURN:
            return executeBackupTurn(command, recovery_start_ms, recovery_step);
        case RECOVERY_PIVOT_360:
            return executePivot360(command, recovery_start_ms, recovery_step);
        case RECOVERY_BACKUP_LONG:
            return executeBackupLong(command, recovery_start_ms, recovery_step);
        case RECOVERY_RANDOM_TURN:
            return executeRandomTurn(command, recovery_start_ms, recovery_step);
        case RECOVERY_WALL_FOLLOW:
            return executeWallFollow(command, recovery_start_ms, recovery_step, scanner);
        default:
            command = CMD_STOP;
            return true;
    }
}

bool RecoveryStrategies::executeBackupTurn(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step) {
    unsigned long now = millis();
    static uint8_t recovery_cmd = CMD_STOP;
    static unsigned long phase_start_ms = 0;
    
    if (recovery_step == 0) {
        Serial.println("[AUTO] Recovery: BACKUP_TURN");
        recovery_cmd = CMD_BACKWARD;
        command = recovery_cmd;
        recovery_step = 1;
        phase_start_ms = now;
        recovery_start_ms = now;
    } else if (recovery_step == 1) {
        // Backup phase
        if (now - phase_start_ms < BACKUP_TIME_MS) {
            command = recovery_cmd;
        } else {
            // Backup complete - start turn
            recovery_cmd = (random(0, 2) == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
            command = recovery_cmd;
            phase_start_ms = now; // Reset timer for turn
            recovery_step = 2;
        }
    } else if (recovery_step == 2) {
        // Turn phase
        if (now - phase_start_ms < TURN_DURATION_MS) {
            command = recovery_cmd;
        } else {
            // Turn complete
            command = CMD_STOP;
            last_strategy_ = RECOVERY_BACKUP_TURN;
            last_recovery_time_ = now;
            recovery_step = 0;
            return true;
        }
    }
    return false;
}

bool RecoveryStrategies::executePivot360(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step) {
    unsigned long now = millis();
    static uint8_t recovery_cmd = CMD_STOP;
    static unsigned long phase_start_ms = 0;
    
    if (recovery_step == 0) {
        Serial.println("[AUTO] Recovery: PIVOT_360");
        recovery_cmd = CMD_ROTATE_CW;
        command = recovery_cmd;
        recovery_step = 1;
        phase_start_ms = now;
        recovery_start_ms = now;
    } else if (recovery_step == 1) {
        // 360° rotation (4 × 90°)
        if (now - phase_start_ms < TURN_DURATION_MS * 4) {
            command = recovery_cmd;
        } else {
            command = CMD_STOP;
            last_strategy_ = RECOVERY_PIVOT_360;
            last_recovery_time_ = now;
            recovery_step = 0;
            return true;
        }
    }
    return false;
}

bool RecoveryStrategies::executeBackupLong(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step) {
    unsigned long now = millis();
    static uint8_t recovery_cmd = CMD_STOP;
    static unsigned long phase_start_ms = 0;
    
    if (recovery_step == 0) {
        Serial.println("[AUTO] Recovery: BACKUP_LONG");
        recovery_cmd = CMD_BACKWARD;
        command = recovery_cmd;
        recovery_step = 1;
        phase_start_ms = now;
        recovery_start_ms = now;
    } else if (recovery_step == 1) {
        // Long backup
        if (now - phase_start_ms < STUCK_BACKUP_DURATION_MS) {
            command = recovery_cmd;
        } else {
            command = CMD_STOP;
            last_strategy_ = RECOVERY_BACKUP_LONG;
            last_recovery_time_ = now;
            recovery_step = 0;
            return true;
        }
    }
    return false;
}

bool RecoveryStrategies::executeRandomTurn(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step) {
    unsigned long now = millis();
    static uint8_t recovery_cmd = CMD_STOP;
    static uint32_t random_turn_duration = 0;
    static unsigned long phase_start_ms = 0;
    
    if (recovery_step == 0) {
        Serial.println("[AUTO] Recovery: RANDOM_TURN");
        recovery_cmd = (random(0, 2) == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
        command = recovery_cmd;
        // Random angle between 90° and 270°
        random_turn_duration = TURN_DURATION_MS + random(0, TURN_DURATION_MS * 2);
        recovery_step = 1;
        phase_start_ms = now;
        recovery_start_ms = now;
    } else if (recovery_step == 1) {
        // Random turn
        if (now - phase_start_ms < random_turn_duration) {
            command = recovery_cmd;
        } else {
            command = CMD_STOP;
            last_strategy_ = RECOVERY_RANDOM_TURN;
            last_recovery_time_ = now;
            recovery_step = 0;
            return true;
        }
    }
    return false;
}

bool RecoveryStrategies::executeWallFollow(uint8_t& command, unsigned long& recovery_start_ms, int& recovery_step, ObstacleScanner* scanner) {
    unsigned long now = millis();
    
    if (recovery_step == 0) {
        Serial.println("[AUTO] Recovery: WALL_FOLLOW (scan only)");
        // Simplified: just scan to find wall
        // Can be extended later with full wall following
        // If scanner provided, we could trigger a scan here
        // For now, we'll mark as complete immediately (scan handled by state machine)
        last_strategy_ = RECOVERY_WALL_FOLLOW;
        last_recovery_time_ = now;
        recovery_step = 1;
        command = CMD_STOP;
        return true; // Scan is immediate (handled by state machine)
    }
    return false;
}

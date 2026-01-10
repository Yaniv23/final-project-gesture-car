/**
 * @file task_autonomous.cpp
 * @brief Autonomous navigation task - 50ms period, Priority 3
 * @details Enhanced obstacle avoidance using servo-mounted ultrasonic sensor
 *          Triggered scanning: servo fixed at 90° during forward movement
 *          When obstacle detected, performs 3-direction scan (45°, 90°, 135°)
 *          Chooses best direction based on scan results
 *          Includes improved stuck detection with pivot-on-place recovery
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../drivers/ultrasonic_driver.h"
#include "../drivers/servo_driver.h"
#include "../communication/command_protocol.h"
#include "../control/mode_manager.h"

// Sensor instances for autonomous mode
static Ultrasonic ultrasonic_sensor;
static ServoDriver servo;

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

// Scan result structure for triggered 3-direction scan
struct ScanResult {
    float distance_left;    // Distance à gauche (45°)
    float distance_front;   // Distance devant (90°)
    float distance_right;   // Distance à droite (135°)
    unsigned long timestamp_ms;
    bool is_valid;
};

static ScanResult last_scan;

// Stuck detection memory structure
struct StuckDetection {
    unsigned long last_forward_attempt_ms;  // Dernière tentative d'avancer
    unsigned long last_position_change_ms;  // Dernier changement de position
    float last_best_distance;               // Meilleure distance trouvée
    int consecutive_failed_attempts;        // Nombre d'échecs consécutifs
    bool is_stuck;                          // Flag de blocage
    int consecutive_stuck_count;           // Compteur de tentatives bloquées
};

static StuckDetection stuck_memory;

// Navigation state machine
enum NavigationState {
    STATE_FORWARD,          // Avance avec mesure devant uniquement (servo fixe 90°)
    STATE_SCAN,             // Scan 3 directions déclenché
    STATE_DECISION,         // Décision basée sur scan
    STATE_ACTION,           // Exécution mouvement choisi
    STATE_BACKING_UP,       // Recul sécurisé
    STATE_STUCK_PIVOTING,   // Pivot sur place pour trouver chemin
    STATE_STOPPED           // Stopped (safety)
};

/**
 * @brief Mesure distance filtrée (moyenne de plusieurs échantillons)
 * @param ultrasonic_sensor Référence au capteur
 * @param samples Nombre d'échantillons pour la moyenne
 * @return Distance filtrée en cm, ou -1.0 si invalide
 */
static float filteredDistance(Ultrasonic& ultrasonic_sensor, int samples) {
    float sum = 0.0f;
    int valid_count = 0;
    
    for (int i = 0; i < samples; i++) {
        float dist = ultrasonic_sensor.readDistanceCM();
        // Filtrer valeurs invalides (négatives ou > 400cm)
        if (dist >= 0.0f && dist <= 400.0f) {
            sum += dist;
            valid_count++;
        }
        if (i < samples - 1) {
            vTaskDelay(pdMS_TO_TICKS(FILTER_DELAY_MS));
        }
    }
    
    if (valid_count == 0) {
        return -1.0f;
    }
    
    return sum / valid_count;
}

/**
 * @brief Effectue un scan des 3 directions (gauche, avant, droite)
 * @param ultrasonic_sensor Référence au capteur ultrasonique
 * @param servo Référence au servo driver
 * @param result Structure ScanResult à remplir
 * @return true si scan réussi, false sinon
 */
static bool scan3Directions(Ultrasonic& ultrasonic_sensor, 
                             ServoDriver& servo, 
                             ScanResult& result) {
    // 1. Gauche (45°)
    servo.setAngle(SCAN_LEFT_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
    result.distance_left = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
    
    // 2. Avant (90°)
    servo.setAngle(SCAN_CENTER_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
    result.distance_front = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
    
    // 3. Droite (135°)
    servo.setAngle(SCAN_RIGHT_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
    result.distance_right = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
    
    // Recentrer servo
    servo.setAngle(SCAN_CENTER_ANGLE);
    
    result.timestamp_ms = millis();
    result.is_valid = (result.distance_left > 0 && 
                       result.distance_front > 0 && 
                       result.distance_right > 0);
    
    return result.is_valid;
}

/**
 * @brief Choisit la meilleure direction basée sur scan
 * @param scan Résultat du scan 3 directions
 * @return Direction choisie: 0=GAUCHE, 1=DROITE, 2=AVANT, 3=DEMI_TOUR
 */
static int decideDirection(const ScanResult& scan) {
    // Priorité: Gauche > Droite > Avant > Demi-tour
    
    if (scan.distance_left > MIN_FREE_SPACE_CM && 
        scan.distance_left > scan.distance_right) {
        return 0; // GAUCHE
    }
    
    if (scan.distance_right > MIN_FREE_SPACE_CM && 
        scan.distance_right >= scan.distance_left) {
        return 1; // DROITE
    }
    
    if (scan.distance_front > MIN_FREE_SPACE_CM) {
        return 2; // AVANT
    }
    
    // Tout bloqué - demi-tour
    return 3; // DEMI_TOUR
}

/**
 * @brief Exécute le mouvement choisi
 * @param direction Direction choisie (0-3: GAUCHE, DROITE, AVANT, DEMI_TOUR)
 * @param turn_duration_ms Durée de rotation en ms
 * @return Commande à envoyer
 */
static uint8_t executeMovement(int direction, uint32_t turn_duration_ms) {
    uint8_t command;
    
    switch (direction) {
        case 0: // GAUCHE
            command = CMD_ROTATE_CCW;
            break;
        case 1: // DROITE
            command = CMD_ROTATE_CW;
            break;
        case 2: // AVANT
            command = CMD_FORWARD;
            break;
        case 3: // DEMI_TOUR
            // Pour demi-tour, on recule d'abord puis on tourne
            // Cette logique sera gérée dans STATE_ACTION
            command = CMD_BACKWARD;
            break;
        default:
            command = CMD_STOP;
            break;
    }
    
    return command;
}


/**
 * @brief Reset stuck detection memory
 */
static void resetStuckMemory() {
    stuck_memory.last_forward_attempt_ms = 0;
    stuck_memory.last_position_change_ms = 0;
    stuck_memory.last_best_distance = 0.0f;
    stuck_memory.consecutive_failed_attempts = 0;
    stuck_memory.is_stuck = false;
    stuck_memory.consecutive_stuck_count = 0;
}

/**
 * @brief Update stuck detection memory
 * @param current_distance Current best distance found
 * @param is_moving Whether vehicle is currently moving forward
 */
static void updateStuckMemory(float current_distance, bool is_moving) {
    unsigned long now = millis();
    
    if (is_moving) {
        stuck_memory.last_position_change_ms = now;
        stuck_memory.consecutive_failed_attempts = 0;
    } else {
        // Not moving - check if we're stuck
        if (now - stuck_memory.last_position_change_ms >= STUCK_TIME_THRESHOLD_MS) {
            stuck_memory.consecutive_failed_attempts++;
            if (stuck_memory.consecutive_failed_attempts >= STUCK_THRESHOLD_ATTEMPTS) {
                stuck_memory.is_stuck = true;
            }
        }
    }
    
    stuck_memory.last_best_distance = current_distance;
}


/**
 * @brief Détecte si le robot est coincé (amélioration)
 * @param consecutive_stuck_count Compteur de tentatives échouées (référence)
 * @return true si bloqué, false sinon
 */
static bool checkIfStuckImproved(int& consecutive_stuck_count) {
    if (!STUCK_DETECTION_ENABLED) {
        return false;
    }
    
    // Si toutes les directions sont bloquées
    if (last_scan.is_valid) {
        if (last_scan.distance_left < MIN_FREE_SPACE_CM &&
            last_scan.distance_front < MIN_FREE_SPACE_CM &&
            last_scan.distance_right < MIN_FREE_SPACE_CM) {
            consecutive_stuck_count++;
            
            if (consecutive_stuck_count >= STUCK_BACKUP_COUNT) {
                return true;
            }
        } else {
            consecutive_stuck_count = 0; // Reset si chemin trouvé
        }
    }
    
    return false;
}
/**
 * @brief Safe backup function - recule de manière sécurisée
 * @details Inspirée de la fonction safeBackup() du code de référence Obstacle_Avoidance_Bot
 *          Vérifie d'abord s'il est sûr de reculer, puis recule pendant BACKUP_TIME_MS
 * @param ultrasonic_sensor Référence au capteur ultrasonique pour vérifier l'arrière
 * @return true si le recul a été effectué avec succès, false si obstacle détecté derrière
 */
static bool safeBackup(Ultrasonic& ultrasonic_sensor) {
    // Note: Dans le code de référence, il y a des capteurs IR arrière
    // Ici, on utilise le capteur ultrasonique monté sur servo
    // On vérifie d'abord si on peut reculer en mesurant la distance
    // Pour simplifier, on assume qu'il est sûr de reculer si pas d'obstacle très proche
    
    // Vérifier la distance actuelle (servo devrait être au centre)
    float rear_distance = ultrasonic_sensor.readDistanceCM();
    
    // Si distance invalide ou très proche (< 15cm), ne pas reculer
    if (rear_distance < 0.0f || rear_distance < 15.0f) {
        Serial.println("[AUTO] Cannot back up: obstacle too close behind");
        return false;
    }
    
    Serial.println("[AUTO] Backing up safely");
    
    // Envoyer commande de recul
    uint8_t backup_cmd = CMD_BACKWARD;
    if (xQueueSend(xCommandQueue, &backup_cmd, 0) != pdTRUE) {
        Serial.println("[AUTO] Warning: Command queue full during backup!");
        return false;
    }
    
    // Attendre pendant BACKUP_TIME_MS en vérifiant périodiquement
    unsigned long backup_start_ms = millis();
    while (millis() - backup_start_ms < BACKUP_TIME_MS) {
        // Vérifier la distance pendant le recul (toutes les 50ms)
        vTaskDelay(pdMS_TO_TICKS(50));
        float current_rear = ultrasonic_sensor.readDistanceCM();
        
        // Si obstacle détecté pendant le recul, arrêter immédiatement
        if (current_rear < 0.0f || current_rear < 15.0f) {
            Serial.println("[AUTO] Obstacle detected while backing - stopping");
            uint8_t stop_cmd = CMD_STOP;
            xQueueSend(xCommandQueue, &stop_cmd, 0);
            return false;
        }
    }
    
    // Arrêter après le recul
    uint8_t stop_cmd = CMD_STOP;
    xQueueSend(xCommandQueue, &stop_cmd, 0);
    
    return true;
}

void task_autonomous(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(AUTONOMOUS_TASK_PERIOD_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    // Get ModeManager instance
    ModeManager& mode_mgr = ModeManager::getInstance();
    
    // Lazy initialization: sensors will be initialized only when entering autonomous mode
    bool sensors_initialized = false;
    
    // Navigation state
    NavigationState nav_state = STATE_FORWARD;
    
    // Timing for servo stabilization before measurement
    unsigned long last_servo_update_ms = 0;
    
    // Turn duration counter
    uint32_t turn_duration_ms = 0;
    uint8_t last_command = CMD_STOP;
    
    // Initialize stuck memory
    resetStuckMemory();
    
    while (1) {
        // Check if we're in autonomous mode
        if (!mode_mgr.isAutonomousMode()) {
            // Not in autonomous mode, just wait
            // Reset sensor initialization flag when leaving autonomous mode
            if (sensors_initialized) {
                sensors_initialized = false;
                servo.stopSweep();
                Serial.println("[AUTO] Left autonomous mode - sensors deactivated");
            }
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Lazy initialization: initialize sensors only when entering autonomous mode
        if (!sensors_initialized) {
            bool sensor_ok = ultrasonic_sensor.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
            bool servo_ok = servo.init(SERVO_PIN);
            
            if (!sensor_ok || !servo_ok) {
                Serial.print("[AUTO] ERROR: Failed to initialize sensors! Sensor: ");
                Serial.print(sensor_ok ? "OK" : "FAIL");
                Serial.print(", Servo: ");
                Serial.println(servo_ok ? "OK" : "FAIL");
            } else {
                Serial.println("[AUTO] Entered autonomous mode - Sensors initialized");
                Serial.println("[AUTO] Starting triggered scan mode...");
                // Reset navigation state to forward
                nav_state = STATE_FORWARD;
                // Reset stuck memory
                resetStuckMemory();
                // Reset timing variables
                turn_duration_ms = 0;
                last_command = CMD_STOP;
                last_servo_update_ms = millis();
                // Set servo to center position (90°) for forward movement
                servo.setAngle(SCAN_CENTER_ANGLE);
                servo.stopSweep();
                Serial.println("[AUTO] Servo set to center (90°) - ready for triggered scanning");
            }
            sensors_initialized = true;
        }
        
        // Default command
        uint8_t command = CMD_STOP;
        
        // Navigation state machine
        switch (nav_state) {
            case STATE_FORWARD: {
                // Moving forward with servo fixed at 90° (center)
                // Measure distance only in front
                unsigned long now = millis();
                
                // Ensure servo is at center position
                servo.setAngle(SCAN_CENTER_ANGLE);
                servo.stopSweep();
                
                // Measure distance only if servo has stabilized
                if (now - last_servo_update_ms >= SERVO_STABILIZATION_MS) {
                    float distance = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
                    
                    // Check for obstacle ahead
                    if (distance >= 0.0f && distance < CRITICAL_DISTANCE_CM) {
                        // Obstacle detected - stop and trigger scan
                        command = CMD_STOP;
                        nav_state = STATE_SCAN;
                        Serial.print("[AUTO] Obstacle detected at ");
                        Serial.print(distance);
                        Serial.println(" cm - triggering scan");
                        break;
                    }
                    
                    last_servo_update_ms = now;
                }
                
                // Check if stuck using improved detection
                if (checkIfStuckImproved(stuck_memory.consecutive_stuck_count)) {
                    // Vehicle is stuck - enter stuck pivoting state
                    nav_state = STATE_STUCK_PIVOTING;
                    command = CMD_STOP;
                    Serial.println("[AUTO] Stuck detected - entering pivot mode");
                    break;
                }
                
                // Path is clear - continue forward
                command = CMD_FORWARD;
                break;
            }
            
            case STATE_SCAN: {
                // Stop motors and perform 3-direction scan
                command = CMD_STOP;
                
                static bool scan_initiated = false;
                static int scan_step = 0; // 0=init, 1=left, 2=center, 3=right, 4=done
                static unsigned long last_servo_move_ms = 0;
                unsigned long now = millis();
                
                if (!scan_initiated) {
                    scan_initiated = true;
                    scan_step = 1; // Start with left
                    last_servo_move_ms = 0;
                    Serial.println("[AUTO] Starting 3-direction scan...");
                }
                
                // Perform scan over multiple task cycles to avoid blocking
                switch (scan_step) {
                    case 1: // Left (45°)
                        if (last_servo_move_ms == 0) {
                            servo.setAngle(SCAN_LEFT_ANGLE);
                            last_servo_move_ms = now;
                        } else if (now - last_servo_move_ms >= SERVO_STABILIZATION_MS) {
                            last_scan.distance_left = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
                            scan_step = 2;
                            last_servo_move_ms = now;
                        }
                        break;
                    case 2: // Center (90°)
                        if (now - last_servo_move_ms >= SERVO_STABILIZATION_MS) {
                            servo.setAngle(SCAN_CENTER_ANGLE);
                            last_servo_move_ms = now;
                            scan_step = 3;
                        }
                        break;
                    case 3: // Right (135°)
                        if (now - last_servo_move_ms >= SERVO_STABILIZATION_MS) {
                            servo.setAngle(SCAN_RIGHT_ANGLE);
                            last_servo_move_ms = now;
                            scan_step = 4;
                        }
                        break;
                    case 4: // Measure right and finish
                        if (now - last_servo_move_ms >= SERVO_STABILIZATION_MS) {
                            last_scan.distance_right = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
                            // Measure front (center) - servo should already be at center from step 2
                            // But we need to wait a bit more and measure
                            servo.setAngle(SCAN_CENTER_ANGLE);
                            vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
                            last_scan.distance_front = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
                            
                            last_scan.timestamp_ms = millis();
                            last_scan.is_valid = (last_scan.distance_left > 0 && 
                                               last_scan.distance_front > 0 && 
                                               last_scan.distance_right > 0);
                            
                            Serial.print("[AUTO] Scan complete - L:");
                            Serial.print(last_scan.distance_left);
                            Serial.print(" F:");
                            Serial.print(last_scan.distance_front);
                            Serial.print(" R:");
                            Serial.println(last_scan.distance_right);
                            
                            // Reset for next scan
                            scan_step = 0;
                            scan_initiated = false;
                            nav_state = STATE_DECISION;
                        }
                        break;
                }
                break;
            }
            
            case STATE_DECISION: {
                // Analyze scan result and choose direction
                command = CMD_STOP;
                
                if (last_scan.is_valid) {
                    static int stored_direction = -1;
                    stored_direction = decideDirection(last_scan);
                    
                    Serial.print("[AUTO] Decision: ");
                    switch (stored_direction) {
                        case 0:
                            Serial.println("TURN LEFT");
                            break;
                        case 1:
                            Serial.println("TURN RIGHT");
                            break;
                        case 2:
                            Serial.println("CONTINUE FORWARD");
                            break;
                        case 3:
                            Serial.println("U-TURN");
                            break;
                    }
                    
                    // Transition to ACTION state (direction is stored in static variable)
                    nav_state = STATE_ACTION;
                } else {
                    // Invalid scan - retry
                    Serial.println("[AUTO] Invalid scan - retrying");
                    nav_state = STATE_SCAN;
                }
                break;
            }
            
            case STATE_ACTION: {
                // Execute chosen movement
                static int action_direction = -1;
                static bool action_initiated = false;
                static unsigned long action_start_ms = 0;
                unsigned long now = millis();
                
                if (!action_initiated) {
                    // Get direction from DECISION state (stored in static variable)
                    // We need to get it from decideDirection again or use a shared static
                    // For simplicity, we'll recalculate it
                    if (last_scan.is_valid) {
                        action_direction = decideDirection(last_scan);
                    } else {
                        // Fallback - go to forward state
                        nav_state = STATE_FORWARD;
                        action_initiated = false;
                        break;
                    }
                    
                    action_initiated = true;
                    action_start_ms = now;
                    
                    // Execute movement
                    if (action_direction == 3) {
                        // U-turn: backup first
                        command = CMD_BACKWARD;
                        Serial.println("[AUTO] Executing U-turn: backing up first");
                    } else {
                        command = executeMovement(action_direction, TURN_DURATION_MS);
                        Serial.print("[AUTO] Executing movement: ");
                        Serial.println(action_direction);
                    }
                } else {
                    if (action_direction == 3) {
                        // U-turn: backup then turn
                        if (now - action_start_ms < STUCK_BACKUP_DURATION_MS / 2) {
                            command = CMD_BACKWARD;
                        } else if (now - action_start_ms < STUCK_BACKUP_DURATION_MS / 2 + TURN_DURATION_MS) {
                            command = CMD_ROTATE_CW; // Turn right for U-turn
                        } else {
                            // U-turn complete
                            command = CMD_STOP;
                            action_initiated = false;
                            action_direction = -1;
                            nav_state = STATE_FORWARD;
                            Serial.println("[AUTO] U-turn complete");
                        }
                    } else if (action_direction == 0 || action_direction == 1) {
                        // Turning left or right
                        if (now - action_start_ms < TURN_DURATION_MS) {
                            command = executeMovement(action_direction, TURN_DURATION_MS);
                        } else {
                            // Turn complete
                            command = CMD_STOP;
                            action_initiated = false;
                            action_direction = -1;
                            nav_state = STATE_FORWARD;
                            Serial.println("[AUTO] Turn complete");
                        }
                    } else {
                        // Forward - just continue briefly then return to FORWARD
                        if (now - action_start_ms < 200) {
                            command = CMD_FORWARD;
                        } else {
                            command = CMD_STOP;
                            action_initiated = false;
                            action_direction = -1;
                            nav_state = STATE_FORWARD;
                        }
                    }
                }
                break;
            }
            
            case STATE_BACKING_UP: {
                // Backing up safely after obstacle detection
                // Continue scanning during backup
                servo.update();
                
                static uint32_t backup_start_ms = 0;
                static bool backup_initiated = false;
                
                if (!backup_initiated) {
                    // Start backup
                    backup_start_ms = millis();
                    backup_initiated = true;
                    Serial.println("[AUTO] Starting safe backup");
                }
                
                // Check if backup duration completed
                if (millis() - backup_start_ms >= BACKUP_TIME_MS) {
                    // Backup completed - continue forward with scanning
                    command = CMD_STOP;
                    nav_state = STATE_FORWARD;
                    backup_initiated = false;
                    Serial.println("[AUTO] Backup completed - resuming forward");
                } else {
                    // Continue backing up, but check distance periodically
                    float rear_distance = ultrasonic_sensor.readDistanceCM();
                    
                    // If obstacle detected behind during backup, stop immediately
                    if (rear_distance >= 0.0f && rear_distance < 15.0f) {
                        Serial.println("[AUTO] Obstacle detected behind during backup - stopping");
                        command = CMD_STOP;
                        nav_state = STATE_FORWARD;  // Will re-evaluate in next cycle
                        backup_initiated = false;
                    } else {
                        // Continue backing
                        command = CMD_BACKWARD;
                    }
                }
                break;
            }
            
            case STATE_STUCK_PIVOTING: {
                // Pivot on place to find a clear path
                // Use triggered scan to find clear direction
                command = CMD_PIVOT_LEFT;  // Default pivot direction
                
                static uint32_t pivot_duration_ms = 0;
                static bool scan_triggered = false;
                pivot_duration_ms += AUTONOMOUS_TASK_PERIOD_MS;
                
                // Trigger scan periodically during pivot
                if (!scan_triggered && pivot_duration_ms >= 500) {
                    // Trigger a scan to find clear path
                    nav_state = STATE_SCAN;
                    scan_triggered = true;
                    pivot_duration_ms = 0;
                    Serial.println("[AUTO] Triggering scan during pivot");
                    break;
                }
                
                // Check scan result if available
                if (last_scan.is_valid) {
                    // Check if any direction is clear
                    if (last_scan.distance_left > MIN_FREE_SPACE_CM ||
                        last_scan.distance_front > MIN_FREE_SPACE_CM ||
                        last_scan.distance_right > MIN_FREE_SPACE_CM) {
                        // Clear path found - use ACTION state to move
                        int direction = decideDirection(last_scan);
                        nav_state = STATE_ACTION;
                        scan_triggered = false;
                        pivot_duration_ms = 0;
                        resetStuckMemory();
                        Serial.println("[AUTO] Path found - exiting stuck state");
                        break;
                    }
                }
                
                if (pivot_duration_ms >= STUCK_PIVOT_MAX_DURATION_MS) {
                    // Max pivot duration reached - try backing up
                    nav_state = STATE_BACKING_UP;
                    pivot_duration_ms = 0;
                    scan_triggered = false;
                    Serial.println("[AUTO] Still stuck after pivot - trying backup");
                }
                break;
            }
            
            case STATE_STOPPED: {
                // Stopped due to error - wait and try to recover
                command = CMD_STOP;
                servo.setAngle(SCAN_CENTER_ANGLE);
                servo.stopSweep();
                
                static uint32_t stop_time_ms = 0;
                stop_time_ms += AUTONOMOUS_TASK_PERIOD_MS;
                if (stop_time_ms >= 1000) {
                    // Try to recover after 1 second
                    nav_state = STATE_FORWARD;
                    stop_time_ms = 0;
                    resetStuckMemory();
                    Serial.println("[AUTO] Attempting recovery");
                }
                break;
            }
        }
        
        // Store last command for reference
        if (command == CMD_ROTATE_CW || command == CMD_ROTATE_CCW) {
            last_command = command;
        }
        
        // Send command to motor control queue
        if (xQueueSend(xCommandQueue, &command, 0) != pdTRUE) {
            Serial.println("[AUTO] Warning: Command queue full!");
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

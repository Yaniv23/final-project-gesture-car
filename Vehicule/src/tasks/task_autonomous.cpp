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

// Stratégies de récupération
enum RecoveryStrategy {
    RECOVERY_BACKUP_TURN,      // Reculer puis tourner aléatoirement
    RECOVERY_PIVOT_360,        // Rotation complète 360°
    RECOVERY_BACKUP_LONG,      // Recul long
    RECOVERY_RANDOM_TURN,      // Tourner aléatoirement avec angle variable
    RECOVERY_WALL_FOLLOW       // Essayer suivi de mur (simplifié: scan seulement)
};

// Historique de positions pour détection mouvement réel
struct PositionHistory {
    float distances[5];           // Historique des 5 dernières distances
    unsigned long timestamps[5];  // Timestamps correspondants
    int index;                    // Index circulaire (0-4)
    bool is_moving;               // Flag: robot bouge-t-il réellement?
};

static PositionHistory position_history;

// Mémoire de récupération
struct RecoveryMemory {
    int attempt_count;                    // Nombre de tentatives de récupération
    RecoveryStrategy last_strategy;       // Dernière stratégie utilisée
    unsigned long last_recovery_time;     // Timestamp dernière récupération
};

static RecoveryMemory recovery_memory;

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
 * @brief Met à jour l'historique de positions pour détecter mouvement réel
 * @param current_distance Distance actuelle mesurée
 * @details Calcule variance des 5 dernières distances pour déterminer si robot bouge
 */
static void updatePositionHistory(float current_distance) {
    unsigned long now = millis();
    
    // Ajouter nouvelle mesure (FIFO circulaire)
    position_history.index = (position_history.index + 1) % 5;
    position_history.distances[position_history.index] = current_distance;
    position_history.timestamps[position_history.index] = now;
    
    // Calculer moyenne des distances
    float mean = 0.0f;
    int valid_count = 0;
    for (int i = 0; i < 5; i++) {
        if (position_history.distances[i] > 0) {
            mean += position_history.distances[i];
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        position_history.is_moving = false;
        return;
    }
    
    mean /= valid_count;
    
    // Calculer variance
    float variance = 0.0f;
    for (int i = 0; i < 5; i++) {
        if (position_history.distances[i] > 0) {
            float diff = position_history.distances[i] - mean;
            variance += diff * diff;
        }
    }
    variance /= valid_count;
    
    // Si variance faible = pas de mouvement (robot probablement bloqué)
    // Si variance élevée = mouvement détecté (robot avance)
    position_history.is_moving = (variance > POSITION_VARIANCE_THRESHOLD);
}

/**
 * @brief Détecte si le robot est coincé (version améliorée)
 * @return true si bloqué, false sinon
 * @details Combine vérification distances ET mouvement réel
 */
static bool checkIfStuckAdvanced() {
    if (!STUCK_DETECTION_ENABLED) {
        return false;
    }
    
    // Vérifier 1: Toutes directions bloquées (logique existante améliorée)
    if (last_scan.is_valid) {
        if (last_scan.distance_left < MIN_FREE_SPACE_CM &&
            last_scan.distance_front < MIN_FREE_SPACE_CM &&
            last_scan.distance_right < MIN_FREE_SPACE_CM) {
            stuck_memory.consecutive_stuck_count++;
            
            if (stuck_memory.consecutive_stuck_count >= STUCK_BACKUP_COUNT) {
                return true;
            }
        } else {
            // Au moins une direction libre - reset compteur
            stuck_memory.consecutive_stuck_count = 0;
        }
    }
    
    // Vérifier 2: Pas de mouvement réel malgré commande FORWARD
    // Cette vérification sera faite dans STATE_FORWARD où on a accès à la commande
    
    // Combiner les deux vérifications
    return (stuck_memory.consecutive_stuck_count >= STUCK_BACKUP_COUNT);
}

/**
 * @brief Choisit une stratégie de récupération (avec variation)
 * @param attempt_number Numéro de tentative (pour varier stratégies)
 * @return Stratégie choisie
 * @details Varie stratégie selon numéro de tentative pour éviter boucles
 */
static RecoveryStrategy chooseRecoveryStrategy(int attempt_number) {
    RecoveryStrategy strategies[] = {
        RECOVERY_BACKUP_TURN,
        RECOVERY_PIVOT_360,
        RECOVERY_BACKUP_LONG,
        RECOVERY_RANDOM_TURN,
        RECOVERY_WALL_FOLLOW
    };
    
    // Varier stratégie selon numéro de tentative (modulo pour éviter répétition)
    int index = (attempt_number % 5);
    return strategies[index];
}

/**
 * @brief Exécute une stratégie de récupération (non-bloquante)
 * @param strategy Stratégie à exécuter
 * @param command Commande à envoyer (sortie)
 * @param recovery_start_ms Timestamp de début de récupération (référence pour modification)
 * @param recovery_step Étape actuelle de la stratégie (0=init, 1=backup, 2=turn, etc.)
 * @return true si stratégie complète, false sinon
 * @details Implémente 5 stratégies différentes pour éviter boucles
 */
static bool executeRecoveryStrategy(RecoveryStrategy strategy, uint8_t& command, 
                                     unsigned long& recovery_start_ms, int& recovery_step) {
    unsigned long now = millis();
    static uint8_t recovery_cmd = CMD_STOP;
    static uint32_t random_turn_duration = 0;
    static unsigned long phase_start_ms = 0;
    
    switch (strategy) {
        case RECOVERY_BACKUP_TURN: {
            if (recovery_step == 0) {
                Serial.println("[AUTO] Recovery: BACKUP_TURN");
                recovery_cmd = CMD_BACKWARD;
                command = recovery_cmd;
                recovery_step = 1;
                phase_start_ms = now;
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
                    recovery_memory.last_strategy = strategy;
                    recovery_memory.last_recovery_time = now;
                    return true;
                }
            }
            break;
        }
        
        case RECOVERY_PIVOT_360: {
            if (recovery_step == 0) {
                Serial.println("[AUTO] Recovery: PIVOT_360");
                recovery_cmd = CMD_ROTATE_CW;
                command = recovery_cmd;
                recovery_step = 1;
                phase_start_ms = now;
            } else if (recovery_step == 1) {
                // 360° rotation (4 × 90°)
                if (now - phase_start_ms < TURN_DURATION_MS * 4) {
                    command = recovery_cmd;
                } else {
                    command = CMD_STOP;
                    recovery_memory.last_strategy = strategy;
                    recovery_memory.last_recovery_time = now;
                    return true;
                }
            }
            break;
        }
        
        case RECOVERY_BACKUP_LONG: {
            if (recovery_step == 0) {
                Serial.println("[AUTO] Recovery: BACKUP_LONG");
                recovery_cmd = CMD_BACKWARD;
                command = recovery_cmd;
                recovery_step = 1;
                phase_start_ms = now;
            } else if (recovery_step == 1) {
                // Long backup
                if (now - phase_start_ms < STUCK_BACKUP_DURATION_MS) {
                    command = recovery_cmd;
                } else {
                    command = CMD_STOP;
                    recovery_memory.last_strategy = strategy;
                    recovery_memory.last_recovery_time = now;
                    return true;
                }
            }
            break;
        }
        
        case RECOVERY_RANDOM_TURN: {
            if (recovery_step == 0) {
                Serial.println("[AUTO] Recovery: RANDOM_TURN");
                recovery_cmd = (random(0, 2) == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
                command = recovery_cmd;
                // Angle aléatoire entre 90° et 270°
                random_turn_duration = TURN_DURATION_MS + random(0, TURN_DURATION_MS * 2);
                recovery_step = 1;
                phase_start_ms = now;
            } else if (recovery_step == 1) {
                // Random turn
                if (now - phase_start_ms < random_turn_duration) {
                    command = recovery_cmd;
                } else {
                    command = CMD_STOP;
                    recovery_memory.last_strategy = strategy;
                    recovery_memory.last_recovery_time = now;
                    return true;
                }
            }
            break;
        }
        
        case RECOVERY_WALL_FOLLOW: {
            if (recovery_step == 0) {
                Serial.println("[AUTO] Recovery: WALL_FOLLOW (scan only)");
                // Simplifié: juste scanner pour trouver mur
                // Peut être étendu plus tard avec suivi de mur complet
                scan3Directions(ultrasonic_sensor, servo, last_scan);
                recovery_memory.last_strategy = strategy;
                recovery_memory.last_recovery_time = now;
                recovery_step = 1;
                return true; // Scan is immediate
            }
            break;
        }
    }
    
    return false; // Strategy not complete yet
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
    
    // Initialiser historique de positions
    for (int i = 0; i < 5; i++) {
        position_history.distances[i] = 0.0f;
        position_history.timestamps[i] = 0;
    }
    position_history.index = 0;
    position_history.is_moving = false;
    
    // Initialiser mémoire de récupération
    recovery_memory.attempt_count = 0;
    recovery_memory.last_strategy = RECOVERY_BACKUP_TURN;
    recovery_memory.last_recovery_time = 0;
    
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
                // Reset position history
                for (int i = 0; i < 5; i++) {
                    position_history.distances[i] = 0.0f;
                    position_history.timestamps[i] = 0;
                }
                position_history.index = 0;
                position_history.is_moving = false;
                // Reset recovery memory
                recovery_memory.attempt_count = 0;
                recovery_memory.last_strategy = RECOVERY_BACKUP_TURN;
                recovery_memory.last_recovery_time = 0;
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
                
                // Track last sent command for stuck detection
                static uint8_t last_sent_command = CMD_STOP;
                static unsigned long last_forward_command_time = 0;
                
                // Ensure servo is at center position
                servo.setAngle(SCAN_CENTER_ANGLE);
                servo.stopSweep();
                
                // Measure distance only if servo has stabilized
                if (now - last_servo_update_ms >= SERVO_STABILIZATION_MS) {
                    float distance = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
                    
                    // Mettre à jour historique de positions pour détection mouvement
                    if (distance >= 0.0f && distance <= 400.0f) {
                        updatePositionHistory(distance);
                    }
                    
                    // Check for obstacle ahead
                    if (distance >= 0.0f && distance < CRITICAL_DISTANCE_CM) {
                        // Obstacle detected - stop and trigger scan
                        command = CMD_STOP;
                        nav_state = STATE_SCAN;
                        Serial.print("[AUTO] Obstacle detected at ");
                        Serial.print(distance);
                        Serial.println(" cm - triggering scan");
                        last_sent_command = CMD_STOP;
                        break;
                    }
                    
                    last_servo_update_ms = now;
                }
                
                // Vérifier mouvement réel si commande FORWARD active depuis X ms
                if (last_sent_command == CMD_FORWARD) {
                    if (now - last_forward_command_time > STUCK_MOVEMENT_CHECK_MS) {
                        if (!position_history.is_moving) {
                            stuck_memory.consecutive_failed_attempts++;
                            Serial.print("[AUTO] No movement detected - attempts: ");
                            Serial.println(stuck_memory.consecutive_failed_attempts);
                            
                            if (stuck_memory.consecutive_failed_attempts >= STUCK_THRESHOLD_ATTEMPTS) {
                                Serial.println("[AUTO] Stuck detected: no movement despite FORWARD command");
                                nav_state = STATE_STUCK_PIVOTING;
                                command = CMD_STOP;
                                last_sent_command = CMD_STOP;
                                stuck_memory.consecutive_failed_attempts = 0;
                                break;
                            }
                        } else {
                            // Mouvement détecté - reset compteur
                            stuck_memory.consecutive_failed_attempts = 0;
                        }
                    }
                }
                
                // Check if stuck using advanced detection
                if (checkIfStuckAdvanced()) {
                    // Vehicle is stuck - enter stuck pivoting state
                    nav_state = STATE_STUCK_PIVOTING;
                    command = CMD_STOP;
                    last_sent_command = CMD_STOP;
                    Serial.println("[AUTO] Stuck detected - entering pivot mode");
                    break;
                }
                
                // Path is clear - continue forward
                command = CMD_FORWARD;
                if (command == CMD_FORWARD) {
                    last_sent_command = CMD_FORWARD;
                    last_forward_command_time = now;
                } else {
                    last_sent_command = command;
                }
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
                        // Turning left or right - utiliser tournant adaptatif
                        static bool adaptive_turn_initiated = false;
                        static unsigned long adaptive_turn_start_ms = 0;
                        static unsigned long last_check_ms = 0;
                        static uint8_t adaptive_turn_cmd = CMD_STOP;
                        
                        if (!adaptive_turn_initiated) {
                            // Démarrer tournant adaptatif
                            adaptive_turn_cmd = (action_direction == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
                            command = adaptive_turn_cmd;
                            adaptive_turn_initiated = true;
                            adaptive_turn_start_ms = now;
                            last_check_ms = now;
                            Serial.println("[AUTO] Starting adaptive turn");
                        } else {
                            // Vérifier périodiquement si chemin clair pendant rotation
                            if (now - last_check_ms >= ADAPTIVE_TURN_CHECK_INTERVAL_MS) {
                                last_check_ms = now;
                                
                                // Mesurer distance devant pendant rotation (servo au centre)
                                servo.setAngle(SCAN_CENTER_ANGLE);
                                // Pas de delay ici - on attendra au prochain cycle
                                
                                // Mesure rapide (3 échantillons au lieu de 5 pour réduire latence)
                                float distance = filteredDistance(ultrasonic_sensor, 3);
                                
                                // Si chemin clair trouvé, arrêter rotation immédiatement
                                if (distance >= MIN_FREE_SPACE_CM && distance > 0) {
                                    command = CMD_STOP;
                                    adaptive_turn_initiated = false;
                                    action_initiated = false;
                                    action_direction = -1;
                                    nav_state = STATE_FORWARD;
                                    Serial.print("[AUTO] Adaptive turn: path clear at ");
                                    Serial.print(distance);
                                    Serial.println(" cm");
                                    break;
                                }
                            }
                            
                            // Vérifier timeout
                            if (now - adaptive_turn_start_ms >= ADAPTIVE_TURN_MAX_DURATION_MS) {
                                // Timeout - arrêter rotation (safety)
                                command = CMD_STOP;
                                adaptive_turn_initiated = false;
                                action_initiated = false;
                                action_direction = -1;
                                nav_state = STATE_FORWARD;
                                Serial.println("[AUTO] Adaptive turn timeout - continuing forward");
                            } else {
                                // Continuer rotation
                                command = adaptive_turn_cmd;
                            }
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
                // Utiliser stratégies de récupération multiples
                command = CMD_STOP;
                
                static bool recovery_initiated = false;
                static unsigned long recovery_start_ms = 0;
                static int recovery_step = 0;
                static RecoveryStrategy current_strategy = RECOVERY_BACKUP_TURN;
                unsigned long now = millis();
                
                if (!recovery_initiated) {
                    // Choisir et démarrer stratégie de récupération
                    current_strategy = chooseRecoveryStrategy(recovery_memory.attempt_count);
                    recovery_initiated = true;
                    recovery_start_ms = now;
                    recovery_step = 0;
                    recovery_memory.attempt_count++;
                    
                    Serial.print("[AUTO] Recovery attempt #");
                    Serial.println(recovery_memory.attempt_count);
                }
                
                // Exécuter stratégie (non-bloquante)
                bool strategy_complete = executeRecoveryStrategy(current_strategy, command, 
                                                                 recovery_start_ms, recovery_step);
                
                if (strategy_complete) {
                    // Stratégie complète - scanner pour trouver chemin
                    nav_state = STATE_SCAN;
                    recovery_initiated = false;
                    recovery_step = 0;
                    Serial.println("[AUTO] Recovery complete - scanning for clear path");
                    break;
                }
                
                // Si trop de tentatives, essayer backup long et reset
                if (recovery_memory.attempt_count >= RECOVERY_MAX_ATTEMPTS) {
                    Serial.println("[AUTO] Max recovery attempts reached - trying long backup");
                    current_strategy = RECOVERY_BACKUP_LONG;
                    recovery_step = 0;
                    recovery_start_ms = now;
                    recovery_memory.attempt_count = 0; // Reset pour prochaine fois
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

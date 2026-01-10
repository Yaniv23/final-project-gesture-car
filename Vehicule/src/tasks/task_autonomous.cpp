/**
 * @file task_autonomous.cpp
 * @brief Autonomous navigation task - 50ms period, Priority 3
 * @details Enhanced obstacle avoidance using servo-mounted ultrasonic sensor
 *          Continuous scanning while moving - servo sweeps 0° to 60° continuously
 *          Distance map stores measurements by angle for intelligent navigation
 *          Includes stuck detection with pivot-on-place recovery
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

// Distance reading structure for distance map
struct DistanceReading {
    int angle;
    float distance;
    unsigned long timestamp_ms;
};

// Distance map: stores distance measurements by angle
static DistanceReading distance_map[SCAN_ANGLES_COUNT];
static bool distance_map_valid[SCAN_ANGLES_COUNT];

// Stuck detection memory structure
struct StuckDetection {
    unsigned long last_forward_attempt_ms;  // Dernière tentative d'avancer
    unsigned long last_position_change_ms;  // Dernier changement de position
    float last_best_distance;               // Meilleure distance trouvée
    int consecutive_failed_attempts;        // Nombre d'échecs consécutifs
    bool is_stuck;                          // Flag de blocage
};

static StuckDetection stuck_memory;

// Navigation state machine
enum NavigationState {
    STATE_FORWARD,          // Moving forward with continuous scanning
    STATE_BACKING_UP,       // Recul sécurisé
    STATE_TURNING,          // Turning to avoid obstacle
    STATE_STUCK_PIVOTING,  // Pivot sur place pour trouver chemin
    STATE_STOPPED           // Stopped (safety)
};

/**
 * @brief Store distance reading in the distance map
 * @param angle Angle in degrees (0-60)
 * @param distance Distance in cm
 */
static void storeDistanceReading(int angle, float distance) {
    // Find the index for this angle (0°, 5°, 10°, ..., 60°)
    int index = angle / SCAN_STEP_DEG;
    if (index >= 0 && index < SCAN_ANGLES_COUNT) {
        distance_map[index].angle = angle;
        distance_map[index].distance = distance;
        distance_map[index].timestamp_ms = millis();
        distance_map_valid[index] = true;
    }
}

/**
 * @brief Get distance reading for a specific angle
 * @param angle Angle in degrees
 * @return Distance in cm, or -1.0 if not available
 */
static float getDistanceAtAngle(int angle) {
    int index = angle / SCAN_STEP_DEG;
    if (index >= 0 && index < SCAN_ANGLES_COUNT && distance_map_valid[index]) {
        return distance_map[index].distance;
    }
    return -1.0f;
}

/**
 * @brief Find the angle with the best (longest) distance
 * @return Best angle in degrees, or -1 if no valid data
 */
static int findBestAngle() {
    float max_distance = -1.0f;
    int best_angle = -1;
    
    for (int i = 0; i < SCAN_ANGLES_COUNT; i++) {
        if (distance_map_valid[i] && distance_map[i].distance > max_distance) {
            max_distance = distance_map[i].distance;
            best_angle = distance_map[i].angle;
        }
    }
    
    return best_angle;
}

/**
 * @brief Check if there's an obstacle in the center (forward direction)
 * @return true if obstacle detected in center angles
 */
static bool isObstacleInCenter() {
    int center_min = SCAN_CENTER_ANGLE - SCAN_CENTER_TOLERANCE;
    int center_max = SCAN_CENTER_ANGLE + SCAN_CENTER_TOLERANCE;
    
    for (int angle = center_min; angle <= center_max; angle += SCAN_STEP_DEG) {
        float dist = getDistanceAtAngle(angle);
        if (dist >= 0.0f && dist < OBSTACLE_DISTANCE_THRESHOLD_CM) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if all directions are blocked
 * @return true if all measured distances are below threshold
 */
static bool areAllDirectionsBlocked() {
    bool has_valid_data = false;
    bool all_blocked = true;
    
    for (int i = 0; i < SCAN_ANGLES_COUNT; i++) {
        if (distance_map_valid[i]) {
            has_valid_data = true;
            if (distance_map[i].distance >= STUCK_ALL_DIRECTIONS_THRESHOLD) {
                all_blocked = false;
                break;
            }
        }
    }
    
    return has_valid_data && all_blocked;
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
 * @brief Check if vehicle is stuck
 * @return true if stuck, false otherwise
 */
static bool checkIfStuck() {
    if (!STUCK_DETECTION_ENABLED) {
        return false;
    }
    
    // Also check if all directions are blocked
    if (areAllDirectionsBlocked()) {
        return true;
    }
    
    return stuck_memory.is_stuck;
}

/**
 * @brief Turn vehicle toward a specific angle
 * @param target_angle Target angle in degrees (0-60)
 * @return Command byte for turning
 */
static uint8_t turnTowardAngle(int target_angle) {
    if (target_angle < 0) {
        return CMD_ROTATE_CW;  // Default to right if invalid
    }
    
    // If target is to the left of center, turn left (CCW)
    // If target is to the right of center, turn right (CW)
    if (target_angle < SCAN_CENTER_ANGLE) {
        return CMD_ROTATE_CCW;
    } else {
        return CMD_ROTATE_CW;
    }
}

/**
 * @brief Initialize distance map
 */
static void initDistanceMap() {
    for (int i = 0; i < SCAN_ANGLES_COUNT; i++) {
        distance_map[i].angle = i * SCAN_STEP_DEG;
        distance_map[i].distance = -1.0f;
        distance_map[i].timestamp_ms = 0;
        distance_map_valid[i] = false;
    }
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
    
    // Initialize distance map and stuck memory
    initDistanceMap();
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
                Serial.println("[AUTO] Starting continuous scanning...");
                // Reset navigation state to forward with continuous scanning
                nav_state = STATE_FORWARD;
                // Reset distance map and stuck memory
                initDistanceMap();
                resetStuckMemory();
                // Reset timing variables
                turn_duration_ms = 0;
                last_command = CMD_STOP;
                last_servo_update_ms = millis();
                // Start continuous sweep (rest_interval_ms = 0 for continuous)
                servo.startSweep(SCAN_MIN_ANGLE, SCAN_MAX_ANGLE, SCAN_STEP_DEG, SCAN_STEP_INTERVAL_MS, SCAN_REST_INTERVAL_MS);
                Serial.print("[AUTO] Continuous sweep started: ");
                Serial.print(SCAN_MIN_ANGLE);
                Serial.print("° to ");
                Serial.print(SCAN_MAX_ANGLE);
                Serial.print("° (step: ");
                Serial.print(SCAN_STEP_DEG);
                Serial.print("°, interval: ");
                Serial.print(SCAN_STEP_INTERVAL_MS);
                Serial.println("ms)");
            }
            sensors_initialized = true;
        }
        
        // Default command
        uint8_t command = CMD_STOP;
        
        // Navigation state machine
        switch (nav_state) {
            case STATE_FORWARD: {
                // Continuous scanning while moving forward
                unsigned long now = millis();
                
                // Update servo sweep (asynchronous)
                servo.update();
                
                // Get current servo angle
                int current_angle = servo.getCurrentAngle();
                
                // Measure distance only if servo has stabilized
                if (now - last_servo_update_ms >= SCAN_SERVO_STABILIZATION_MS) {
                    float distance = ultrasonic_sensor.readDistanceCM();
                    
                    // Filter out invalid readings (distance > 400cm is likely error)
                    if (distance >= 0.0f && distance <= 400.0f) {
                        storeDistanceReading(current_angle, distance);
                    }
                    
                    last_servo_update_ms = now;
                }
                
                // Decision logic based on distance map
                int best_angle = findBestAngle();
                float best_distance = (best_angle >= 0) ? getDistanceAtAngle(best_angle) : -1.0f;
                
                // Check if stuck (check previous command state)
                static uint8_t prev_forward_command = CMD_STOP;
                bool is_moving = (prev_forward_command == CMD_FORWARD);
                updateStuckMemory(best_distance, is_moving);
                
                if (checkIfStuck()) {
                    // Vehicle is stuck - enter stuck pivoting state
                    nav_state = STATE_STUCK_PIVOTING;
                    command = CMD_STOP;
                    Serial.println("[AUTO] Stuck detected - entering pivot mode");
                    break;
                }
                
                // Check for obstacle ahead (center angles)
                if (isObstacleInCenter()) {
                    // Obstacle detected ahead - backup
                    nav_state = STATE_BACKING_UP;
                    command = CMD_STOP;
                    Serial.println("[AUTO] Obstacle detected ahead - backing up");
                } else if (best_angle >= 0 && best_distance >= OBSTACLE_DISTANCE_THRESHOLD_CM) {
                    // Path is clear - continue forward
                    command = CMD_FORWARD;
                } else {
                    // No valid data or all blocked - stop and scan
                    command = CMD_STOP;
                    Serial.println("[AUTO] No valid scan data - waiting");
                }
                
                // Update previous command for stuck detection
                prev_forward_command = command;
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
            
            case STATE_TURNING: {
                // Turning to avoid obstacle - continue scanning during turn
                servo.update();
                
                // Measure distance at current servo angle
                unsigned long now = millis();
                if (now - last_servo_update_ms >= SCAN_SERVO_STABILIZATION_MS) {
                    int current_angle = servo.getCurrentAngle();
                    float distance = ultrasonic_sensor.readDistanceCM();
                    if (distance >= 0.0f && distance <= 400.0f) {
                        storeDistanceReading(current_angle, distance);
                    }
                    last_servo_update_ms = now;
                }
                
                turn_duration_ms += AUTONOMOUS_TASK_PERIOD_MS;
                
                if (turn_duration_ms < TURN_DURATION_MS) {
                    // Continue turning
                    command = last_command;  // Keep same turn direction
                } else {
                    // Turn completed - check distance map for best direction
                    int best_angle = findBestAngle();
                    float best_distance = (best_angle >= 0) ? getDistanceAtAngle(best_angle) : -1.0f;
                    
                    if (best_distance >= OBSTACLE_DISTANCE_THRESHOLD_CM) {
                        // Path is clear - go forward
                        nav_state = STATE_FORWARD;
                        command = CMD_FORWARD;
                        turn_duration_ms = 0;
                        Serial.print("[AUTO] Turn completed - path clear (");
                        Serial.print(best_distance);
                        Serial.println(" cm) - going forward");
                    } else {
                        // Still obstacle - continue scanning and re-evaluate
                        nav_state = STATE_FORWARD;  // Will re-evaluate in next cycle
                        command = CMD_STOP;
                        turn_duration_ms = 0;
                        Serial.println("[AUTO] Turn completed - re-evaluating");
                    }
                }
                break;
            }
            
            case STATE_STUCK_PIVOTING: {
                // Pivot on place to find a clear path
                // Continue scanning during pivot
                servo.update();
                
                // Measure distance at current servo angle
                unsigned long now = millis();
                if (now - last_servo_update_ms >= SCAN_SERVO_STABILIZATION_MS) {
                    int current_angle = servo.getCurrentAngle();
                    float distance = ultrasonic_sensor.readDistanceCM();
                    if (distance >= 0.0f && distance <= 400.0f) {
                        storeDistanceReading(current_angle, distance);
                    }
                    last_servo_update_ms = now;
                }
                
                // Pivot on place
                static uint32_t pivot_duration_ms = 0;
                pivot_duration_ms += AUTONOMOUS_TASK_PERIOD_MS;
                
                // Check if a clear path has been found
                int best_angle = findBestAngle();
                float best_distance = (best_angle >= 0) ? getDistanceAtAngle(best_angle) : -1.0f;
                
                if (best_distance >= OBSTACLE_DISTANCE_THRESHOLD_CM) {
                    // Clear path found!
                    nav_state = STATE_TURNING;
                    command = turnTowardAngle(best_angle);
                    last_command = command;
                    turn_duration_ms = 0;
                    pivot_duration_ms = 0;
                    resetStuckMemory();
                    Serial.print("[AUTO] Path found at angle ");
                    Serial.print(best_angle);
                    Serial.print("° (distance: ");
                    Serial.print(best_distance);
                    Serial.println(" cm) - exiting stuck state");
                } else if (pivot_duration_ms >= STUCK_PIVOT_MAX_DURATION_MS) {
                    // Max pivot duration reached - try backing up
                    nav_state = STATE_BACKING_UP;
                    pivot_duration_ms = 0;
                    Serial.println("[AUTO] Still stuck after pivot - trying backup");
                } else {
                    // Continue pivoting
                    command = CMD_PIVOT_LEFT;  // Default pivot direction
                }
                break;
            }
            
            case STATE_STOPPED: {
                // Stopped due to error - continue scanning while stopped
                servo.update();
                
                // Measure distance at current servo angle
                unsigned long now = millis();
                if (now - last_servo_update_ms >= SCAN_SERVO_STABILIZATION_MS) {
                    int current_angle = servo.getCurrentAngle();
                    float distance = ultrasonic_sensor.readDistanceCM();
                    if (distance >= 0.0f && distance <= 400.0f) {
                        storeDistanceReading(current_angle, distance);
                    }
                    last_servo_update_ms = now;
                }
                
                // Wait and try to recover
                command = CMD_STOP;
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
        
        // Store last command for turning state
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

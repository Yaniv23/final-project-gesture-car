/**
 * @file task_autonomous.cpp
 * @brief Autonomous obstacle avoidance (simplified, blocking)
 */

#include <Arduino.h>
#include <array>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../shared/queues.h"
#include "../shared/sensor_state.h"
#include "../communication/command_protocol.h"
#include "../control/mode_manager.h"
#include "../drivers/servo_driver.h"

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

namespace {

// Helper to push motion commands without blocking long
void sendMotionCommand(uint8_t cmd) {
    if (xCommandQueue != NULL) {
        xQueueSend(xCommandQueue, &cmd, 0);
    }
}

// Three-position scan order: 1st RIGHT (170°), 2nd CENTER (90°), 3rd LEFT (10°).
// Indices: 0=right (best→CW), 1=center (best→forward), 2=left (best→CCW).
// Reads raw from front sensor per direction and applies per-direction filter (no cross-direction state).
std::array<float, 3> scanThreeDirections(ServoDriver& servo) {
    const int angles[3] = {10, 90, 180};  // right, center, left
    std::array<float, 3> distances = {-1.0f, -1.0f, -1.0f};
    constexpr int samples_per_direction = 20;
    const TickType_t sample_interval = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);  // 60 ms
    const TickType_t stabilization_delay = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);  // 60 ms after servo move

    for (int i = 0; i < 3; ++i) {
        servo.setAngle(angles[i], false);
        vTaskDelay(stabilization_delay);

        // Collect raw samples for this direction only (no filter state from other directions)
        std::array<float, samples_per_direction> samples;
        samples.fill(-1.0f);
        for (int s = 0; s < samples_per_direction; ++s) {
            vTaskDelay(sample_interval);
            float raw = -1.0f;
            if (readFrontSensorRaw(&raw)) {
                samples[s] = raw;
            }
        }

        // Simple per-direction filter: average of valid in-range values (0 < d < 400 cm)
        float sum = 0.0f;
        int valid_count = 0;
        for (int s = 0; s < samples_per_direction; ++s) {
            float d = samples[s];
            if (d > 0.0f && d < 400.0f) {
                sum += d;
                valid_count++;
            }
        }

        if (valid_count > 0) {
            distances[i] = sum / valid_count;
        } else {
            const char* pos_names[3] = {"Left", "Center", "Right"};
            Serial.print("[SCAN] ");
            Serial.print(pos_names[i]);
            Serial.println(": NO valid readings!");
        }
    }

    servo.setAngle(90, false);
    return distances;
}

// Pick index of the longest clear direction (0 = right, 1 = center, 2 = left)
// Improved: Better handling of invalid values, ensures center is considered
int pickBestDirection(const std::array<float, 3>& distances, float& max_distance) {
    // Find the best valid direction
    int best = -1;
    max_distance = -1.0f;
    
    // Check all three directions
    for (int i = 0; i < 3; ++i) {
        // Only consider valid distances (> 0 and < 400)
        if (distances[i] > 0.0f && distances[i] < 400.0f) {
            if (distances[i] > max_distance) {
                max_distance = distances[i];
                best = i;
            }
        }
    }
    
    // If no valid direction found, default to center (index 1)
    if (best < 0) {
        best = 1;  // Default to center
        max_distance = -1.0f;
        Serial.println("[PICK] No valid directions found, defaulting to center");
    }
    
    // Debug: Log the chosen direction
    const char* dir_names[3] = {"LEFT", "CENTER", "RIGHT"};
    Serial.print("[PICK] Best: ");
    Serial.print(dir_names[best]);
    Serial.print(" (");
    Serial.print(max_distance);
    Serial.print(" cm) - L:");
    Serial.print(distances[0]);
    Serial.print(" C:");
    Serial.print(distances[1]);
    Serial.print(" R:");
    Serial.println(distances[2]);
    
    return best;
}

} // namespace

void task_autonomous(void *pvParameters) {
    // Wait for setup to complete
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ModeManager& mode_mgr = ModeManager::getInstance();

    ServoDriver servo;
    bool servo_ok = servo.init(SERVO_PIN);
    if (servo_ok) {
        servo.setAngle(90, true);  // Center before starting
    }

    bool init_logged = false;       // for "servo init failed" (once)
    bool active_logged = false;    // for "autonomous active" (once)

    // 60 ms step: matches task_sensors read interval; we consume getSensorState() only
    const TickType_t forward_delay = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);
    const TickType_t backup_step = pdMS_TO_TICKS(100);
    const TickType_t backup_total = pdMS_TO_TICKS(800);
    const TickType_t turn_window = pdMS_TO_TICKS(1200);
    const TickType_t rotate_recovery = pdMS_TO_TICKS(800);

    // Track current rotation direction for scan recovery
    uint8_t current_rotate_cmd = CMD_ROTATE_CW;  // Default to CW

    while (1) {
        if (!mode_mgr.isAutonomousMode()) {
            init_logged = false;
            active_logged = false;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!servo_ok && !init_logged) {
            Serial.println("[AUTO] Servo init failed - staying stopped");
            init_logged = true;
            sendMotionCommand(CMD_STOP);
        }

        if (!servo_ok) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Log once when autonomous is active with servo ready
        if (!active_logged) {
            active_logged = true;
            Serial.println("[AUTO] Autonomous active (servo ready, using SensorState every 60 ms)");
        }

        // Baseline forward step
        sendMotionCommand(CMD_FORWARD);
        vTaskDelay(forward_delay);

        // Read sensor state from shared structure (non-blocking)
        SensorState sensor_state;
        if (!getSensorState(&sensor_state)) {
            // Failed to read sensor state - skip this iteration
            continue;
        }

        // Check for obstacle using front distance from sensor state
        float front_distance = sensor_state.front_distance;
        bool obstacleDetected = (front_distance > 0.0f && front_distance < 20.0f);
        if (!obstacleDetected) {
            continue;
        }

        // Obstacle reaction: stop first
        sendMotionCommand(CMD_STOP);

        // Backup with rear safety check
        sendMotionCommand(CMD_BACKWARD);
        TickType_t backup_start = xTaskGetTickCount();
        bool rear_blocked = false;
        while ((xTaskGetTickCount() - backup_start) < backup_total) {
            vTaskDelay(backup_step);
            // Read sensor state during backup (non-blocking)
            SensorState backup_sensor_state;
            if (getSensorState(&backup_sensor_state)) {
                float rear_distance = backup_sensor_state.rear_distance;
                if (rear_distance > 0.0f && rear_distance < 15.0f) {
                    rear_blocked = true;
                    break;
                }
            }
        }
        sendMotionCommand(CMD_STOP);

        if (rear_blocked) {
            Serial.println("[AUTO] Rear obstacle detected during backup");
        }

        // Scan environment (uses getSensorState() only; task_sensors provides data every 60 ms)
        std::array<float, 3> distances = scanThreeDirections(servo);
        float max_distance = 0.0f;
        int best_dir = pickBestDirection(distances, max_distance);

        // Stuck recovery if all sides blocked
        // Continue in current direction (CW/CCW) where distance is largest
        int recovery_attempts = 0;
        while (max_distance <= 40.0f && recovery_attempts < 3) {
            // Find direction with largest distance
            int max_dir = -1;
            float max_dist = -1.0f;
            for (int i = 0; i < 3; ++i) {
                if (distances[i] > 0.0f && distances[i] < 400.0f && distances[i] > max_dist) {
                    max_dist = distances[i];
                    max_dir = i;
                }
            }
            
            // Determine motion based on largest distance
            // Right (0) -> CW, Center (1) -> forward, Left (2) -> CCW, none (-1) -> keep rotating to get unstuck
            uint8_t motion_cmd;
            if (max_dir == 0) {
                motion_cmd = CMD_ROTATE_CW;
                current_rotate_cmd = CMD_ROTATE_CW;
            } else if (max_dir == 2) {
                motion_cmd = CMD_ROTATE_CCW;
                current_rotate_cmd = CMD_ROTATE_CCW;
            } else if (max_dir == 1) {
                // Center has largest distance -> go forward (clearest path is ahead)
                motion_cmd = CMD_FORWARD;
            } else {
                // No valid direction -> keep rotating in current direction to try to get unstuck
                motion_cmd = current_rotate_cmd;
            }
            
            Serial.print("[SCAN] All directions blocked, ");
            if (motion_cmd == CMD_FORWARD) {
                Serial.print("going forward ");
            } else {
                Serial.print((motion_cmd == CMD_ROTATE_CW) ? "rotating CW " : "rotating CCW ");
            }
            Serial.print(" (max distance: ");
            Serial.print(max_dist);
            Serial.print(" cm at ");
            const char* dir_names[3] = {"LEFT", "CENTER", "RIGHT"};
            if (max_dir >= 0) {
                Serial.print(dir_names[max_dir]);
            } else {
                Serial.print("NONE");
            }
            Serial.println(")");
            
            sendMotionCommand(motion_cmd);
            vTaskDelay(rotate_recovery);
            sendMotionCommand(CMD_STOP);

            distances = scanThreeDirections(servo);
            best_dir = pickBestDirection(distances, max_distance);
            recovery_attempts++;
        }

        if (max_distance <= 40.0f) {
            Serial.println("[AUTO] Stuck: no clear path after recovery attempts");
            sendMotionCommand(CMD_STOP);
            continue;
        }

        // Execute chosen direction: 0=right→CW, 1=center→forward, 2=left→CCW
        if (best_dir == 0) {
            current_rotate_cmd = CMD_ROTATE_CW;
            sendMotionCommand(CMD_ROTATE_CW);
            vTaskDelay(turn_window);
            sendMotionCommand(CMD_STOP);
        } else if (best_dir == 2) {
            current_rotate_cmd = CMD_ROTATE_CCW;
            sendMotionCommand(CMD_ROTATE_CCW);
            vTaskDelay(turn_window);
            sendMotionCommand(CMD_STOP);
        } else {
            // best_dir == 1: center is best, move forward on next loop iteration
        }
    }
}

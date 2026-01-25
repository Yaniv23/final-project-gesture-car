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
#include "../communication/command_protocol.h"
#include "../control/mode_manager.h"
#include "../drivers/ultrasonic_driver.h"
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

// Three-position scan: left (30°), center (90°), right (150°)
// Improved: Multiple readings per position to ensure valid data, especially for center
std::array<float, 3> scanThreeDirections(ServoDriver& servo, Ultrasonic& sensor) {
    const int angles[3] = {30, 90, 150};
    std::array<float, 3> distances = {-1.0f, -1.0f, -1.0f};
    
    // Number of readings per position (same for all angles for consistency)
    const int readings_per_position = 7;  // 7 readings for each angle (Left, Center, Right)
    const TickType_t stabilization_delay = pdMS_TO_TICKS(250);  // Wait for servo to stabilize
    const TickType_t reading_interval = pdMS_TO_TICKS(80);       // Interval between readings

    for (int i = 0; i < 3; ++i) {
        // Move servo to position and wait for stabilization
        servo.setAngle(angles[i], false);  // Non-blocking
        vTaskDelay(stabilization_delay);
        
        // Verify servo is stable (optional check)
        // Note: isStable() uses millis() which may not be accurate in FreeRTOS
        // The delay above should be sufficient
        
        // Take multiple readings for this position
        float sum = 0.0f;
        int valid_readings = 0;
        float min_distance = 400.0f;
        float max_distance = 0.0f;
        
        for (int reading = 0; reading < readings_per_position; ++reading) {
            float distance = sensor.readDistanceCM();
            
            // Only count valid readings (0 < distance < 400)
            if (distance > 0.0f && distance < 400.0f) {
                sum += distance;
                valid_readings++;
                if (distance < min_distance) min_distance = distance;
                if (distance > max_distance) max_distance = distance;
            }
            
            // Small delay between readings to allow sensor to settle
            if (reading < readings_per_position - 1) {
                vTaskDelay(reading_interval);
            }
        }
        
        // Calculate average of valid readings
        if (valid_readings > 0) {
            distances[i] = sum / valid_readings;
            
            // Debug: Log if any position has issues (not enough valid readings)
            if (valid_readings < readings_per_position) {
                const char* pos_names[3] = {"Left", "Center", "Right"};
                Serial.print("[SCAN] ");
                Serial.print(pos_names[i]);
                Serial.print(": only ");
                Serial.print(valid_readings);
                Serial.print("/");
                Serial.print(readings_per_position);
                Serial.println(" valid readings");
            }
        } else {
            // No valid readings - keep -1.0f
            const char* pos_names[3] = {"Left", "Center", "Right"};
            Serial.print("[SCAN] ");
            Serial.print(pos_names[i]);
            Serial.println(": NO valid readings!");
        }
    }

    // Return servo to center for the next cycle
    servo.setAngle(90, false);
    return distances;
}

// Pick index of the longest clear direction (0 = left, 1 = center, 2 = right)
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

    Ultrasonic front_sensor;
    Ultrasonic rear_sensor;
    ServoDriver servo;

    bool front_ok = front_sensor.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
    bool rear_ok = rear_sensor.init(ULTRASONIC_TRIG_REAR, ULTRASONIC_ECHO_REAR);
    bool servo_ok = servo.init(SERVO_PIN);
    if (servo_ok) {
        servo.setAngle(90, true);  // Center before starting
    }

    bool init_logged = false;

    // Measurement intervals: 60ms provides ~16.7 Hz sampling rate
    // - Optimal balance between reactivity and CPU load
    // - Compatible with HC-SR04 sensor timing (max 25ms measurement time)
    // - Provides good safety margin for obstacle detection
    const TickType_t forward_delay = pdMS_TO_TICKS(60);
    const TickType_t backup_step = pdMS_TO_TICKS(100);
    const TickType_t backup_total = pdMS_TO_TICKS(900);
    const TickType_t turn_window = pdMS_TO_TICKS(1500);
    const TickType_t rotate_recovery = pdMS_TO_TICKS(1500);

    // Track current rotation direction for scan recovery
    uint8_t current_rotate_cmd = CMD_ROTATE_CW;  // Default to CW

    while (1) {
        if (!mode_mgr.isAutonomousMode()) {
            init_logged = false;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if ((!front_ok || !rear_ok || !servo_ok) && !init_logged) {
            Serial.println("[AUTO] Sensor/servo init failed - staying stopped");
            init_logged = true;
            sendMotionCommand(CMD_STOP);
        }

        if (!front_ok || !rear_ok || !servo_ok) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Baseline forward step
        sendMotionCommand(CMD_FORWARD);
        vTaskDelay(forward_delay);

        // Read filtered distance (filtering applied automatically)
        float front_distance = front_sensor.readDistanceCM();
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
            float rear_distance = rear_sensor.readDistanceCM();
            if (rear_distance > 0.0f && rear_distance < 15.0f) {
                rear_blocked = true;
                break;
            }
        }
        sendMotionCommand(CMD_STOP);

        if (rear_blocked) {
            Serial.println("[AUTO] Rear obstacle detected during backup");
        }

        // Scan environment
        std::array<float, 3> distances = scanThreeDirections(servo, front_sensor);
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
            
            // Determine rotation direction based on largest distance
            // Left (0) -> CCW, Right (2) -> CW, Center (1) -> keep current direction
            uint8_t rotate_cmd;
            if (max_dir == 0) {
                // Left has largest distance -> rotate CCW
                rotate_cmd = CMD_ROTATE_CCW;
            } else if (max_dir == 2) {
                // Right has largest distance -> rotate CW
                rotate_cmd = CMD_ROTATE_CW;
            } else {
                // Center has largest distance or no valid direction -> continue current direction
                rotate_cmd = current_rotate_cmd;
            }
            
            // Update current rotation direction
            current_rotate_cmd = rotate_cmd;
            
            Serial.print("[SCAN] All directions blocked, continuing ");
            Serial.print((rotate_cmd == CMD_ROTATE_CW) ? "CW" : "CCW");
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
            
            sendMotionCommand(rotate_cmd);
            vTaskDelay(rotate_recovery);
            sendMotionCommand(CMD_STOP);

            distances = scanThreeDirections(servo, front_sensor);
            best_dir = pickBestDirection(distances, max_distance);
            recovery_attempts++;
        }

        if (max_distance <= 40.0f) {
            Serial.println("[AUTO] Stuck: no clear path after recovery attempts");
            sendMotionCommand(CMD_STOP);
            continue;
        }

        // Execute chosen direction
        if (best_dir == 0) {
            current_rotate_cmd = CMD_ROTATE_CCW;
            sendMotionCommand(CMD_ROTATE_CCW);
            vTaskDelay(turn_window);
            sendMotionCommand(CMD_STOP);
        } else if (best_dir == 2) {
            current_rotate_cmd = CMD_ROTATE_CW;
            sendMotionCommand(CMD_ROTATE_CW);
            vTaskDelay(turn_window);
            sendMotionCommand(CMD_STOP);
        } else {
            // Center is already clear; keep going on next loop iteration
        }
    }
}

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

extern volatile bool setupComplete;

namespace {

// Helper to push motion commands without blocking long
void sendMotionCommand(uint8_t cmd) {
    if (xCommandQueue != NULL) {
        xQueueSend(xCommandQueue, &cmd, 0);
    }
}

// Scan order: right(10°), center(90°), left(180°). Indices 0=right, 1=center, 2=left.
std::array<float, 3> scanThreeDirections(ServoDriver& servo) {
    const int angles[3] = {10, 90, 180};
    std::array<float, 3> distances = {-1.0f, -1.0f, -1.0f};
    constexpr int samples_per_direction = 20;
    const TickType_t sample_interval = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);
    const TickType_t stabilization_delay = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);

    for (int i = 0; i < 3; ++i) {
        servo.setAngle(angles[i], false);
        vTaskDelay(stabilization_delay);

        std::array<float, samples_per_direction> samples;
        samples.fill(-1.0f);
        for (int s = 0; s < samples_per_direction; ++s) {
            vTaskDelay(sample_interval);
            float raw = -1.0f;
            if (readFrontSensorRaw(&raw)) {
                samples[s] = raw;
            }
        }

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
        }
    }

    servo.setAngle(90, false);
    return distances;
}

// Pick index of longest clear direction (0=right, 1=center, 2=left).
int pickBestDirection(const std::array<float, 3>& distances, float& max_distance) {
    int best = -1;
    max_distance = -1.0f;

    for (int i = 0; i < 3; ++i) {
        if (distances[i] > 0.0f && distances[i] < 400.0f) {
            if (distances[i] > max_distance) {
                max_distance = distances[i];
                best = i;
            }
        }
    }

    if (best < 0) {
        best = 1;
        max_distance = -1.0f;
    }

    return best;
}

} // namespace

void task_autonomous(void *pvParameters) {
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ModeManager& mode_mgr = ModeManager::getInstance();

    ServoDriver servo;
    bool servo_ok = servo.init(SERVO_PIN);
    if (servo_ok) {
        servo.setAngle(90, true);
    }

    bool init_logged = false;
    bool active_logged = false;

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

        if (!active_logged) {
            active_logged = true;
            Serial.println("[AUTO] Autonomous active (servo ready, using SensorState every 60 ms)");
        }

        sendMotionCommand(CMD_FORWARD);
        vTaskDelay(forward_delay);

        SensorState sensor_state;
        if (!getSensorState(&sensor_state)) {
            continue;
        }

        float front_distance = sensor_state.front_distance;
        bool obstacleDetected = (front_distance > 0.0f && front_distance < 20.0f);
        if (!obstacleDetected) {
            continue;
        }

        // Obstacle: stop and backup
        sendMotionCommand(CMD_STOP);
        sendMotionCommand(CMD_BACKWARD);
        TickType_t backup_start = xTaskGetTickCount();
        bool rear_blocked = false;
        while ((xTaskGetTickCount() - backup_start) < backup_total) {
            vTaskDelay(backup_step);
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

        // Scan and pick direction
        std::array<float, 3> distances = scanThreeDirections(servo);
        float max_distance = 0.0f;
        int best_dir = pickBestDirection(distances, max_distance);

        // Stuck recovery
        int recovery_attempts = 0;
        while (max_distance <= 40.0f && recovery_attempts < 3) {
            int max_dir = -1;
            float max_dist = -1.0f;
            for (int i = 0; i < 3; ++i) {
                if (distances[i] > 0.0f && distances[i] < 400.0f && distances[i] > max_dist) {
                    max_dist = distances[i];
                    max_dir = i;
                }
            }

            uint8_t motion_cmd;
            if (max_dir == 0) {
                motion_cmd = CMD_ROTATE_CW;
                current_rotate_cmd = CMD_ROTATE_CW;
            } else if (max_dir == 2) {
                motion_cmd = CMD_ROTATE_CCW;
                current_rotate_cmd = CMD_ROTATE_CCW;
            } else if (max_dir == 1) {
                motion_cmd = CMD_FORWARD;
            } else {
                motion_cmd = current_rotate_cmd;
            }

            sendMotionCommand(motion_cmd);
            vTaskDelay(rotate_recovery);
            sendMotionCommand(CMD_STOP);

            distances = scanThreeDirections(servo);
            best_dir = pickBestDirection(distances, max_distance);
            recovery_attempts++;
        }

        if (max_distance <= 40.0f) {
            sendMotionCommand(CMD_STOP);
            continue;
        }

        // Execute direction
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
            // best_dir == 1: center, move forward on next iteration
        }
    }
}

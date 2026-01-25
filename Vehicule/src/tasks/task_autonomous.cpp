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
std::array<float, 3> scanThreeDirections(ServoDriver& servo, Ultrasonic& sensor) {
    const int angles[3] = {30, 90, 150};
    std::array<float, 3> distances = {-1.0f, -1.0f, -1.0f};

    for (int i = 0; i < 3; ++i) {
        servo.setAngle(angles[i]);
        vTaskDelay(pdMS_TO_TICKS(250));
        distances[i] = sensor.readDistanceCM();
    }

    // Return servo to center for the next cycle
    servo.setAngle(90);
    return distances;
}

// Pick index of the longest clear direction (0 = left, 1 = center, 2 = right)
int pickBestDirection(const std::array<float, 3>& distances, float& max_distance) {
    int best = 0;
    max_distance = distances[0];
    for (int i = 1; i < 3; ++i) {
        if (distances[i] > max_distance) {
            max_distance = distances[i];
            best = i;
        }
    }
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
    const TickType_t backup_step = pdMS_TO_TICKS(60);
    const TickType_t backup_total = pdMS_TO_TICKS(500);
    const TickType_t turn_window = pdMS_TO_TICKS(400);
    const TickType_t rotate_recovery = pdMS_TO_TICKS(500);

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
        int recovery_attempts = 0;
        while (max_distance <= 30.0f && recovery_attempts < 3) {
            sendMotionCommand(CMD_ROTATE_CCW);
            vTaskDelay(rotate_recovery);
            sendMotionCommand(CMD_STOP);

            distances = scanThreeDirections(servo, front_sensor);
            best_dir = pickBestDirection(distances, max_distance);
            recovery_attempts++;
        }

        if (max_distance <= 30.0f) {
            Serial.println("[AUTO] Stuck: no clear path after recovery attempts");
            sendMotionCommand(CMD_STOP);
            continue;
        }

        // Execute chosen direction
        if (best_dir == 0) {
            sendMotionCommand(CMD_ROTATE_CCW);
            vTaskDelay(turn_window);
            sendMotionCommand(CMD_STOP);
        } else if (best_dir == 2) {
            sendMotionCommand(CMD_ROTATE_CW);
            vTaskDelay(turn_window);
            sendMotionCommand(CMD_STOP);
        } else {
            // Center is already clear; keep going on next loop iteration
        }
    }
}

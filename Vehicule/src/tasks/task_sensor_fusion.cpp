/**
 * @file task_sensor_fusion.cpp
 * @brief Sensor fusion task - 50ms period, Priority 3
 * @details Reads sensors (ultrasonic, IMU) and fuses data
 *          Based on updateServoSensor() from Vehicule_Controller.ino
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config.h"
#include "../drivers/servo_driver.h"
#include "../drivers/ultrasonic_driver.h"
#include "../safety/emergency_stop.h"
#include "../control/motion_control.h"

// Servo and sensor instances
static ServoDriver servo;
static Ultrasonic ultrasonic;

// External flag from main.cpp indicating setup is complete
extern volatile bool setupComplete;

void task_sensor_fusion(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SENSOR_FUSION);  // 50ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_SENSOR] Sensor fusion task started");
    
    // Wait for setup to complete before initializing sensors
    Serial.println("[TASK_SENSOR] Waiting for setup to complete...");
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    Serial.println("[TASK_SENSOR] Setup complete, initializing sensors...");
    
    // Initialize ultrasonic sensor (matching Vehicule_Controller.ino setup)
    bool ultrasonic_ok = ultrasonic.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
    if (!ultrasonic_ok) {
        Serial.println("[SENSOR] Warning: Ultrasonic sensor init failed - continuing anyway");
    } else {
        Serial.println("[SENSOR] Ultrasonic sensor initialized");
    }
    
    // Initialize servo (matching Vehicule_Controller.ino: scanServo.attach(SERVO_PIN))
    bool servo_ok = servo.init(SERVO_PIN);
    if (!servo_ok) {
        Serial.println("[SENSOR] Warning: Servo init failed - continuing anyway");
    } else {
        Serial.println("[SENSOR] Servo initialized");
    }
    
    // Start servo sweep only if servo initialized successfully
    if (servo_ok) {
        servo.startSweep(0, 60, 5, 300);
        Serial.println("[SENSOR] Servo sweep started");
    }
    
    Serial.println("[SENSOR] Sensor fusion task ready");
    
    // Track previous obstacle state to detect transitions
    bool prev_obstacle = false;
    
    while (1) {
        // Update servo sweep (matching updateServoSensor() from Vehicule_Controller.ino)
        servo.update();
        
        // Read distance (matching: lastDistanceCm = readDistanceCM())
        float distance = ultrasonic.readDistanceCM();
        
        // Check for obstacles (matching Vehicule_Controller.ino logic)
        // if (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_DISTANCE_CM)
        bool obstacle_detected = ultrasonic.isObstacle(EMERGENCY_STOP_DISTANCE_CM);
        
        // Only log and change state when obstacle status changes
        if (obstacle_detected && !prev_obstacle) {
            // Obstacle just detected
            Serial.println("⚠️ Object detected close!");
            Serial.print("[SENSOR] Distance: ");
            Serial.print(distance);
            Serial.println(" cm - TRIGGERING EMERGENCY STOP");
            motion_stop();             // Immediately stop motors
            emergency_stop_trigger();  // Trigger emergency stop
            prev_obstacle = true;
        } else if (!obstacle_detected && prev_obstacle) {
            // Obstacle just cleared
            Serial.println("✓ Obstacle cleared - Emergency stop auto-cleared");
            emergency_stop_clear();    // Clear emergency stop
            prev_obstacle = false;
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

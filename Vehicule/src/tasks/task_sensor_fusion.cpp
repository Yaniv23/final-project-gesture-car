/**
 * @file task_sensor_fusion.cpp
 * @brief Sensor fusion task - 50ms period, Priority 3
 * @details Reads sensors (ultrasonic, IMU) and fuses data
 *          Based on updateServoSensor() from Vehicule_Controller.ino
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "../config.h"
#include "../drivers/servo_driver.h"
#include "../drivers/ultrasonic_driver.h"
#include "../safety/emergency_stop.h"

// Servo and sensor instances
static ServoDriver servo;
static Ultrasonic ultrasonic;

void task_sensor_fusion(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SENSOR_FUSION);  // 50ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("[TASK_SENSOR] Sensor fusion task started");
    
    // Initialize ultrasonic sensor (matching Vehicule_Controller.ino setup)
    if (!ultrasonic.init(ULTRASONIC_TRIG, ULTRASONIC_ECHO)) {
        Serial.println("[ERROR] Ultrasonic sensor init failed!");
    }
    
    // Initialize servo (matching Vehicule_Controller.ino: scanServo.attach(SERVO_PIN))
    if (!servo.init(SERVO_PIN)) {
        Serial.println("[ERROR] Servo init failed!");
    }
    
    // Start servo sweep (matching Vehicule_Controller.ino default values)
    // SERVO_MIN_ANGLE = 0, SERVO_MAX_ANGLE = 60, SERVO_STEP_DEG = 5, SERVO_STEP_INTERVAL_MS = 300
    servo.startSweep(0, 60, 5, 300);
    
    while (1) {
        // Update servo sweep (matching updateServoSensor() from Vehicule_Controller.ino)
        servo.update();
        
        // Read distance (matching: lastDistanceCm = readDistanceCM())
        float distance = ultrasonic.readDistanceCM();
        
        // Check for obstacles (matching Vehicule_Controller.ino logic)
        // if (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_DISTANCE_CM)
        if (ultrasonic.isObstacle(EMERGENCY_STOP_DISTANCE_CM)) {
            Serial.println("⚠️ Object detected close!");
            emergency_stop_trigger();  // Trigger emergency stop
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

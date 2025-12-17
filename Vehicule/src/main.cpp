/**
 * @file main.cpp
 * @brief Main entry point for ESP32 Vehicle Controller
 * @details FreeRTOS-based multi-task system for gesture-controlled mecanum car
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

// Configuration
#include "config.h"

// HAL Layer
#include "hal/hal_gpio.h"
#include "hal/hal_pwm.h"
#include "hal/hal_timer.h"

// Forward declarations (will be implemented in later tasks)
void task_motor_control(void *pvParameters);
void task_communication(void *pvParameters);
void task_sensor_fusion(void *pvParameters);
void task_safety_monitor(void *pvParameters);
void task_telemetry(void *pvParameters);

/**
 * @brief Initialize HAL layer for motor control pins
 */
void initHAL_Motors(void) {
    Serial.println("[HAL] Initializing motor control pins...");
    
    // Initialize PWM pins for motor speed control
    HAL_PWM_Init(FRONT_RIGHT_ENA, 500, 8);  // 500Hz, 8-bit (0-255)
    HAL_PWM_Init(BACK_RIGHT_ENA, 500, 8);
    // Note: FRONT_LEFT_ENA and BACK_LEFT_ENA share channels with FR and BR
    
    // Initialize GPIO pins for motor direction control
    HAL_GPIO_Init(FRONT_RIGHT_IN1, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(FRONT_RIGHT_IN2, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(FRONT_LEFT_IN3, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(FRONT_LEFT_IN4, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(BACK_RIGHT_IN1, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(BACK_RIGHT_IN2, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(BACK_LEFT_IN3, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(BACK_LEFT_IN4, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    
    // Initialize all motors to stopped state
    HAL_PWM_Stop(FRONT_RIGHT_ENA);
    HAL_PWM_Stop(BACK_RIGHT_ENA);
    HAL_GPIO_Write(FRONT_RIGHT_IN1, HAL_GPIO_LOW);
    HAL_GPIO_Write(FRONT_RIGHT_IN2, HAL_GPIO_LOW);
    HAL_GPIO_Write(FRONT_LEFT_IN3, HAL_GPIO_LOW);
    HAL_GPIO_Write(FRONT_LEFT_IN4, HAL_GPIO_LOW);
    HAL_GPIO_Write(BACK_RIGHT_IN1, HAL_GPIO_LOW);
    HAL_GPIO_Write(BACK_RIGHT_IN2, HAL_GPIO_LOW);
    HAL_GPIO_Write(BACK_LEFT_IN3, HAL_GPIO_LOW);
    HAL_GPIO_Write(BACK_LEFT_IN4, HAL_GPIO_LOW);
    
    Serial.println("[HAL] Motor control pins initialized");
}

/**
 * @brief Initialize HAL layer for sensors
 */
void initHAL_Sensors(void) {
    Serial.println("[HAL] Initializing sensor pins...");
    
    // Ultrasonic sensor pins
    HAL_GPIO_Init(ULTRASONIC_TRIG, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(ULTRASONIC_ECHO, HAL_GPIO_INPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Write(ULTRASONIC_TRIG, HAL_GPIO_LOW);
    
    // Servo PWM (will be initialized by servo driver)
    // HAL_PWM_Init(SERVO_PIN, 50, 8);  // 50Hz for servo
    
    Serial.println("[HAL] Sensor pins initialized");
}

/**
 * @brief Initialize watchdog timer
 */
void initHAL_Watchdog(void) {
    Serial.println("[HAL] Initializing watchdog timer...");
    
    HAL_Timer_WatchdogConfig wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT_MS,
        .enable = true
    };
    
    if (HAL_Timer_WatchdogInit(&wdt_config)) {
        Serial.println("[HAL] Watchdog timer initialized (" + String(WATCHDOG_TIMEOUT_MS) + " ms)");
    } else {
        Serial.println("[HAL] ⚠️ Watchdog timer initialization failed");
    }
}

void setup() {
    // Initialize Serial for debugging
    Serial.begin(SERIAL_BAUD_RATE);
    HAL_Timer_DelayMs(1000);  // Wait for Serial Monitor to connect
    
    Serial.println("\n========================================");
    Serial.println("Gesture Car - ESP32 Vehicle Controller");
    Serial.println("Phase 1: Infrastructure Setup");
    Serial.println("========================================");
    Serial.println("FreeRTOS Version: " + String(tskKERNEL_VERSION_NUMBER));
    Serial.println("CPU Frequency: " + String(getCpuFrequencyMhz()) + " MHz");
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("========================================\n");
    
    // Task 1.2 - Initialize HAL layer
    initHAL_Motors();
    initHAL_Sensors();
    initHAL_Watchdog();
    
    // TODO: Task 1.3 - Initialize MotorDriver
    // TODO: Task 1.4 - Create FreeRTOS tasks
    
    Serial.println("[SETUP] HAL layer initialized");
    Serial.println("[SETUP] Ready for MotorDriver implementation (Task 1.3)\n");
}

void loop() {
    // Empty - FreeRTOS tasks handle everything
    // In a FreeRTOS setup, loop() should not contain blocking code
    // All work is done in tasks created in setup()
    
    // Feed watchdog timer (safety mechanism)
    HAL_Timer_WatchdogFeed();
    
    // This delay ensures loop() doesn't consume CPU
    // In production, you might remove loop() entirely or use it for
    // low-priority background tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Optional: Print free heap periodically for debugging
    static uint32_t lastPrint = 0;
    uint32_t now = HAL_Timer_GetMillis();
    if (now - lastPrint > 5000) {
        Serial.println("[LOOP] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
        lastPrint = now;
    }
}


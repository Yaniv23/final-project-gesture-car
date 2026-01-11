/**
 * @file main.cpp
 * @brief Main entry point for ESP32 Vehicle Controller
 * @details FreeRTOS-based multi-task system for gesture-controlled mecanum car
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

// Configuration
#include "config.h"

// Drivers
#include "drivers/motor_driver.h"

// Shared resources
#include "shared/queues.h"

// Safety
#include "safety/watchdog.h"
#include "safety/timeout_monitor.h"

// Communication
#include "communication/command_protocol.h"
#include "communication/espnow_handler.h"

// Control
#include "control/mode_manager.h"

// Task implementations (forward declarations)
void task_motor_control(void *pvParameters);
void task_communication(void *pvParameters);
void task_safety_monitor(void *pvParameters);
void task_autonomous(void *pvParameters);

// Global motor driver instance
MotorDriver motor_driver;

// Task handles for suspending/resuming tasks
TaskHandle_t taskHandle_safety = NULL;
TaskHandle_t taskHandle_motor = NULL;
TaskHandle_t taskHandle_comm = NULL;
TaskHandle_t taskHandle_autonomous = NULL;

// Global flag to signal tasks that setup is complete
volatile bool setupComplete = false;

void setup() {
    // Initialize Serial for debugging
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);  // Wait for Serial Monitor to connect
    
    Serial.println("\n========================================");
    Serial.println("Gesture Car - ESP32 Vehicle Controller");
    Serial.println("Phase 1: Infrastructure Setup");
    Serial.println(">>> RUNNING IN NORMAL MODE <<<");
    Serial.println(">>> PRODUCTION MODE <<<");
    
    Serial.println("[SETUP] Initializing WiFi and ESP-NOW...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(200);  // Give WiFi time to initialize
    
    // Print MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    Serial.print("[SETUP] MAC Address: ");
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println(macStr);
    
    // Initialize ESP-NOW
    if (espnow_init(false)) {
        Serial.println("[SETUP] ESP-NOW initialized successfully");
        
        // Wait for connection from sender before continuing
        Serial.println("[SETUP] Waiting for ESP-NOW connection from sender...");
        if (!espnow_wait_for_connection(ESP_NOW_CONNECTION_TIMEOUT_MS)) {
            Serial.println("[ERROR] ESP-NOW connection timeout!");
            Serial.println("[ERROR] No message received from sender within timeout period");
            while (1) delay(1000);  // Halt on error
        }
        Serial.println("[SETUP] ✓ ESP-NOW connection established");
    } else {
        Serial.println("[ERROR] ESP-NOW initialization failed!");
        while (1) delay(1000);  // Halt on error
    }
    Serial.println();
    
    // Initialize shared queues and semaphores
    Serial.println("[SETUP] Initializing shared queues...");
    if (!initSharedQueues()) {
        Serial.println("[ERROR] Failed to initialize shared queues!");
        while (1) delay(1000);  // Halt on error
    }
    Serial.println("[SETUP] Shared queues initialized");
    
    // Initialize MotorDriver with individual enable pins for each motor
    Serial.println("[SETUP] Initializing MotorDriver...");
    MotorDriver::MotorConfig motor_configs[4] = {
        // Front Left Motor (direction pins + enable pin)
        {FRONT_LEFT_IN1, FRONT_LEFT_IN2, FRONT_LEFT_EN},
        // Front Right Motor
        {FRONT_RIGHT_IN1, FRONT_RIGHT_IN2, FRONT_RIGHT_EN},
        // Back Left Motor
        {BACK_LEFT_IN1, BACK_LEFT_IN2, BACK_LEFT_EN},
        // Back Right Motor
        {BACK_RIGHT_IN1, BACK_RIGHT_IN2, BACK_RIGHT_EN}
    };
    
    // Initialize with individual enable pins (each motor has its own PWM pin)
    if (!motor_driver.init(motor_configs)) {
        Serial.println("[ERROR] Failed to initialize MotorDriver!");
        while (1) delay(1000);  // Halt on error
    }
    
    // Ensure all motors are stopped initially (safety measure)
    motor_driver.stopAll();
    
    // Initialize safety systems
    Serial.println("[SETUP] Initializing safety systems...");
    watchdog_init(WATCHDOG_TIMEOUT_MS);
    timeout_monitor_init(COMMAND_TIMEOUT_MS);
    
    // Create FreeRTOS tasks
    Serial.println("========================================");
    Serial.println("[SETUP] Creating FreeRTOS tasks...");
    Serial.println("========================================");
    
    // Task 1: Safety Monitor (Highest Priority - 5)
    xTaskCreate(
        task_safety_monitor, // Function to execute
        "SafetyMonitor", // Task name
        TASK_STACK_SIZE_SAFETY_MONITOR, // Stack size
        NULL, // Task parameters
        TASK_PRIORITY_SAFETY_MONITOR, // Priority
        &taskHandle_safety // Task handle
    );
    Serial.println("[SETUP] Created task: SafetyMonitor (Priority 5)");
    
    // Task 2: Motor Control (Priority 4)
    xTaskCreate(
        task_motor_control,
        "MotorControl",
        TASK_STACK_SIZE_MOTOR_CONTROL,
        NULL,
        TASK_PRIORITY_MOTOR_CONTROL,
        &taskHandle_motor
    );
    Serial.println("[SETUP] Created task: MotorControl (Priority 4)");
    
    // Task 3: Autonomous (Priority 3)
    xTaskCreate(
        task_autonomous,
        "Autonomous",
        AUTONOMOUS_TASK_STACK_SIZE,
        NULL,
        AUTONOMOUS_TASK_PRIORITY,
        &taskHandle_autonomous
    );
    Serial.println("[SETUP] Created task: Autonomous (Priority 3)");
    
    // Task 5: Communication (Priority 2)
    xTaskCreate(
        task_communication,
        "Communication",
        TASK_STACK_SIZE_COMMUNICATION,
        NULL,
        TASK_PRIORITY_COMMUNICATION,
        &taskHandle_comm
    );
    Serial.println("[SETUP] Created task: Communication (Priority 2)");
    
    Serial.println("========================================");
    Serial.println("[SETUP] All tasks created successfully!");
    Serial.println("========================================");
    
    // Signal all tasks that setup is complete
    setupComplete = true;
    
    // Give tasks a moment to start
    delay(100);
    
    Serial.println("[SETUP] System ready 🎉- FreeRTOS scheduler running!\n");
}

void loop() {
    // Empty - FreeRTOS tasks handle everything
    // In a FreeRTOS setup, loop() should not contain blocking code
    // All work is done in tasks created in setup()
    
    // Feed watchdog timer (safety mechanism)
    watchdog_feed();
    
    // This delay ensures loop() doesn't consume CPU
    // In production, you might remove loop() entirely or use it for
    // low-priority background tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
    
}

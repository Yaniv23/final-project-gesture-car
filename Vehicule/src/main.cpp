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
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"  // For brownout detection disable

// LED_BUILTIN is GPIO 2 on most ESP32 dev boards
// Define it explicitly in case it's not defined
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

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
void task_autonomous(void *pvParameters);
void task_sensors(void *pvParameters);
// REMOVED: void task_telemetry(void *pvParameters); - Telemetry disabled to fix command reception
// REMOVED: void task_safety_monitor(void *pvParameters); - Safety monitor task not used

// Global motor driver instance
MotorDriver motor_driver;

// Task handles for suspending/resuming tasks
TaskHandle_t taskHandle_motor = NULL;
TaskHandle_t taskHandle_comm = NULL;
TaskHandle_t taskHandle_autonomous = NULL;
TaskHandle_t taskHandle_sensors = NULL;
// REMOVED: TaskHandle_t taskHandle_telemetry = NULL; - Telemetry disabled
// REMOVED: TaskHandle_t taskHandle_safety = NULL; - Safety monitor task not used

// Global flag to signal tasks that setup is complete
volatile bool setupComplete = false;

void setup() {
    // =========================================================================
    // CRITICAL: Disable brownout detector for battery power operation
    // The ESP32 brownout detector can trigger false resets when:
    // - Powered via VIN from battery (voltage drops during WiFi TX)
    // - Current spikes occur during WiFi initialization (300-400mA)
    // =========================================================================
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Initialize Serial for debugging
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Setup built-in LED for visual feedback when not connected to USB
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);  // LED ON = booting
    
    // CRITICAL: Extended delay for battery power stabilization
    // When powered from battery, the ESP32 needs time for:
    // - Voltage regulators to stabilize
    // - Capacitors to charge
    // - Power supply to handle WiFi current spikes
    delay(3000);  // 3 seconds for power stabilization (increased from 2s)
    
    Serial.println("\n========================================");
    Serial.println("Gesture Car - ESP32 Vehicle Controller");
    Serial.println("========================================");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // CRITICAL for battery operation: Disable WiFi sleep mode early
    // This prevents connection issues and ensures reliable ESP-NOW
    WiFi.setSleep(false);
    
    delay(1500);  // Increased delay for WiFi stability on battery power (was 1000ms)
    
    // Additional WiFi power configuration for battery operation
    // Set WiFi to max power to ensure reliable ESP-NOW communication
    esp_wifi_set_max_tx_power(78);  // Max TX power (19.5 dBm)
    Serial.println("[SETUP] WiFi TX power set to maximum (19.5 dBm)");
    
    // Initialize ESP-NOW
    if (espnow_init(false)) {
        // Visual feedback: 3 quick blinks = ESP-NOW ready, waiting for connection
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
        }
    } else {
        Serial.println("[ERROR] ESP-NOW initialization failed!");
        // Visual feedback: rapid blinking = error
        while (1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
    }
    Serial.println();
    
    // Initialize shared queues, semaphores, and SensorState (used by Sensors + Autonomous tasks)
    Serial.println("[SETUP] Initializing shared queues and SensorState...");
    if (!initSharedQueues()) {
        Serial.println("[ERROR] Failed to initialize shared queues / SensorState!");
        while (1) delay(1000);  // Halt on error
    }
    Serial.println("[SETUP] Shared queues and SensorState initialized");
    
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
    
    // REMOVED: Safety Monitor task - not used (watchdog fed in loop())
    
    // Task 1: Motor Control (Priority 4)
    xTaskCreate(
        task_motor_control,
        "MotorControl",
        TASK_STACK_SIZE_MOTOR_CONTROL,
        NULL,
        TASK_PRIORITY_MOTOR_CONTROL,
        &taskHandle_motor
    );
    Serial.println("[SETUP] Created task: MotorControl (Priority 4)");
    
    // Task 2: Sensors (Priority 3)
    xTaskCreate(
        task_sensors,
        "Sensors",
        TASK_STACK_SIZE_SENSORS,
        NULL,
        TASK_PRIORITY_SENSORS,
        &taskHandle_sensors
    );
    Serial.println("[SETUP] Created task: Sensors (Priority 3)");
    
    // Task 3: Autonomous (Priority 3)
    xTaskCreate(
        task_autonomous,
        "Autonomous",
        AUTONOMOUS_TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY_AUTONOMOUS,
        &taskHandle_autonomous
    );
    Serial.println("[SETUP] Created task: Autonomous (Priority 3)");
    
    // Register task handles with ModeManager so it can suspend/resume Sensors and Autonomous on mode change
    ModeManager& mode_mgr = ModeManager::getInstance();
    mode_mgr.registerTaskHandles(taskHandle_autonomous, taskHandle_sensors);

    // Set default mode to AUTONOMOUS for testing
    // ModeManager will keep Sensors and Autonomous tasks running (no suspend on first setMode)
    mode_mgr.setMode(MODE_AUTONOMOUS);
    Serial.println("[SETUP] Default mode: AUTONOMOUS (for testing)");
    
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
    
    // Signal all tasks that setup is complete
    setupComplete = true;
    
    // Give tasks a moment to start
    delay(100);
    
    Serial.println("[SETUP] System ready - FreeRTOS scheduler running!\n");
}

// LED state tracking for connection indicator
static uint32_t led_last_toggle = 0;
static bool led_state = HIGH;
static const uint32_t LED_BLINK_INTERVAL_MS = 500;  // Blink every 500ms when waiting for connection

void loop() {
    // Feed watchdog timer (safety mechanism)
    watchdog_feed();
    
    // Connection status LED indicator
    // - Blinking (500ms): Waiting for sender connection
    // - Solid ON: Connected to sender
    // - Solid OFF: Error state (should not happen in normal operation)
    
    bool is_connected = espnow_is_connected();
    uint32_t now = millis();
    
    if (!is_connected) {
        // Not connected: Blink LED to indicate waiting for connection
        if (now - led_last_toggle >= LED_BLINK_INTERVAL_MS) {
            led_state = !led_state;
            digitalWrite(LED_BUILTIN, led_state);
            led_last_toggle = now;
        }
    } else {
        // Connected: LED solid ON
        if (led_state != HIGH) {
            led_state = HIGH;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
    
    // Small delay to prevent tight loop (50ms = 20 Hz update rate)
    vTaskDelay(pdMS_TO_TICKS(50));
}

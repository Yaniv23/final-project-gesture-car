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

// Task implementations (forward declarations)
void task_motor_control(void *pvParameters);
void task_communication(void *pvParameters);
void task_sensor_fusion(void *pvParameters);
void task_safety_monitor(void *pvParameters);
void task_telemetry(void *pvParameters);

// Test function (forward declaration)
void test_command_reception();
void test_motor_led_actuation();
void task_test_commands(void *pvParameters);

// Global motor driver instance
MotorDriver motor_driver;

// Task handles for suspending/resuming tasks
TaskHandle_t taskHandle_safety = NULL;
TaskHandle_t taskHandle_motor = NULL;
TaskHandle_t taskHandle_sensor = NULL;
TaskHandle_t taskHandle_comm = NULL;
TaskHandle_t taskHandle_telemetry = NULL;
TaskHandle_t taskHandle_test = NULL;

// Global flag to signal tasks that setup is complete
volatile bool setupComplete = false;

void setup() {
    // Initialize Serial for debugging
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);  // Wait for Serial Monitor to connect
    
    Serial.println("\n========================================");
    Serial.println("Gesture Car - ESP32 Vehicle Controller");
    Serial.println("Phase 1: Infrastructure Setup");
    #if SIMULATION_MODE
    Serial.println(">>> RUNNING IN SIMULATION MODE <<<");
    Serial.println(">>> TEST MODE ENABLED <<<");
    #else
    Serial.println(">>> RUNNING IN NORMAL MODE <<<");
    Serial.println(">>> PRODUCTION MODE <<<");
    #endif

    
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
    
    // Initialize ESP-NOW (honor SIMULATION_MODE flag)
    if (espnow_init(SIMULATION_MODE)) {
        Serial.println("[SETUP] ESP-NOW initialized successfully");
        
        #if !SIMULATION_MODE
        // Wait for connection from sender before continuing
        Serial.println("[SETUP] Waiting for ESP-NOW connection from sender...");
        if (!espnow_wait_for_connection(ESP_NOW_CONNECTION_TIMEOUT_MS)) {
            Serial.println("[ERROR] ESP-NOW connection timeout!");
            Serial.println("[ERROR] No message received from sender within timeout period");
            while (1) delay(1000);  // Halt on error
        }
        Serial.println("[SETUP] ✓ ESP-NOW connection established");
        #else
        Serial.println("[SETUP] SIMULATION MODE - skipping connection wait");
        #endif
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
        &taskHandle_safety, // Task handle
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
    
    // Task 3: Sensor Fusion (Priority 3)
    xTaskCreate(
        task_sensor_fusion,
        "SensorFusion",
        TASK_STACK_SIZE_SENSOR_FUSION,
        NULL,
        TASK_PRIORITY_SENSOR_FUSION,
        &taskHandle_sensor
    );
    Serial.println("[SETUP] Created task: SensorFusion (Priority 3)");
    
    #if !SIMULATION_MODE
    // Task 4: Communication (Priority 2) - Only in normal mode
    xTaskCreate(
        task_communication,
        "Communication",
        TASK_STACK_SIZE_COMMUNICATION,
        NULL,
        TASK_PRIORITY_COMMUNICATION,
        &taskHandle_comm
    );
    Serial.println("[SETUP] Created task: Communication (Priority 2)");
    #else
    Serial.println("[SETUP] Communication task disabled in SIMULATION_MODE");
    #endif
    
    // Task 5: Telemetry (Lowest Priority - 1)
    xTaskCreate(
        task_telemetry,
        "Telemetry",
        TASK_STACK_SIZE_TELEMETRY,
        NULL,
        TASK_PRIORITY_TELEMETRY,
        &taskHandle_telemetry
    );
    Serial.println("[SETUP] Created task: Telemetry (Priority 1)");
    
    #if SIMULATION_MODE
    // Task 6: Test Commands (Priority 1) - Only in simulation mode
    xTaskCreate(
        task_test_commands,
        "TestCommands",
        4096,
        NULL,
        1,  // Same as telemetry - will run after 2 second delay
        &taskHandle_test
    );
    Serial.println("[SETUP] Created task: TestCommands (Priority 1) - SIMULATION MODE");
    #endif
    
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

// ============================================================================
// TEST TASK: Command Reception Testing
// ============================================================================

void task_test_commands(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    test_motor_led_actuation();
    vTaskDelete(NULL);
}


// ============================================================================
// TEST SUITE: Command Queue Communication Test
// ============================================================================

void test_motor_led_actuation() {
    Serial.println("[TEST] Starting command queue test");
    
    struct CommandTest {
        uint8_t cmd_byte;
        const char* cmd_name;
    };
    
    CommandTest tests[] = {
        {CMD_STOP, "STOP"},
        {CMD_FORWARD, "FORWARD"},
        {CMD_BACKWARD, "BACKWARD"},
        {CMD_SIDEWAY_LEFT, "SIDEWAY_LEFT"},
        {CMD_SIDEWAY_RIGHT, "SIDEWAY_RIGHT"},
        {CMD_ROTATE_CW, "ROTATE_CW"},
        {CMD_ROTATE_CCW, "ROTATE_CCW"},
        {CMD_DIAGONAL_315, "DIAGONAL_315"},
        {CMD_DIAGONAL_45, "DIAGONAL_45"},
        {CMD_DIAGONAL_225, "DIAGONAL_225"},
        {CMD_DIAGONAL_135, "DIAGONAL_135"},
        {CMD_PIVOT_LEFT, "PIVOT_LEFT"},
        {CMD_PIVOT_RIGHT, "PIVOT_RIGHT"}
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        if (xQueueSend(xCommandQueue, &tests[i].cmd_byte, pdMS_TO_TICKS(100)) == pdPASS) {
            Serial.print(".");
            passed++;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (i < num_tests - 1) {
            uint8_t stop_cmd = CMD_STOP;
            xQueueSend(xCommandQueue, &stop_cmd, pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    
    Serial.println();
    Serial.print("[TEST] Commands sent: ");
    Serial.print(passed);
    Serial.print("/");
    Serial.println(num_tests);
}

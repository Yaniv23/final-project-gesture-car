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
#include "safety/emergency_stop.h"

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
    Serial.println("========================================");
    Serial.println("FreeRTOS Version: " + String(tskKERNEL_VERSION_NUMBER));
    Serial.println("CPU Frequency: " + String(getCpuFrequencyMhz()) + " MHz");
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("========================================\n");
    
    // Initialize WiFi and ESP-NOW for communication
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
    if (espnow_init()) {
        Serial.println("[SETUP] ESP-NOW initialized successfully");
        Serial.println("[SETUP] Receiver ready - waiting for sender connection...");
        Serial.println("[SETUP] Connection status: Ready to receive ESP-NOW messages");
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
    
    // Initialize MotorDriver with common PWM pin
    Serial.println("[SETUP] Initializing MotorDriver...");
    MotorDriver::MotorConfig motor_configs[4] = {
        // Front Left Motor (direction pins only)
        {FRONT_LEFT_IN3, FRONT_LEFT_IN4},
        // Front Right Motor
        {FRONT_RIGHT_IN1, FRONT_RIGHT_IN2},
        // Back Left Motor
        {BACK_LEFT_IN3, BACK_LEFT_IN4},
        // Back Right Motor
        {BACK_RIGHT_IN1, BACK_RIGHT_IN2}
    };
    
    // Initialize with common PWM pin (pin 34) and LEDC channel 0
    if (!motor_driver.init(motor_configs, MOTOR_PWM_COMMON, 0)) {
        Serial.println("[ERROR] Failed to initialize MotorDriver!");
        while (1) delay(1000);  // Halt on error
    }
    Serial.println("[SETUP] MotorDriver initialized (common PWM on pin " + String(MOTOR_PWM_COMMON) + ")");
    
    // Ensure all motors are stopped initially (safety measure)
    motor_driver.stopAll();
    Serial.println("[SETUP] All motors set to STOP state (safety)");
    
    // Initialize safety systems
    Serial.println("[SETUP] Initializing safety systems...");
    watchdog_init(WATCHDOG_TIMEOUT_MS);
    timeout_monitor_init(COMMAND_TIMEOUT_MS);
    emergency_stop_init();
    Serial.println("[SETUP] Safety systems initialized");
    
    // Create FreeRTOS tasks
    Serial.println("[SETUP] Creating FreeRTOS tasks...");
    
    // Task 1: Safety Monitor (Highest Priority - 5)
    xTaskCreate(
        task_safety_monitor,
        "SafetyMonitor",
        TASK_STACK_SIZE_SAFETY_MONITOR,
        NULL,
        TASK_PRIORITY_SAFETY_MONITOR,
        &taskHandle_safety
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
    
    Serial.println("\n[SETUP] All tasks created successfully!");
    Serial.println("[SETUP] Signaling tasks that setup is complete...");
    
    // Signal all tasks that setup is complete
    setupComplete = true;
    
    // Give tasks a moment to start
    delay(100);
    
    Serial.println("[SETUP] System ready - FreeRTOS scheduler running!\n");
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
    
    // Optional: Print free heap periodically for debugging
    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint > 5000) {
        Serial.println("[LOOP] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
        lastPrint = now;
    }
}

// ============================================================================
// TEST TASK: Command Reception Testing
// ============================================================================

void task_test_commands(void *pvParameters) {
    // Give other tasks time to start up (2 seconds)
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║  AUTOMATED TEST SUITE STARTING         ║");
    Serial.println("║  Simulation Mode Active                ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    Serial.println("[TEST] Waiting 2 seconds for other tasks to initialize...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Run the motor command queue test
    test_motor_led_actuation();
    
    // Delete this task after testing is complete
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║  ALL TESTS COMPLETED                   ║");
    Serial.println("║  Test Task Terminating                 ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    Serial.println("[INFO] System will continue running in simulation mode");
    Serial.println("[INFO] Set SIMULATION_MODE=0 in config.h for production use\n");
    vTaskDelete(NULL);
}


// ============================================================================
// TEST SUITE: Command Queue Communication Test
// ============================================================================

void test_motor_led_actuation() {
    Serial.println("\n========================================");
    Serial.println("TEST SUITE: Command Queue Communication");
    Serial.println("========================================");
    Serial.println("Testing command send through queue\n");
    
    // Test commands with motor movement descriptions
    struct CommandTest {
        uint8_t cmd_byte;
        const char* cmd_name;
        const char* motor_pattern;  // Motor movement pattern
    };
    
    CommandTest tests[] = {
        {CMD_STOP, "STOP", "Motors: STOP (All Stopped)"},
        {CMD_FORWARD, "FORWARD", "Motors: FRF, FLF, BRF, BLF (All Forward)"},
        {CMD_BACKWARD, "BACKWARD", "Motors: FRB, FLB, BRB, BLB (All Backward)"},
        {CMD_SIDEWAY_LEFT, "SIDEWAY_LEFT", "Motors: FRF, FLB, BRB, BLF (Left strafe)"},
        {CMD_SIDEWAY_RIGHT, "SIDEWAY_RIGHT", "Motors: FRB, FLF, BRF, BLB (Right strafe)"},
        {CMD_ROTATE_CW, "ROTATE_CW", "Motors: FRB, FLF, BRB, BLF (Clockwise spin)"},
        {CMD_ROTATE_CCW, "ROTATE_CCW", "Motors: FRF, FLB, BRF, BLB (Counter-clockwise spin)"},
        {CMD_DIAGONAL_315, "DIAGONAL_315", "Motors: FRF, BLF (Forward-left diagonal)"},
        {CMD_DIAGONAL_45, "DIAGONAL_45", "Motors: FLF, BRF (Forward-right diagonal)"},
        {CMD_DIAGONAL_225, "DIAGONAL_225", "Motors: FRB, BLB (Backward-left diagonal)"},
        {CMD_DIAGONAL_135, "DIAGONAL_135", "Motors: FLB, BRB (Backward-right diagonal)"},
        {CMD_PIVOT_LEFT, "PIVOT_LEFT", "Motors: FRF, FLB, BR stop, BL stop (Pivot on left)"},
        {CMD_PIVOT_RIGHT, "PIVOT_RIGHT", "Motors: FRB, FLF, BR stop, BL stop (Pivot on right)"}
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    Serial.println("Sending commands to queue:\n");
    Serial.println("Legend: F=Front, B=Back, R=Right, L=Left");
    Serial.println("        F=Forward, B=Backward\n");
    
    for (int i = 0; i < num_tests; i++) {
        Serial.println("----------------------------------------");
        Serial.print("[TEST] Command ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.print(num_tests);
        Serial.print(": ");
        Serial.println(tests[i].cmd_name);
        Serial.print("Motor Pattern: ");
        Serial.println(tests[i].motor_pattern);
        Serial.print("Command: 0x");
        if (tests[i].cmd_byte < 0x10) Serial.print("0");
        Serial.print(tests[i].cmd_byte, HEX);
        Serial.print(" -> ");
        
        // Send command directly to queue without pre-validation
        if (xQueueSend(xCommandQueue, &tests[i].cmd_byte, pdMS_TO_TICKS(100)) == pdPASS) {
            Serial.println("SENT ✓");
            passed++;
        } else {
            Serial.println("FAILED ✗");
            failed++;
        }
        
        // Wait for motor control task to process and observe LED changes
        Serial.println("Observing motor response for 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        Serial.println();
        
        // Send STOP command between tests to ensure clean state
        if (i < num_tests - 1) {
            Serial.println("[TEST] Sending STOP between tests...");
            uint8_t stop_cmd = CMD_STOP;
            xQueueSend(xCommandQueue, &stop_cmd, pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(500));  // Brief pause for stop to take effect
        }
    }
    
    // Print summary
    Serial.println("========================================");
    Serial.println("COMMAND QUEUE TEST SUMMARY");
    Serial.println("========================================");
    Serial.print("Total Commands Sent: ");
    Serial.println(passed + failed);
    Serial.print("Successfully Sent: ");
    Serial.println(passed);
    Serial.print("Failed: ");
    Serial.println(failed);
    
    if (failed == 0) {
        Serial.println("\n✓ ALL COMMANDS SENT SUCCESSFULLY");
    } else {
        Serial.println("\n✗ SOME COMMANDS FAILED TO SEND");
    }
    Serial.println("========================================\n");
    Serial.println("Note: Validation of commands is performed");
    Serial.println("by the Motor Control Task, not in this test.\n");
}

/**
 * @file main.cpp
 * @brief Main entry point for ESP32 Vehicle Controller
 * @details FreeRTOS-based multi-task system for gesture-controlled mecanum car
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

// Task implementations (forward declarations)
void task_motor_control(void *pvParameters);
void task_communication(void *pvParameters);
void task_sensor_fusion(void *pvParameters);
void task_safety_monitor(void *pvParameters);
void task_telemetry(void *pvParameters);

// Test function (forward declaration)
void test_command_reception();

// Global motor driver instance
MotorDriver motor_driver;

void setup() {
    // Initialize Serial for debugging
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);  // Wait for Serial Monitor to connect
    
    Serial.println("\n========================================");
    Serial.println("Gesture Car - ESP32 Vehicle Controller");
    Serial.println("Phase 1: Infrastructure Setup");
    Serial.println("========================================");
    Serial.println("FreeRTOS Version: " + String(tskKERNEL_VERSION_NUMBER));
    Serial.println("CPU Frequency: " + String(getCpuFrequencyMhz()) + " MHz");
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("========================================\n");
    
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
        NULL
    );
    Serial.println("[SETUP] Created task: SafetyMonitor (Priority 5)");
    
    // Task 2: Motor Control (Priority 4)
    xTaskCreate(
        task_motor_control,
        "MotorControl",
        TASK_STACK_SIZE_MOTOR_CONTROL,
        NULL,
        TASK_PRIORITY_MOTOR_CONTROL,
        NULL
    );
    Serial.println("[SETUP] Created task: MotorControl (Priority 4)");
    
    // Task 3: Sensor Fusion (Priority 3)
    xTaskCreate(
        task_sensor_fusion,
        "SensorFusion",
        TASK_STACK_SIZE_SENSOR_FUSION,
        NULL,
        TASK_PRIORITY_SENSOR_FUSION,
        NULL
    );
    Serial.println("[SETUP] Created task: SensorFusion (Priority 3)");
    
    // Task 4: Communication (Priority 2)
    xTaskCreate(
        task_communication,
        "Communication",
        TASK_STACK_SIZE_COMMUNICATION,
        NULL,
        TASK_PRIORITY_COMMUNICATION,
        NULL
    );
    Serial.println("[SETUP] Created task: Communication (Priority 2)");
    
    // Task 5: Telemetry (Lowest Priority - 1)
    xTaskCreate(
        task_telemetry,
        "Telemetry",
        TASK_STACK_SIZE_TELEMETRY,
        NULL,
        TASK_PRIORITY_TELEMETRY,
        NULL
    );
    Serial.println("[SETUP] Created task: Telemetry (Priority 1)");
    
    Serial.println("\n[SETUP] All tasks created successfully!");
    Serial.println("[SETUP] System ready - FreeRTOS scheduler starting...\n");
    
    // Run command reception tests
    test_command_reception();
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
// TEST SUITE: Command Reception Testing
// ============================================================================

void test_command_reception() {
    Serial.println("\n========================================");
    Serial.println("TEST SUITE: Command Reception Validation");
    Serial.println("========================================\n");
    
    // Define all test commands with names
    struct CommandTest {
        uint8_t cmd_byte;
        const char* cmd_name;
    };
    
    CommandTest tests[] = {
        {CMD_STOP, "STOP"},
        {CMD_FORWARD, "FORWARD"},
        {CMD_BACKWARD, "BACKWARD"},
        {CMD_STRAFE_LEFT, "STRAFE_LEFT"},
        {CMD_STRAFE_RIGHT, "STRAFE_RIGHT"},
        {CMD_ROTATE_CW, "ROTATE_CW"},
        {CMD_ROTATE_CCW, "ROTATE_CCW"},
        {CMD_DIAGONAL_FORWARD_LEFT, "DIAGONAL_FORWARD_LEFT"},
        {CMD_DIAGONAL_FORWARD_RIGHT, "DIAGONAL_FORWARD_RIGHT"},
        {CMD_DIAGONAL_BACKWARD_LEFT, "DIAGONAL_BACKWARD_LEFT"},
        {CMD_DIAGONAL_BACKWARD_RIGHT, "DIAGONAL_BACKWARD_RIGHT"},
        {CMD_PIVOT_LEFT, "PIVOT_LEFT"},
        {CMD_PIVOT_RIGHT, "PIVOT_RIGHT"}
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    int failed = 0;
    
    Serial.println("Testing command validation for all commands:\n");
    
    // Test each valid command
    for (int i = 0; i < num_tests; i++) {
        bool is_valid = isValidCommand(tests[i].cmd_byte);
        
        if (is_valid) {
            Serial.print("[✓ PASS] ");
            passed++;
        } else {
            Serial.print("[✗ FAIL] ");
            failed++;
        }
        
        Serial.print("Command: ");
        Serial.print(tests[i].cmd_name);
        Serial.print(" (0x");
        if (tests[i].cmd_byte < 0x10) Serial.print("0");
        Serial.print(tests[i].cmd_byte, HEX);
        Serial.println(")");
    }
    
    // Test invalid commands
    Serial.println("\nTesting invalid command rejection:\n");
    
    uint8_t invalid_commands[] = {0x0D, 0x0E, 0x7F, 0xFF};
    int num_invalid = sizeof(invalid_commands) / sizeof(invalid_commands[0]);
    
    for (int i = 0; i < num_invalid; i++) {
        bool is_valid = isValidCommand(invalid_commands[i]);
        
        if (!is_valid) {
            Serial.print("[✓ PASS] ");
            passed++;
        } else {
            Serial.print("[✗ FAIL] ");
            failed++;
        }
        
        Serial.print("Invalid Command: 0x");
        if (invalid_commands[i] < 0x10) Serial.print("0");
        Serial.print(invalid_commands[i], HEX);
        Serial.println(" (correctly rejected)");
    }
    
    // Print summary
    Serial.println("\n========================================");
    Serial.println("TEST SUMMARY");
    Serial.println("========================================");
    Serial.print("Total Tests: ");
    Serial.println(passed + failed);
    Serial.print("Passed: ");
    Serial.print(passed);
    Serial.print(" (");
    Serial.print((passed * 100) / (passed + failed));
    Serial.println("%)");
    Serial.print("Failed: ");
    Serial.println(failed);
    
    if (failed == 0) {
        Serial.println("\n✓ ALL TESTS PASSED - System ready!");
    } else {
        Serial.println("\n✗ SOME TESTS FAILED - Check implementation!");
    }
    Serial.println("========================================\n");
}


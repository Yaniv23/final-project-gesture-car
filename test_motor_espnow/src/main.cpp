/**
 * @file main.cpp
 * @brief Motor + ESP-NOW Command Reception Test
 * @details Simplified test that only tests motor control and ESP-NOW command reception
 *          WITHOUT servo motor or ultrasonic sensor components
 * 
 * This test program:
 * 1. Initializes ESP-NOW and waits for connection from sender
 * 2. Initializes motor driver with 4 motors
 * 3. Creates FreeRTOS tasks for communication and motor control
 * 4. Receives commands via ESP-NOW and executes them on motors
 * 5. NO servo or ultrasonic sensor initialization
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

// Safety (minimal - no sensor-based stops)
#include "safety/watchdog.h"
#include "safety/timeout_monitor.h"
#include "safety/emergency_stop.h"

// Communication
#include "communication/command_protocol.h"
#include "communication/espnow_handler.h"

// Control
#include "control/motion_control.h"

// Task implementations (forward declarations)
void task_motor_control(void *pvParameters);
void task_communication(void *pvParameters);
void task_safety_monitor(void *pvParameters);

// Global motor driver instance
MotorDriver motor_driver;

// Task handles
TaskHandle_t taskHandle_safety = NULL;
TaskHandle_t taskHandle_motor = NULL;
TaskHandle_t taskHandle_comm = NULL;

// Global flag to signal tasks that setup is complete
volatile bool setupComplete = false;

void setup() {
    // Initialize Serial for debugging
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);  // Wait for Serial Monitor to connect
    
    Serial.println("\n========================================");
    Serial.println("Motor + ESP-NOW Test");
    Serial.println("Testing: Motor Control + Command Reception");
    Serial.println("Excluded: Servo Motor, Ultrasonic Sensor");
    Serial.println("========================================");
    
    // Initialize WiFi and ESP-NOW
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
    
    // Initialize ESP-NOW (SIMULATION_MODE = 0 for real ESP-NOW)
    if (espnow_init(false)) {  // false = normal mode, not simulation
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
    Serial.println("[SETUP] MotorDriver initialized - all motors stopped");
    
    // Initialize safety systems (minimal - no sensor-based stops)
    Serial.println("[SETUP] Initializing safety systems...");
    watchdog_init(WATCHDOG_TIMEOUT_MS);
    timeout_monitor_init(COMMAND_TIMEOUT_MS);
    emergency_stop_init();
    Serial.println("[SETUP] Safety systems initialized (timeout only, no sensors)");
    
    // Create FreeRTOS tasks
    Serial.println("========================================");
    Serial.println("[SETUP] Creating FreeRTOS tasks...");
    Serial.println("========================================");
    
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
    
    // Task 3: Communication (Priority 2)
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
    
    Serial.println("[SETUP] System ready 🎉 - FreeRTOS scheduler running!");
    Serial.println("[SETUP] Waiting for ESP-NOW commands...\n");
}

void loop() {
    // Empty - FreeRTOS tasks handle everything
    // In a FreeRTOS setup, loop() should not contain blocking code
    // All work is done in tasks created in setup()
    
    // Feed watchdog timer (safety mechanism)
    watchdog_feed();
    
    // This delay ensures loop() doesn't consume CPU
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ============================================================================
// TASK: Communication - Receives ESP-NOW commands and forwards to queue
// ============================================================================

void task_communication(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION);  // 100ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    Serial.println("[TASK_COMM] Running - waiting for ESP-NOW commands");
    
    ESPNowRawMessage raw_msg;
    
    while (1) {
        // Read command from ESP-NOW queue (ISR puts commands here)
        if (xQueueReceive(xESPNowQueue, &raw_msg, pdMS_TO_TICKS(TASK_PERIOD_COMMUNICATION))) {
            if (raw_msg.length >= 1) {
                uint8_t first_byte = raw_msg.first_byte;

                // -----------------------------------------------------------------
                // Handshake / control messages (NOT motion commands)
                // -----------------------------------------------------------------
                if (first_byte == CMD_HANDSHAKE_INIT) {
                    Serial.print("[COMM] 🤝 Handshake INIT received, protocol byte = ");
                    Serial.println(raw_msg.second_byte);

                    // Echo back an ACK with the protocol/status byte
                    if (!espnow_send_handshake_ack(raw_msg.second_byte)) {
                        Serial.println("[COMM] ⚠️ Failed to send HANDSHAKE_ACK");
                    } else {
                        Serial.println("[COMM] ✅ HANDSHAKE_ACK sent");
                    }

                    // Do NOT forward handshake frames to motor control
                } else if (first_byte == CMD_HEARTBEAT) {
                    // Optional future use: could feed timeout monitor here
                    Serial.println("[COMM] 💓 HEARTBEAT frame received (ignored for now)");
                } else {
                    // -----------------------------------------------------------------
                    // Normal motion commands
                    // -----------------------------------------------------------------
                    if (isValidCommand(first_byte)) {
                        uint8_t cmd_byte = first_byte;
                        if (xQueueSend(xCommandQueue, &cmd_byte, 0) != pdTRUE) {
                            Serial.println("[COMM] Warning: Command queue full!");
                        } else {
                            // Reset timeout monitor on valid command
                            timeout_monitor_reset();
                        }
                    } else {
                        Serial.print("[COMM] ❌ INVALID command byte (ignored): 0x");
                        Serial.println(first_byte, HEX);
                    }
                }
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// ============================================================================
// TASK: Motor Control - Processes commands from queue and controls motors
// ============================================================================

void task_motor_control(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL);  // 10ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    uint8_t last_logged_cmd = 0xFF;  // Track last logged command to avoid duplicate prints
    
    // Wait for setup to complete before initializing
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    // Initialize motion control with MotorDriver
    motion_init(&motor_driver);
    Serial.println("[TASK_MOTOR] Motion control initialized");
    
    uint8_t cmd_byte = 0;
    
    while (1) {
        // CRITICAL: Check emergency stop flag FIRST (before any command processing)
        if (emergency_stop_is_active()) {
            // Emergency stop is active - ensure motors are stopped
            motion_stop();
            
            // Only process STOP command to acknowledge (emergency stop must be cleared separately)
            if (xQueueReceive(xCommandQueue, &cmd_byte, 0) == pdTRUE) {
                if (cmd_byte == CMD_STOP) {
                    Serial.println("[MOTOR] STOP (Emergency stop active - use emergency_stop_clear() to resume)");
                } else {
                    Serial.println("[MOTOR] Command blocked - Emergency stop active!");
                }
            }
            
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }
        
        // Emergency stop is NOT active - proceed with normal command processing
        if (xQueueReceive(xCommandQueue, &cmd_byte, pdMS_TO_TICKS(TASK_PERIOD_MOTOR_CONTROL))) {
            // Decide if we need to log this command (only when it changes)
            bool shouldLog = (cmd_byte != last_logged_cmd);
            
            // Double-check emergency stop flag (race condition protection)
            if (emergency_stop_is_active()) {
                motion_stop();
                Serial.println("[MOTOR] Emergency stop detected - Command cancelled");
                vTaskDelayUntil(&lastWakeTime, period);
                continue;
            }
              
            // Check safety semaphore (secondary safety mechanism)
            if (xSafetySemaphore != NULL && 
                xSemaphoreTake(xSafetySemaphore, 0) == pdTRUE) {
                // System is safe - execute command
                switch (cmd_byte) {
                case CMD_STOP:
                    motion_stop();
                    if (shouldLog) Serial.println("[MOTOR] STOP");
                    break;
                case CMD_FORWARD:
                    motion_forward();
                    if (shouldLog) Serial.println("[MOTOR] FORWARD");
                    break;
                case CMD_BACKWARD:
                    motion_backward();
                    if (shouldLog) Serial.println("[MOTOR] BACKWARD");
                    break;
                case CMD_SIDEWAY_LEFT:
                    motion_sideway_left();
                    if (shouldLog) Serial.println("[MOTOR] SIDEWAY_LEFT");
                    break;
                case CMD_SIDEWAY_RIGHT:
                    motion_sideway_right();
                    if (shouldLog) Serial.println("[MOTOR] SIDEWAY_RIGHT");
                    break;
                case CMD_ROTATE_CW:
                    motion_rotate_cw();
                    if (shouldLog) Serial.println("[MOTOR] ROTATE_CW");
                    break;
                case CMD_ROTATE_CCW:
                    motion_rotate_ccw();
                    if (shouldLog) Serial.println("[MOTOR] ROTATE_CCW");
                    break;
                case CMD_DIAGONAL_315:
                    motion_diagonal_315();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_315");
                    break;
                case CMD_DIAGONAL_45:
                    motion_diagonal_45();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_45");
                    break;
                case CMD_DIAGONAL_225:
                    motion_diagonal_225();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_225");
                    break;
                case CMD_DIAGONAL_135:
                    motion_diagonal_135();
                    if (shouldLog) Serial.println("[MOTOR] DIAGONAL_135");
                    break;
                case CMD_PIVOT_LEFT:
                    motion_pivot_left();
                    if (shouldLog) Serial.println("[MOTOR] PIVOT_LEFT");
                    break;
                case CMD_PIVOT_RIGHT:
                    motion_pivot_right();
                    if (shouldLog) Serial.println("[MOTOR] PIVOT_RIGHT");
                    break;
                default:
                    if (shouldLog) {
                        Serial.print("[MOTOR] Unknown command byte: 0x");
                        Serial.println(cmd_byte, HEX);
                    }
                    motion_stop();  // Safety: stop on unknown command
                    break;
                }
                
                // Update last logged command after successful handling
                last_logged_cmd = cmd_byte;
                
                // Return semaphore after command execution
                xSemaphoreGive(xSafetySemaphore);
            } else {
                // Semaphore not available (shouldn't happen if flag check passed)
                Serial.println("[MOTOR] Warning: Safety semaphore unavailable - Command blocked");
                motion_stop();  // Safety: stop motors
            }
        }
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// ============================================================================
// TASK: Safety Monitor - Minimal safety (timeout only, no sensor-based stops)
// ============================================================================

void task_safety_monitor(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SAFETY_MONITOR);  // 50ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    // Wait for setup to complete before starting monitoring
    while (!setupComplete) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Check every 10ms
    }
    
    Serial.println("[TASK_SAFETY] Safety monitor running (timeout only, no sensors)");
    
    while (1) {
        // Feed watchdog (prevents system reset)
        watchdog_feed();
        
        // Check command timeout (if no command received for COMMAND_TIMEOUT_MS, stop motors)
        // Note: timeout_monitor_check() is currently a stub, but structure is in place
        timeout_monitor_check();
        
        // Note: No sensor-based emergency stops in this test
        // Emergency stop can still be triggered manually if needed
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
}


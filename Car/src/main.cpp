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
#include "soc/rtc_cntl_reg.h"

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

// Global motor driver instance
MotorDriver motor_driver;

// Task handles for suspending/resuming tasks
TaskHandle_t taskHandle_motor = NULL;
TaskHandle_t taskHandle_comm = NULL;
TaskHandle_t taskHandle_autonomous = NULL;
TaskHandle_t taskHandle_sensors = NULL;

// Global flag to signal tasks that setup is complete
volatile bool setupComplete = false;

void setup() {
    // Disable brownout detector (battery: voltage drops during WiFi TX / init spikes)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(SERIAL_BAUD_RATE);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    delay(3000);

    Serial.println("\n========================================");
    Serial.println("Gesture Car - ESP32 Vehicle Controller");
    Serial.println("========================================");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    delay(1500);

    esp_wifi_set_max_tx_power(78);

    if (espnow_init(false)) {
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
        }
    } else {
        Serial.println("[ERROR] ESP-NOW initialization failed!");
        while (1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
    }

    if (!initSharedQueues()) {
        Serial.println("[ERROR] Failed to initialize shared queues / SensorState!");
        while (1) delay(1000);
    }

    MotorDriver::MotorConfig motor_configs[4] = {
        {FRONT_LEFT_IN1, FRONT_LEFT_IN2, FRONT_LEFT_EN},
        {FRONT_RIGHT_IN1, FRONT_RIGHT_IN2, FRONT_RIGHT_EN},
        {BACK_LEFT_IN1, BACK_LEFT_IN2, BACK_LEFT_EN},
        {BACK_RIGHT_IN1, BACK_RIGHT_IN2, BACK_RIGHT_EN}
    };

    if (!motor_driver.init(motor_configs)) {
        Serial.println("[ERROR] Failed to initialize MotorDriver!");
        while (1) delay(1000);
    }
    motor_driver.stopAll();

    watchdog_init(WATCHDOG_TIMEOUT_MS);
    timeout_monitor_init(COMMAND_TIMEOUT_MS);

    xTaskCreate(
        task_motor_control,
        "MotorControl",
        TASK_STACK_SIZE_MOTOR_CONTROL,
        NULL,
        TASK_PRIORITY_MOTOR_CONTROL,
        &taskHandle_motor
    );

    xTaskCreate(
        task_sensors,
        "Sensors",
        TASK_STACK_SIZE_SENSORS,
        NULL,
        TASK_PRIORITY_SENSORS,
        &taskHandle_sensors
    );

    xTaskCreate(
        task_autonomous,
        "Autonomous",
        AUTONOMOUS_TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY_AUTONOMOUS,
        &taskHandle_autonomous
    );

    ModeManager& mode_mgr = ModeManager::getInstance();
    mode_mgr.registerTaskHandles(taskHandle_autonomous, taskHandle_sensors);
    mode_mgr.setMode(MODE_MANUAL);

    xTaskCreate(
        task_communication,
        "Communication",
        TASK_STACK_SIZE_COMMUNICATION,
        NULL,
        TASK_PRIORITY_COMMUNICATION,
        &taskHandle_comm
    );

    setupComplete = true;
    delay(100);
}

static uint32_t led_last_toggle = 0;
static bool led_state = HIGH;
static const uint32_t LED_BLINK_INTERVAL_MS = 500;

void loop() {
    watchdog_feed();

    bool is_connected = espnow_is_connected();
    uint32_t now = millis();

    if (!is_connected) {
        if (now - led_last_toggle >= LED_BLINK_INTERVAL_MS) {
            led_state = !led_state;
            digitalWrite(LED_BUILTIN, led_state);
            led_last_toggle = now;
        }
    } else {
        if (led_state != HIGH) {
            led_state = HIGH;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}

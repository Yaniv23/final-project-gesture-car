#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Hardware Pin Definitions
// ============================================================================
// NOTE: ESP32 GPIO 34, 35, 36, 39 are INPUT-ONLY and cannot be used for PWM output

// Front Right Motor (Driver #2, Channel C)
#define FRONT_RIGHT_EN     27  // PWM pin
#define FRONT_RIGHT_IN1    25  // Direction pin 1
#define FRONT_RIGHT_IN2    26  // Direction pin 2

// Back Right Motor (Driver #1, Channel A)
#define BACK_RIGHT_EN      19  // PWM pin
#define BACK_RIGHT_IN1     5   // Direction pin 1
#define BACK_RIGHT_IN2     18  // Direction pin 2

// Front Left Motor (Driver #2, Channel D)
#define FRONT_LEFT_EN      14  // PWM pin
#define FRONT_LEFT_IN1     32  // Direction pin 1
#define FRONT_LEFT_IN2     33  // Direction pin 2

// Back Left Motor (Driver #1, Channel B)
#define BACK_LEFT_EN       23  // PWM pin
#define BACK_LEFT_IN1      21  // Direction pin 1
#define BACK_LEFT_IN2      22  // Direction pin 2

// Servo Motor
#define SERVO_PIN          4
#define SERVO_LEDC_CHANNEL 4
#define SERVO_FREQUENCY    50   // 50 Hz standard servo
#define SERVO_RESOLUTION   16   // 16-bit resolution
#define SERVO_MIN_PULSE_US 500  // 0.5ms pulse for 0 degrees
#define SERVO_MAX_PULSE_US 2500 // 2.5ms pulse for 180 degrees
#define SERVO_STABILIZATION_MS 200  // Servo stabilization delay in milliseconds

// Ultrasonic Sensors (front + rear)
#define ULTRASONIC_TRIG        12  // Front trigger
#define ULTRASONIC_ECHO        16  // Front echo (input only OK)
#define ULTRASONIC_TRIG_REAR   17  // Rear trigger (spare GPIO)
#define ULTRASONIC_ECHO_REAR   34  // Rear echo (input-only pin)

// ============================================================================
// Motor Speed Constants
// ============================================================================
#define MOTOR_SPEED_SLOW     150   // Slow speed (0-255)
#define MOTOR_SPEED_FAST     255   // Fast speed (0-255)
#define MOTOR_SPEED_MAX_8BIT 255   // Maximum 8-bit value
#define MOTOR_PWM_MAX        1023  // Maximum PWM value (10-bit)

// ============================================================================
// FreeRTOS Task Configuration
// ============================================================================

// Task Priorities (higher number = higher priority)
#define TASK_PRIORITY_MOTOR_CONTROL     4  // Highest - real-time motor control
#define TASK_PRIORITY_AUTONOMOUS        3  // Medium - autonomous navigation
#define TASK_PRIORITY_COMMUNICATION     2  // Lower - command handling

// Task Periods (milliseconds)
#define TASK_PERIOD_MOTOR_CONTROL      10   // 100 Hz - motor updates
#define TASK_PERIOD_COMMUNICATION     100   // 10 Hz - command processing
#define AUTONOMOUS_TASK_PERIOD_MS      50   // 20 Hz - autonomous loop

// Task Stack Sizes (bytes)
#define TASK_STACK_SIZE_MOTOR_CONTROL   4096
#define TASK_STACK_SIZE_COMMUNICATION   4096
#define AUTONOMOUS_TASK_STACK_SIZE      2048  // Reduced - stub only

// ============================================================================
// Communication Settings
// ============================================================================
#define SERIAL_BAUD_RATE           115200
#define ESP_NOW_CHANNEL            0
#define COMMAND_TIMEOUT_MS         500   // Auto-stop if no command received

// ============================================================================
// Safety Settings
// ============================================================================
#define WATCHDOG_TIMEOUT_MS        5000  // 5 second watchdog

#endif // CONFIG_H

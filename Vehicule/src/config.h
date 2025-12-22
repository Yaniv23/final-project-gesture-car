#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Hardware Pin Definitions
// ============================================================================
// Based on existing Vehicule_Controller.ino pin assignments

// Common PWM pin for all motors (controls speed of all motors simultaneously)
#define MOTOR_PWM_COMMON   34  // Common PWM enable pin for all 4 motors

// Front Right Motor (L298N Driver #1)
#define FRONT_RIGHT_IN1    35
#define FRONT_RIGHT_IN2    32

// Front Left Motor (L298N Driver #1)
#define FRONT_LEFT_IN3     33
#define FRONT_LEFT_IN4     25

// Back Right Motor (L298N Driver #2)
#define BACK_RIGHT_IN1     27
#define BACK_RIGHT_IN2     14

// Back Left Motor (L298N Driver #2)
#define BACK_LEFT_IN3      12
#define BACK_LEFT_IN4      13

// Servo Motor (for scanning)
#define SERVO_PIN          4

// Ultrasonic Sensor (HC-SR04)
#define ULTRASONIC_TRIG    18
#define ULTRASONIC_ECHO    16

// ============================================================================
// Motor Speed Constants
// ============================================================================
#define MOTOR_SPEED_SLOW   150  // Slow speed (0-255)
#define MOTOR_SPEED_FAST   255  // Fast speed (0-255)
#define MOTOR_SPEED_MAX_8BIT 255  // Maximum value for 8-bit speed range
#define MOTOR_PWM_MAX      1023 // Maximum PWM value for MotorDriver (10-bit)

// ============================================================================
// FreeRTOS Task Priorities
// ============================================================================
// Higher number = higher priority
// Range: 0-25 (configMAX_PRIORITIES)
#define TASK_PRIORITY_SAFETY_MONITOR    5  // Highest - safety critical
#define TASK_PRIORITY_MOTOR_CONTROL     4  // High - real-time control
#define TASK_PRIORITY_SENSOR_FUSION     3  // Medium - sensor reading
#define TASK_PRIORITY_COMMUNICATION     2  // Medium - command handling
#define TASK_PRIORITY_TELEMETRY         1  // Lowest - status reporting

// ============================================================================
// FreeRTOS Task Periods (milliseconds)
// ============================================================================
#define TASK_PERIOD_MOTOR_CONTROL      10   // 100 Hz - critical for control
#define TASK_PERIOD_SENSOR_FUSION      50   // 20 Hz - sensor updates
#define TASK_PERIOD_COMMUNICATION     100   // 10 Hz - command processing
#define TASK_PERIOD_SAFETY_MONITOR     50   // 20 Hz - safety checks
#define TASK_PERIOD_TELEMETRY         100   // 10 Hz - status updates

// ============================================================================
// Stack Sizes (bytes)
// ============================================================================
// Adjust based on actual usage (monitor with FreeRTOS stack high water mark)
#define TASK_STACK_SIZE_MOTOR_CONTROL   4096
#define TASK_STACK_SIZE_SENSOR_FUSION   2048
#define TASK_STACK_SIZE_COMMUNICATION   4096
#define TASK_STACK_SIZE_SAFETY_MONITOR  2048
#define TASK_STACK_SIZE_TELEMETRY       2048

// ============================================================================
// Communication Settings
// ============================================================================
#define SERIAL_BAUD_RATE           115200
#define ESP_NOW_CHANNEL            0
#define PROTOCOL_TIMEOUT_MS        500
#define COMMAND_TIMEOUT_MS         500   // Auto-stop if no command received

// ============================================================================
// Safety Settings
// ============================================================================
#define WATCHDOG_TIMEOUT_MS        5000   // 5 second watchdog
#define EMERGENCY_STOP_DISTANCE_CM 10    // Stop if obstacle < 10cm

// ============================================================================
// Kinematic Parameters (shared with Person 2)
// ============================================================================
// These will be used by Person 2's kinematics code
// Update with actual measurements from your robot
#define WHEEL_RADIUS_M             0.05f   // 5cm radius (adjust to actual)
#define WHEEL_BASE_M               0.20f   // 20cm front-to-back (adjust)
#define TRACK_WIDTH_M              0.18f   // 18cm left-to-right (adjust)

#endif // CONFIG_H


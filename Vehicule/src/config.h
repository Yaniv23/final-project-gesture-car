#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Hardware Pin Definitions
// ============================================================================
// NOTE: ESP32 GPIO 34, 35, 36, 39 are INPUT-ONLY and cannot be used for PWM output
// Using valid PWM-capable pins (2-33, except 24, 28-31)

// Front Right Motor (Driver #2, Channel C)
#define FRONT_RIGHT_EN     27  // PWM pin for Front Right motor (PWMC on Driver #2)
#define FRONT_RIGHT_IN1    25  // Front Right direction pin 1 (CIN1 on Driver #2)
#define FRONT_RIGHT_IN2    26  // Front Right direction pin 2 (CIN2 on Driver #2)

// Back Right Motor (Driver #1, Channel A)
#define BACK_RIGHT_EN      19  // PWM pin for Back Right motor (PWMA on Driver #1)
#define BACK_RIGHT_IN1     5   // Back Right direction pin 1 (AIN1 on Driver #1)
#define BACK_RIGHT_IN2     18  // Back Right direction pin 2 (AIN2 on Driver #1)

// Front Left Motor (Driver #2, Channel D)
#define FRONT_LEFT_EN      14  // PWM pin for Front Left motor (PWMD on Driver #2)
#define FRONT_LEFT_IN1     32  // Front Left direction pin 1 (DIN1 on Driver #2)
#define FRONT_LEFT_IN2     33  // Front Left direction pin 2 (DIN2 on Driver #2)

// Back Left Motor (Driver #1, Channel B)
#define BACK_LEFT_EN       23  // PWM pin for Back Left motor (PWMB on Driver #1)
#define BACK_LEFT_IN1      21  // Back Left direction pin 1 (BIN1 on Driver #1)
#define BACK_LEFT_IN2      22  // Back Left direction pin 2 (BIN2 on Driver #1)
// Servo Motor (for scanning)
// Using LEDC channel 4 to avoid conflict with motors (channels 0-3)
#define SERVO_PIN          4
#define SERVO_LEDC_CHANNEL 4
#define SERVO_FREQUENCY    50   // 50 Hz (20ms period) - standard servo frequency
#define SERVO_RESOLUTION   16   // 16-bit resolution for precision
#define SERVO_MIN_PULSE_US 500  // 0.5ms pulse for 0 degrees
#define SERVO_MAX_PULSE_US 2500 // 2.5ms pulse for 180 degrees

// Ultrasonic Sensor (HC-SR04)
#define ULTRASONIC_TRIG    12
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
#define ESP_NOW_CONNECTION_TIMEOUT_MS 60000  // 60 seconds - wait for connection in setup

// Simulation mode: Set to 1 for testing without ESP-NOW receiver
// Set to 0 for actual ESP-NOW operation with remote control
#define SIMULATION_MODE            0     // 1 = Simulation (no ESP-NOW wait), 0 = Normal (ESP-NOW active)

// ============================================================================
// Safety Settings
// ============================================================================
#define WATCHDOG_TIMEOUT_MS        5000   // 5 second watchdog

// ============================================================================
// Kinematic Parameters (shared with Person 2)
// ============================================================================
// These will be used by Person 2's kinematics code
// Update with actual measurements from your robot
#define WHEEL_RADIUS_M             0.05f   // 5cm radius (adjust to actual)
#define WHEEL_BASE_M               0.20f   // 20cm front-to-back (adjust)
#define TRACK_WIDTH_M              0.18f   // 18cm left-to-right (adjust)

// ============================================================================
// Autonomous Mode Settings
// ============================================================================
#define AUTONOMOUS_TASK_PRIORITY     3
#define AUTONOMOUS_TASK_PERIOD_MS    50   // 20 Hz
#define AUTONOMOUS_TASK_STACK_SIZE   4096

// Navigation parameters
#define OBSTACLE_DISTANCE_THRESHOLD_CM  20  // Distance minimale avant obstacle
#define SAFE_DISTANCE_CM                30  // Distance de sécurité
#define TURN_DURATION_MS                450  // Durée de rotation (réduit de 1000ms pour plus de fluidité)
#define AUTONOMOUS_FORWARD_SPEED       200  // Vitesse avant en mode autonome
#define AUTONOMOUS_TURN_SPEED          150  // Vitesse de rotation

// Autonomous Mode Improvements - Inspired by Obstacle_Avoidance_Bot
#define BACKUP_TIME_MS                400   // Durée de recul après détection d'obstacle
#define ULTRASONIC_TIMEOUT_US         30000 // Timeout pour pulseIn (30ms)
#define FORWARD_CHECK_PERIOD_MS       50    // Vérification distance pendant forward (chaque cycle)
#define PERIODIC_RESCAN_MS           500   // Rescan périodique (réduit de 2000ms)
#define SERVO_MOVE_DELAY_MS          100   // Délai servo réduit (de 150ms) pour scanning plus rapide

// Continuous Scanning Configuration
#define SCAN_MIN_ANGLE                0     // Angle minimum du sweep (degrés)
#define SCAN_MAX_ANGLE                60    // Angle maximum du sweep (degrés)
#define SCAN_STEP_DEG                 5     // Pas du sweep (degrés)
#define SCAN_STEP_INTERVAL_MS         50    // Intervalle entre chaque pas (ms)
#define SCAN_REST_INTERVAL_MS         0     // Pas de pause entre les sweeps (0 = continu)
#define SCAN_CENTER_ANGLE              30   // Angle considéré comme "centre"
#define SCAN_CENTER_TOLERANCE         5    // Tolérance pour angles centraux (±5°)
#define SCAN_ANGLES_COUNT              13   // Nombre d'angles (0°, 5°, 10°, ..., 60° = 13 angles)
#define SCAN_SERVO_STABILIZATION_MS   20   // Temps d'attente pour stabilisation servo avant mesure

// Stuck Detection Configuration
#define STUCK_DETECTION_ENABLED       1     // Activer la détection de blocage
#define STUCK_THRESHOLD_ATTEMPTS      5     // Nombre d'échecs consécutifs avant détection
#define STUCK_TIME_THRESHOLD_MS       3000  // Temps sans mouvement avant détection (3s)
#define STUCK_ALL_DIRECTIONS_THRESHOLD 15   // Si toutes distances < 15cm = bloqué
#define STUCK_PIVOT_MAX_DURATION_MS   5000  // Durée max de pivot (5s)
#define STUCK_PIVOT_SPEED             150   // Vitesse de pivot (0-255, utilise AUTONOMOUS_TURN_SPEED)

#endif // CONFIG_H


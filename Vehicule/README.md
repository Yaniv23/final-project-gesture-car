# ESP32 Vehicle Controller

Advanced FreeRTOS-based control system for gesture-controlled mecanum wheel robot. This ESP32-based controller receives wireless commands via ESP-NOW and controls 4 motors, a servo, and an ultrasonic sensor.

## Architecture Overview

The vehicle controller uses **FreeRTOS** (Real-Time Operating System) to manage multiple tasks simultaneously:

- **Safety Monitor** (Priority 5) - Highest priority, monitors for obstacles and emergencies
- **Motor Control** (Priority 4) - Real-time motor control at 100Hz
- **Sensor Fusion** (Priority 3) - Reads ultrasonic sensor and servo control
- **Communication** (Priority 2) - Handles ESP-NOW command reception
- **Telemetry** (Priority 1) - Status reporting and debugging

### Key Features

- **Multi-task Architecture**: FreeRTOS tasks for concurrent operations
- **Safety Systems**: Watchdog, emergency stop, timeout monitoring
- **Mecanum Drive**: Full omnidirectional movement (forward, backward, strafe, rotate, diagonal)
- **Obstacle Detection**: Ultrasonic sensor with automatic emergency stop
- **Binary Protocol**: Efficient single-byte command protocol over ESP-NOW

## Hardware Connections

### Motor Connections

The vehicle uses **4 DC motors** with **2 L298N motor driver modules**. All motors share a **common PWM pin** for speed control.

#### Common PWM Pin
- **Pin 2**: Connected to all motor drivers' ENA pins (controls speed for all motors)

#### Front Right Motor (L298N Driver #1)
- **IN1**: GPIO 5
- **IN2**: GPIO 32
- **ENA**: Pin 2 (common PWM)

#### Front Left Motor (L298N Driver #1)
- **IN3**: GPIO 33
- **IN4**: GPIO 25
- **ENA**: Pin 2 (common PWM)

#### Back Right Motor (L298N Driver #2)
- **IN1**: GPIO 27
- **IN2**: GPIO 14
- **ENA**: Pin 2 (common PWM)

#### Back Left Motor (L298N Driver #2)
- **IN3**: GPIO 12
- **IN4**: GPIO 13
- **ENA**: Pin 2 (common PWM)

### Sensor Connections

- **Servo Motor**: GPIO 4 (for obstacle scanning)
- **Ultrasonic Sensor**:
  - **TRIG**: GPIO 18
  - **ECHO**: GPIO 16

### Power Supply

- **ESP32**: USB or external 5V supply
- **Motors**: Separate power supply (e.g., 7.4V battery pack) connected to L298N drivers
- **Common Ground**: Ensure all grounds are connected together

> **Important**: GPIO pins 34, 35, 36, 39 are INPUT-ONLY on ESP32 and cannot be used for PWM output. The configuration uses valid PWM-capable pins.

## Building and Uploading

### Prerequisites

- **PlatformIO**: Install with `pip install platformio`
- **USB Cable**: For connecting ESP32 to PC
- **Serial Port**: Identify your ESP32's COM port

### Build Instructions

```bash
cd Vehicule

# Build the project
pio run

# Build and upload to ESP32
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor
```

### Port Configuration

Edit `platformio.ini` to set your upload port:

```ini
upload_port = /dev/ttyUSB0  # Linux
# upload_port = COM3        # Windows
# upload_port = /dev/cu.usbserial-*  # macOS
```

Or specify at upload time:
```bash
pio run -t upload --upload-port /dev/ttyUSB0
```

## Configuration

### Pin Assignments

All pin definitions are in `src/config.h`. Key constants:

```cpp
// Common PWM pin for all motors
#define MOTOR_PWM_COMMON   2

// Motor direction pins
#define FRONT_RIGHT_IN1    5
#define FRONT_RIGHT_IN2    32
// ... (see config.h for complete list)

// Sensors
#define SERVO_PIN          4
#define ULTRASONIC_TRIG    18
#define ULTRASONIC_ECHO    16
```

### Motor Speed Settings

Adjust motor speeds in `src/config.h`:

```cpp
#define MOTOR_SPEED_SLOW   150  // Slow speed (0-255)
#define MOTOR_SPEED_FAST   255  // Fast speed (0-255)
```

### Safety Thresholds

Configure safety parameters:

```cpp
#define EMERGENCY_STOP_DISTANCE_CM 10    // Stop if obstacle < 10cm
#define COMMAND_TIMEOUT_MS         500   // Auto-stop if no command received
#define WATCHDOG_TIMEOUT_MS        5000  // 5 second watchdog
```

### FreeRTOS Task Configuration

Task priorities and periods (in `src/config.h`):

```cpp
// Task Priorities (higher = more important)
#define TASK_PRIORITY_SAFETY_MONITOR    5
#define TASK_PRIORITY_MOTOR_CONTROL     4
#define TASK_PRIORITY_SENSOR_FUSION     3
#define TASK_PRIORITY_COMMUNICATION     2
#define TASK_PRIORITY_TELEMETRY         1

// Task Periods (milliseconds)
#define TASK_PERIOD_MOTOR_CONTROL      10   // 100 Hz
#define TASK_PERIOD_SENSOR_FUSION      50   // 20 Hz
#define TASK_PERIOD_COMMUNICATION     100   // 10 Hz
```

## ESP-NOW Setup

### Getting the MAC Address

1. Upload the code to your ESP32
2. Open Serial Monitor (115200 baud)
3. Look for output like:
   ```
   [COMM] ESP-NOW initialized
   [COMM] MAC Address: XX:XX:XX:XX:XX:XX
   ```
4. **Copy this MAC address** - you'll need it for the sender ESP32

### Pairing with Sender

The sender ESP32 needs to know the vehicle controller's MAC address. See [Sender Documentation](../Transmission/Sender_Code/README.md) for configuration.

## Serial Monitor Usage

### Startup Output

When the vehicle controller starts, you should see:

```
========================================
Gesture Car - ESP32 Vehicle Controller
Phase 1: Infrastructure Setup
========================================
FreeRTOS Version: 10.4.3
CPU Frequency: 240 MHz
Free Heap: 250000 bytes
========================================

[SETUP] Initializing shared queues...
[SETUP] Shared queues initialized
[SETUP] Initializing MotorDriver...
[SETUP] MotorDriver initialized (common PWM on pin 2)
[SETUP] Safety systems initialized
[SETUP] Creating FreeRTOS tasks...
[SETUP] Created task: SafetyMonitor (Priority 5)
[SETUP] Created task: MotorControl (Priority 4)
[SETUP] Created task: SensorFusion (Priority 3)
[SETUP] Created task: Communication (Priority 2)
[SETUP] Created task: Telemetry (Priority 1)
[SETUP] All tasks created successfully!
[SETUP] System ready - FreeRTOS scheduler starting...
```

### Command Reception

When commands are received via ESP-NOW:

```
[COMM] Received command byte: 0x01
[MOTOR] FORWARD
```

### Emergency Stop

If obstacle detected:

```
⚠️ Object detected close!
[MOTOR] EMERGENCY STOP
```

## Testing

See [TESTING_GUIDE.md](TESTING_GUIDE.md) for comprehensive testing procedures.

### Quick Test

1. **Elevate vehicle** (wheels off ground) for safety
2. **Upload code** and open Serial Monitor
3. **Note MAC address** from Serial Monitor
4. **Send test commands** via ESP-NOW sender
5. **Verify motors respond** correctly

### Command Byte Reference

| Command | Byte | Description |
|---------|------|-------------|
| STOP | 0x00 | All motors stop |
| FORWARD | 0x01 | Move forward |
| BACKWARD | 0x02 | Move backward |
| STRAFE_LEFT | 0x03 | Strafe left |
| STRAFE_RIGHT | 0x04 | Strafe right |
| ROTATE_CW | 0x05 | Rotate clockwise |
| ROTATE_CCW | 0x06 | Rotate counter-clockwise |
| DIAGONAL_FORWARD_LEFT | 0x07 | Diagonal forward-left |
| DIAGONAL_FORWARD_RIGHT | 0x08 | Diagonal forward-right |
| DIAGONAL_BACKWARD_LEFT | 0x09 | Diagonal backward-left |
| DIAGONAL_BACKWARD_RIGHT | 0x0A | Diagonal backward-right |
| PIVOT_LEFT | 0x0B | Pivot left |
| PIVOT_RIGHT | 0x0C | Pivot right |

## Troubleshooting

### Motors Don't Move

**Check:**
- Common PWM pin (pin 2) connected to all motor drivers' ENA pins
- Motor driver power supply is adequate
- Direction pins connected correctly
- Serial Monitor for error messages

**Solution:**
- Verify pin connections match `config.h`
- Check motor driver power supply voltage
- Test with Serial Monitor to see if commands are received

### Wrong Movement Direction

**Check:**
- IN1/IN2 pins swapped in motor configuration
- Motor wiring matches pin assignments

**Solution:**
- Swap direction pins in `config.h` or physically swap motor wires

### ESP-NOW Not Receiving Commands

**Check:**
- MAC address printed in Serial Monitor
- Sender ESP32 configured with correct MAC address
- Both ESP32s powered on
- ESP-NOW channel matches (default: 0)

**Solution:**
- Verify MAC address in sender code matches vehicle controller MAC
- Check Serial Monitor shows ESP-NOW initialization success

### Emergency Stop Always Active

**Check:**
- Ultrasonic sensor reading (should be > 10cm normally)
- `EMERGENCY_STOP_DISTANCE_CM` in `config.h`

**Solution:**
- Check ultrasonic sensor connections (TRIG/ECHO pins)
- Increase distance threshold if sensor is too sensitive
- Verify sensor is not blocked or damaged

### Vehicle Moves Too Fast/Slow

**Solution:**
- Adjust `MOTOR_SPEED_SLOW` and `MOTOR_SPEED_FAST` in `config.h`
- Range: 0-255 (lower = slower)
- Rebuild and upload after changes

## Code Structure

```
Vehicule/
├── src/
│   ├── main.cpp                 # Entry point, FreeRTOS initialization
│   ├── config.h                 # Pin definitions, constants
│   ├── drivers/                 # Hardware drivers
│   │   ├── motor_driver.h/cpp   # Motor control
│   │   ├── servo_driver.h/cpp   # Servo control
│   │   └── ultrasonic_driver.h/cpp  # Distance sensor
│   ├── communication/          # Communication protocols
│   │   ├── command_protocol.h   # Command byte definitions
│   │   └── espnow_handler.h/cpp # ESP-NOW implementation
│   ├── control/                 # Control algorithms
│   │   └── motion_control.h/cpp # Movement control logic
│   ├── safety/                  # Safety systems
│   │   ├── watchdog.h/cpp       # Watchdog timer
│   │   ├── timeout_monitor.h/cpp # Command timeout
│   │   └── emergency_stop.h/cpp # Emergency stop
│   ├── tasks/                   # FreeRTOS tasks
│   │   ├── task_motor_control.cpp
│   │   ├── task_communication.cpp
│   │   ├── task_sensor_fusion.cpp
│   │   ├── task_safety_monitor.cpp
│   │   └── task_telemetry.cpp
│   └── shared/                  # Shared resources
│       ├── types.h              # Common data structures
│       └── queues.h/cpp         # FreeRTOS queues
├── platformio.ini               # Build configuration
├── README.md                    # This file
└── TESTING_GUIDE.md            # Testing procedures
```

## Technical Documentation

For detailed technical information, see:
- [Vehicle Controller Technical Details](../docs/VEHICLE_CONTROLLER.md) - Architecture, protocols, API

## Development Status

- [x] FreeRTOS task structure
- [x] Motor driver implementation
- [x] ESP-NOW communication
- [x] Safety systems (watchdog, emergency stop, timeout)
- [x] Binary command protocol
- [x] Sensor integration (ultrasonic, servo)
- [x] Motion control (all movement types)

## Next Steps

- Fine-tune PID controllers (if velocity feedback added)
- Add encoder support for closed-loop control
- Implement sensor fusion (IMU integration)
- Add telemetry reporting
- Optimize power consumption

---

**For questions or issues, check the troubleshooting section or review the technical documentation.**

# Task 1.1: Project Structure Setup - Detailed Implementation Guide

**Assigned to:** Person 1 (Infrastructure Lead)  
**Timeline:** Day 1-2 (3-4 hours total)  
**Dependencies:** None (can start immediately after joint session)

---

## Overview

This task establishes the foundation for the entire Phase 1 refactoring. You'll set up the PlatformIO build system, create a modular directory structure, and prepare the project for FreeRTOS-based development.

**What you'll accomplish:**
- PlatformIO project initialization
- Complete directory structure matching the architecture
- Basic compilation setup
- Git branch strategy
- Configuration files

---

## Prerequisites

Before starting, ensure you have:

- [ ] PlatformIO installed (`pip install platformio` or PlatformIO IDE)
- [ ] Git repository initialized
- [ ] Access to ESP32 development board
- [ ] USB cable for ESP32
- [ ] Completed Day 1 joint session (shared interfaces defined)

---

## Step-by-Step Implementation

### Step 1: Initialize PlatformIO Project

**Location:** `Vehicule_Controller/` directory

**Commands:**
```bash
cd Vehicule_Controller
pio project init --board esp32dev
```

**What this creates:**
- `platformio.ini` - Build configuration file
- `.pio/` directory - Build artifacts (auto-created, not committed)

**Verification:**
```bash
ls -la platformio.ini  # Should exist
```

---

### Step 2: Configure `platformio.ini`

**File:** `Vehicule_Controller/platformio.ini`

**Complete configuration:**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

; FreeRTOS configuration
; Note: FreeRTOS is included by default in ESP32 Arduino framework
; We configure it via build flags

; Build flags for FreeRTOS and debugging
build_flags = 
    -DFREERTOS_ENABLED
    -DCONFIG_FREERTOS_HZ=1000
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM

; Libraries (add as needed)
lib_deps = 
    ; ESP32 libraries are built-in
    ; External libraries go here if needed later

; Upload settings
upload_speed = 921600
upload_port = /dev/ttyUSB0  ; Adjust for your system (COM port on Windows)

; Monitor settings (Serial debugging)
monitor_speed = 115200
monitor_filters = 
    time
    default

; Build options
build_type = release
```

**Platform-specific notes:**
- **Linux:** `upload_port = /dev/ttyUSB0` or `/dev/ttyACM0`
- **Windows:** `upload_port = COM3` (check Device Manager)
- **macOS:** `upload_port = /dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`

**Verification:**
```bash
pio run  # Should compile without errors (even with empty main.cpp)
```

---

### Step 3: Create Directory Structure

**Commands:**
```bash
cd Vehicule_Controller

# Create all source directories
mkdir -p src/hal
mkdir -p src/drivers
mkdir -p src/tasks
mkdir -p src/communication
mkdir -p src/control
mkdir -p src/utils
mkdir -p src/shared

# Create test directories
mkdir -p tests/unit
mkdir -p tests/integration

# Create documentation directory
mkdir -p docs
```

**Complete directory tree:**
```
Vehicule_Controller/
├── platformio.ini              # Build configuration
├── .gitignore                  # Git ignore rules
├── README.md                   # Project documentation
├── src/                        # Source code root
│   ├── main.cpp               # Entry point (FreeRTOS init)
│   ├── config.h               # Build-time configuration
│   ├── hal/                   # Hardware Abstraction Layer
│   │   ├── hal_pwm.h
│   │   ├── hal_pwm.cpp
│   │   ├── hal_gpio.h
│   │   ├── hal_gpio.cpp
│   │   ├── hal_timer.h
│   │   └── hal_timer.cpp
│   ├── drivers/               # Device drivers (use HAL)
│   │   ├── motor_driver.h
│   │   ├── motor_driver.cpp
│   │   ├── ultrasonic_driver.h
│   │   ├── ultrasonic_driver.cpp
│   │   ├── servo_driver.h
│   │   └── servo_driver.cpp
│   ├── tasks/                 # FreeRTOS tasks
│   │   ├── task_motor_control.cpp
│   │   ├── task_communication.cpp
│   │   ├── task_sensor_fusion.cpp
│   │   ├── task_safety_monitor.cpp
│   │   └── task_telemetry.cpp
│   ├── communication/         # Communication protocols
│   │   ├── protocol_binary.h
│   │   ├── protocol_binary.cpp
│   │   ├── espnow_handler.h
│   │   ├── espnow_handler.cpp
│   │   └── protocol_types.h
│   ├── control/               # Control algorithms (Person 2)
│   │   ├── mecanum_kinematics.h
│   │   ├── mecanum_kinematics.cpp
│   │   ├── pid_controller.h
│   │   ├── pid_controller.cpp
│   │   ├── motion_planner.h
│   │   └── motion_planner.cpp
│   ├── utils/                 # Utility functions
│   │   ├── logger.h
│   │   └── logger.cpp
│   └── shared/                # Shared data structures
│       ├── types.h            # BodyVelocity, WheelVelocities, etc.
│       └── queues.h           # FreeRTOS queue definitions
├── tests/                     # Unit tests
│   ├── unit/
│   │   ├── test_kinematics.cpp
│   │   └── test_pid.cpp
│   └── integration/
│       └── test_hil.cpp
└── docs/                      # Documentation
    ├── ARCHITECTURE.md
    └── TASK_1_PROJECT_SETUP.md
```

**Verification:**
```bash
tree src/ -d  # or: find src/ -type d
```

---

### Step 4: Create `src/config.h`

**File:** `Vehicule_Controller/src/config.h`

**Complete configuration:**
```cpp
#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Hardware Pin Definitions
// ============================================================================
// Based on existing Vehicule_Controller.ino pin assignments

// Front Right Motor (L298N Driver #1)
#define FRONT_RIGHT_IN1    35
#define FRONT_RIGHT_IN2    32
#define FRONT_RIGHT_ENA    34  // PWM channel

// Front Left Motor (L298N Driver #1)
#define FRONT_LEFT_IN3     33
#define FRONT_LEFT_IN4     25
#define FRONT_LEFT_ENA     34  // Shared PWM with FR

// Back Right Motor (L298N Driver #2)
#define BACK_RIGHT_IN1     27
#define BACK_RIGHT_IN2     14
#define BACK_RIGHT_ENA     26  // PWM channel

// Back Left Motor (L298N Driver #2)
#define BACK_LEFT_IN3      12
#define BACK_LEFT_IN4      13
#define BACK_LEFT_ENA      26  // Shared PWM with BR

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
#define MOTOR_PWM_MAX      1023 // Maximum PWM value for HAL

// ============================================================================
// FreeRTOS Task Priorities
// ============================================================================
// Higher number = higher priority
// Range: 0-25 (configMAX_PRIORITIES)
#define TASK_PRIORITY_SAFETY_MONITOR   5  // Highest - safety critical
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
#define COMMAND_TIMEOUT_MS         500

// ============================================================================
// Safety Settings
// ============================================================================
#define WATCHDOG_TIMEOUT_MS        5000   // 5 second watchdog
#define EMERGENCY_STOP_DISTANCE_CM 10    // Stop if obstacle < 10cm
#define COMMAND_TIMEOUT_MS         500   // Auto-stop if no command

// ============================================================================
// Kinematic Parameters (shared with Person 2)
// ============================================================================
// These will be used by Person 2's kinematics code
// Update with actual measurements from your robot
#define WHEEL_RADIUS_M             0.05f   // 5cm radius (adjust to actual)
#define WHEEL_BASE_M               0.20f   // 20cm front-to-back (adjust)
#define TRACK_WIDTH_M              0.18f   // 18cm left-to-right (adjust)

#endif // CONFIG_H
```

**Notes:**
- Pin definitions match your existing `Vehicule_Controller.ino`
- Task priorities and periods are defined for future use
- Kinematic parameters are placeholders (measure your actual robot)

---

### Step 5: Create Basic `src/main.cpp`

**File:** `Vehicule_Controller/src/main.cpp`

**Initial skeleton:**
```cpp
/**
 * @file main.cpp
 * @brief Main entry point for ESP32 Vehicle Controller
 * @details FreeRTOS-based multi-task system for gesture-controlled mecanum car
 */

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

// Configuration
#include "config.h"

// Forward declarations (will be implemented in later tasks)
void task_motor_control(void *pvParameters);
void task_communication(void *pvParameters);
void task_sensor_fusion(void *pvParameters);
void task_safety_monitor(void *pvParameters);
void task_telemetry(void *pvParameters);

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
    
    // TODO: Task 1.2 - Initialize HAL layer
    // TODO: Task 1.3 - Initialize MotorDriver
    // TODO: Task 1.4 - Create FreeRTOS tasks
    
    Serial.println("[SETUP] System skeleton initialized");
    Serial.println("[SETUP] Tasks will be created in Task 1.4");
    Serial.println("[SETUP] Ready for HAL implementation\n");
}

void loop() {
    // Empty - FreeRTOS tasks handle everything
    // In a FreeRTOS setup, loop() should not contain blocking code
    // All work is done in tasks created in setup()
    
    // This delay ensures loop() doesn't consume CPU
    // In production, you might remove loop() entirely or use it for
    // low-priority background tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Optional: Print free heap periodically for debugging
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        Serial.println("[LOOP] Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
        lastPrint = millis();
    }
}
```

**Why this structure:**
- Includes FreeRTOS headers
- Shows system information on startup
- Provides placeholders for future tasks
- Demonstrates FreeRTOS pattern (tasks replace `loop()`)

---

### Step 6: Create `.gitignore`

**File:** `Vehicule_Controller/.gitignore`

**Complete `.gitignore`:**
```gitignore
# PlatformIO
.pio/
.vscode/
.clang_complete
.gcc-flags.json
compile_commands.json

# Build artifacts
.pioenvs/
.piolibdeps/
*.o
*.elf
*.bin
*.hex
*.map
*.lst

# IDE
.idea/
*.swp
*.swo
*~
.vscode/
*.code-workspace

# OS
.DS_Store
Thumbs.db
desktop.ini

# Logs
*.log
*.tmp

# Python (if using Python scripts)
__pycache__/
*.pyc
*.pyo
*.pyd
.Python
env/
venv/

# Backup files
*.bak
*.backup
*~
```

**Verification:**
```bash
git status  # Should not show .pio/, build files, etc.
```

---

### Step 7: Set Up Git Branches

**Commands:**
```bash
# Ensure you're in the repository root
cd /home/yaniv/.cursor/worktrees/final-project-gesture-car/mlg

# Create feature branches
git checkout -b person1/infrastructure
git checkout -b person2/control
git checkout -b integration/phase1-2

# Return to main branch
git checkout main

# Verify branches
git branch -a
```

**Branch Strategy:**

| Branch | Purpose | Who Works On It |
|--------|---------|-----------------|
| `main` | Stable, working code | Both (via PRs) |
| `person1/infrastructure` | Person 1's Phase 1 work | Person 1 |
| `person2/control` | Person 2's Phase 2 work | Person 2 |
| `integration/phase1-2` | Integration testing | Both (joint work) |

**Workflow:**
1. Person 1 works on `person1/infrastructure`
2. Person 2 works on `person2/control`
3. Both merge to `integration/phase1-2` for testing
4. Once stable, merge to `main`

---

### Step 8: Create Initial `README.md` for Vehicle Controller

**File:** `Vehicule_Controller/README.md`

**Initial README:**
```markdown
# ESP32 Vehicle Controller

Advanced FreeRTOS-based control system for gesture-controlled mecanum wheel robot.

## Architecture

- **FreeRTOS**: Multi-task real-time operating system
- **HAL Layer**: Hardware abstraction for portability
- **Modular Design**: Separated into drivers, tasks, control, communication

## Building

```bash
# Install dependencies (PlatformIO)
pip install platformio

# Build project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Project Structure

See `docs/ARCHITECTURE.md` for detailed architecture documentation.

## Development Status

- [x] Task 1.1: Project structure setup
- [ ] Task 1.2: HAL layer implementation
- [ ] Task 1.3: MotorDriver implementation
- [ ] Task 1.4: FreeRTOS task structure
- [ ] Task 1.5: Binary protocol
- [ ] Task 1.6: ESP-NOW integration

## Contributors

- Person 1: Infrastructure, HAL, Communication
- Person 2: Control algorithms, Kinematics, PID
```

---

## Verification Checklist

Before moving to Task 1.2, verify:

- [ ] **PlatformIO Setup**
  - [ ] `platformio.ini` exists and configures ESP32 correctly
  - [ ] `pio run` compiles without errors
  - [ ] `pio run --target upload` can upload to ESP32 (optional test)

- [ ] **Directory Structure**
  - [ ] All directories created (`hal/`, `drivers/`, `tasks/`, etc.)
  - [ ] Directory structure matches architecture diagram

- [ ] **Source Files**
  - [ ] `src/main.cpp` exists and compiles
  - [ ] `src/config.h` exists with all pin definitions
  - [ ] Serial output shows system information

- [ ] **Version Control**
  - [ ] `.gitignore` excludes build artifacts
  - [ ] Git branches created (`person1/infrastructure`, etc.)
  - [ ] Initial commit made to `person1/infrastructure` branch

- [ ] **Documentation**
  - [ ] `README.md` created in `Vehicule_Controller/`
  - [ ] This task documentation file created

---

## Testing Your Setup

### Test 1: Compilation
```bash
cd Vehicule_Controller
pio run
```

**Expected output:**
```
Building in release mode
...
Successfully created the .elf file
RAM:   [====      ]  40.0% (used 131072 bytes from 327680 bytes)
Flash: [==        ]  20.0% (used 262144 bytes from 1310720 bytes)
```

### Test 2: Serial Output
```bash
pio run --target upload
pio device monitor
```

**Expected Serial output:**
```
========================================
Gesture Car - ESP32 Vehicle Controller
Phase 1: Infrastructure Setup
========================================
FreeRTOS Version: 10.4.3
CPU Frequency: 240 MHz
Free Heap: 250000 bytes
========================================

[SETUP] System skeleton initialized
[SETUP] Tasks will be created in Task 1.4
[SETUP] Ready for HAL implementation

[LOOP] Free Heap: 250000 bytes
```

### Test 3: Directory Structure
```bash
tree src/ -L 2
```

**Expected:**
```
src/
├── main.cpp
├── config.h
├── hal/
├── drivers/
├── tasks/
├── communication/
├── control/
├── utils/
└── shared/
```

---

## Common Issues & Solutions

### Issue 1: PlatformIO Command Not Found

**Error:**
```bash
pio: command not found
```

**Solution:**
```bash
# Install PlatformIO
pip install platformio

# Or use pip3
pip3 install platformio

# Verify installation
pio --version
```

### Issue 2: ESP32 Board Not Found

**Error:**
```
Error: Unknown board ID 'esp32dev'
```

**Solution:**
```bash
# Update platform
pio platform update espressif32

# List available boards
pio boards espressif32
```

### Issue 3: Upload Port Not Found

**Error:**
```
Error: Please specify `upload_port` for environment
```

**Solution:**
1. Find your ESP32 port:
   - **Linux:** `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
   - **Windows:** Check Device Manager → Ports (COM & LPT)
   - **macOS:** `ls /dev/cu.*`

2. Update `platformio.ini`:
   ```ini
   upload_port = /dev/ttyUSB0  # Your actual port
   ```

3. Or specify at upload time:
   ```bash
   pio run --target upload --upload-port /dev/ttyUSB0
   ```

### Issue 4: Compilation Errors

**Error:**
```
fatal error: FreeRTOS.h: No such file or directory
```

**Solution:**
- ESP32 Arduino framework includes FreeRTOS automatically
- Check `platformio.ini` has `framework = arduino`
- Try: `pio platform update espressif32`

### Issue 5: Serial Monitor Not Working

**Error:**
- No output in Serial Monitor

**Solution:**
1. Check baud rate matches: `monitor_speed = 115200`
2. Check `Serial.begin(115200)` in code
3. Try different USB cable/port
4. Check ESP32 is powered on

---

## Next Steps

Once Task 1.1 is complete:

1. **Commit your work:**
   ```bash
   git add .
   git commit -m "Task 1.1: Project structure setup complete"
   git push origin person1/infrastructure
   ```

2. **Notify Person 2:**
   - Share the branch
   - Confirm directory structure matches plan
   - Coordinate on shared interfaces if needed

3. **Move to Task 1.2:**
   - HAL Layer Implementation
   - See `docs/TASK_1_2_HAL.md` (to be created)

---

## Time Estimate

| Activity | Time |
|----------|------|
| PlatformIO setup | 30 min |
| Directory structure | 15 min |
| Config files | 30 min |
| Git branches | 15 min |
| Testing & verification | 30 min |
| Documentation | 30 min |
| **Total** | **~2.5 hours** |

**Buffer time:** Add 1-2 hours for troubleshooting and learning PlatformIO if new to it.

---

## Success Criteria

✅ **Task 1.1 is complete when:**
- Project compiles without errors
- All directories exist
- Git branches are set up
- Serial output shows system information
- Ready to proceed to Task 1.2 (HAL implementation)

---

## Additional Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP32 FreeRTOS Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)

---

**Last Updated:** Task 1.1 Implementation Guide  
**Status:** Ready for implementation  
**Next Task:** Task 1.2 - HAL Layer Implementation

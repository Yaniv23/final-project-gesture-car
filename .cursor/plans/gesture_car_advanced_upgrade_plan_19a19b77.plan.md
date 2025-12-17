---
name: Gesture Car Advanced Upgrade Plan
overview: Transform the functional gesture-controlled mecanum car into a portfolio-grade embedded systems platform with closed-loop control, RTOS architecture, sensor fusion, robust communication protocols, and safety mechanisms suitable for Electrical/Embedded/Robotics Engineering roles.
todos:
  - id: phase1_rtos
    content: "Phase 1: Refactor to FreeRTOS architecture - Create task structure, HAL layer, and basic binary protocol"
    status: pending
  - id: phase2_control
    content: "Phase 2: Implement closed-loop control - Mecanum kinematics, PID controllers, velocity-based motor control"
    status: pending
  - id: phase3_comm
    content: "Phase 3: Upgrade communication stack - Binary protocol with CRC/ACK/retry, enhanced ESP-NOW handler, telemetry"
    status: pending
  - id: phase4_sensors
    content: "Phase 4: Sensor fusion - IMU integration, multi-sensor obstacle detection, gesture confidence scoring"
    status: pending
  - id: phase5_safety
    content: "Phase 5: Safety & modes - Watchdog timer, emergency stop, state machine, autonomous obstacle avoidance"
    status: pending
  - id: phase6_testing
    content: "Phase 6: Testing & validation - Unit tests, HIL testing, performance metrics, documentation"
    status: pending
---

# Gesture-Controlled Car: Advanced Embedded Systems Upgrade Plan

## System-Level Architecture

### Current State Analysis

**Existing Components:**

- PC-side: MediaPipe hand tracking → USB Serial → ESP32 sender
- Communication: ESP-NOW (string-based commands)
- Vehicle Controller: ESP32 with 4-motor mecanum drive, servo, ultrasonic sensor
- Camera: ESP32-S3 MJPEG streaming via WiFi

**Current Limitations:**

- Open-loop motor control (no feedback)
- String-based protocol (no framing, CRC, versioning)
- No real-time guarantees (cooperative scheduling)
- Basic sensor integration (ultrasonic only)
- No error handling or fault recovery
- Monolithic code structure

### Target Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    PERCEPTION LAYER                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Hand Tracker │  │ Camera Feed  │  │ IMU Sensor   │      │
│  │ (PC/MediaPipe)│  │ (ESP32-S3)   │  │ (ESP32)      │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │               │
│         └──────────────────┼──────────────────┘              │
│                            │                                  │
│                    ┌───────▼────────┐                        │
│                    │ Sensor Fusion  │                        │
│                    │ & Confidence   │                        │
│                    └───────┬────────┘                        │
└────────────────────────────┼─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│                    COMMUNICATION LAYER                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Binary Protocol Stack                                 │   │
│  │ - Packet framing (SOF/EOF markers)                   │   │
│  │ - Command ID + payload + CRC16                        │   │
│  │ - Sequence numbers + ACK/NACK                         │   │
│  │ - Version negotiation                                  │   │
│  │ - Retry logic + timeout handling                      │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ USB Serial   │  │ ESP-NOW       │  │ WiFi Debug    │     │
│  │ (PC→Sender)  │  │ (Sender→Car)  │  │ (Telemetry)   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│                    CONTROL LAYER                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ State Machine (FreeRTOS Tasks)                        │   │
│  │ - Manual Mode (gesture control)                       │   │
│  │ - Autonomous Mode (obstacle avoidance)                │   │
│  │ - Calibration Mode                                    │   │
│  │ - Diagnostics Mode                                    │   │
│  │ - Emergency Stop State                                │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Motion Planner                                        │   │
│  │ - Mecanum kinematics (vx, vy, ω) → wheel velocities  │   │
│  │ - Motion primitives (rotate, strafe, arc)            │   │
│  │ - Trajectory following                                │   │
│  └──────────────────────────────────────────────────────┘   │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│                    ACTUATION LAYER                            │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Closed-Loop Motor Control (4x PID controllers)        │   │
│  │ - Velocity setpoints from kinematics                  │   │
│  │ - Encoder feedback (or current sensing)               │   │
│  │ - PWM output with deadband compensation               │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Motor Drivers│  │ Servo Control │  │ Status LEDs   │     │
│  │ (L298N x2)   │  │ (Scanning)    │  │ (Debug)       │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│                    SAFETY LAYER                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Watchdog     │  │ Emergency    │  │ Timeout      │      │
│  │ Timer        │  │ Stop Logic   │  │ Monitoring   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Sensor Fusion Safety                                 │   │
│  │ - Obstacle detection (ultrasonic + vision)           │   │
│  │ - Collision avoidance                                │   │
│  │ - Safe stop zones                                    │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow & Timing Constraints

**Critical Paths:**

1. **Gesture → Motor Response**: < 100ms (PC processing + serial + ESP-NOW + motor update)
2. **Sensor → Safety Stop**: < 50ms (ultrasonic read + decision + motor stop)
3. **Control Loop**: 10ms period (100 Hz PID updates)
4. **Telemetry**: 100ms period (10 Hz status updates)

**Real-Time Requirements:**

- Motor control: Hard real-time (10ms)
- Safety monitoring: Hard real-time (50ms)
- Communication: Soft real-time (100ms)
- Telemetry: Soft real-time (100ms)

---

## Phase 1: Embedded Software Architecture Refactor

### 1.1 FreeRTOS Task Structure

**File Structure:**

```
Vehicule_Controller/
├── src/
│   ├── main.cpp                    # Entry point, FreeRTOS init
│   ├── tasks/
│   │   ├── task_motor_control.cpp  # 10ms motor control loop
│   │   ├── task_sensor_fusion.cpp  # 50ms sensor reading
│   │   ├── task_communication.cpp  # ESP-NOW handler
│   │   ├── task_safety_monitor.cpp # Watchdog + timeout checks
│   │   └── task_telemetry.cpp      # Status reporting
│   ├── drivers/
│   │   ├── motor_driver.cpp        # HAL for L298N
│   │   ├── encoder_driver.cpp      # Quadrature encoder (if added)
│   │   ├── imu_driver.cpp          # MPU6050/BNO055 driver
│   │   ├── ultrasonic_driver.cpp   # HC-SR04 driver
│   │   └── servo_driver.cpp        # Servo abstraction
│   ├── hal/
│   │   ├── hal_pwm.cpp             # PWM abstraction
│   │   ├── hal_gpio.cpp            # GPIO abstraction
│   │   └── hal_timer.cpp           # Timer/WDT abstraction
│   ├── control/
│   │   ├── mecanum_kinematics.cpp  # Forward/inverse kinematics
│   │   ├── pid_controller.cpp      # Generic PID implementation
│   │   ├── motion_planner.cpp      # Trajectory generation
│   │   └── state_machine.cpp       # Mode management
│   ├── communication/
│   │   ├── protocol_binary.cpp     # Packet framing, CRC
│   │   ├── espnow_handler.cpp      # ESP-NOW wrapper
│   │   └── telemetry.cpp           # Status packet generation
│   ├── safety/
│   │   ├── watchdog.cpp            # Hardware watchdog
│   │   ├── emergency_stop.cpp      # E-stop logic
│   │   └── timeout_monitor.cpp     # Command timeout
│   └── utils/
│       ├── logger.cpp               # Structured logging
│       └── config.h                 # Build-time configuration
└── platformio.ini                   # Build system (PlatformIO)
```

**Task Priorities (FreeRTOS):**

- `task_safety_monitor`: Priority 5 (highest)
- `task_motor_control`: Priority 4
- `task_sensor_fusion`: Priority 3
- `task_communication`: Priority 2
- `task_telemetry`: Priority 1 (lowest)

**Engineering Value:**

- Demonstrates RTOS task scheduling and priority management
- Shows understanding of real-time constraints
- Portfolio highlight: "Multi-threaded embedded system with deterministic timing"

### 1.2 Hardware Abstraction Layer (HAL)

**Abstraction Goals:**

- Decouple application logic from hardware pins
- Enable unit testing (mock HAL)
- Support multiple hardware variants

**Example Interface:**

```cpp
// hal/hal_pwm.h
class HAL_PWM {
public:
    virtual void setDutyCycle(uint8_t channel, uint16_t duty) = 0;
    virtual void setFrequency(uint8_t channel, uint32_t freq_hz) = 0;
};

// hal/hal_gpio.h
class HAL_GPIO {
public:
    virtual void setPin(uint8_t pin, bool state) = 0;
    virtual bool readPin(uint8_t pin) = 0;
};
```

**Engineering Value:**

- Demonstrates layered architecture and abstraction
- Shows portability and testability considerations
- Portfolio highlight: "Hardware abstraction enabling test-driven development"

---

## Phase 2: Communication Stack Upgrade

### 2.1 Binary Protocol Design

**Packet Structure:**

```
┌──────┬──────┬──────┬──────────┬──────────┬──────┬──────┐
│ SOF  │ VER  │ CMD  │ SEQ      │ LEN      │ DATA │ CRC  │
│ 0xAA │ 0x01 │ 0xXX │ 0xXXXX   │ 0xXX     │ ...  │ 0xXX │
│ 1B   │ 1B   │ 1B   │ 2B       │ 1B       │ N    │ 2B   │
└──────┴──────┴──────┴──────────┴──────────┴──────┴──────┘
```

**Command Types:**

- `0x01`: Motion command (vx, vy, ω, duration_ms)
- `0x02`: Mode change (manual/autonomous/calibration)
- `0x03`: Calibration request
- `0x04`: Telemetry request
- `0x05`: Emergency stop
- `0x80-0xFF`: ACK/NACK responses

**Features:**

- CRC16-CCITT for error detection
- Sequence numbers for duplicate detection
- ACK/NACK with retry (max 3 attempts)
- Version negotiation on startup
- Timeout handling (500ms default)

**File:** `communication/protocol_binary.cpp`

**Engineering Value:**

- Demonstrates protocol design and error handling
- Shows understanding of communication reliability
- Portfolio highlight: "Robust binary protocol with CRC, ACK, and retry logic"

### 2.2 Enhanced ESP-NOW Handler

**Improvements:**

- Packet fragmentation for large payloads (>250 bytes ESP-NOW limit)
- Connection state tracking (connected/disconnected)
- RSSI monitoring for link quality
- Automatic reconnection logic

**File:** `communication/espnow_handler.cpp`

---

## Phase 3: Control & Robotics Layer

### 3.1 Mecanum Kinematics Library

**Forward Kinematics:**

```
Given: wheel velocities [ω_FL, ω_FR, ω_BL, ω_BR]
Compute: body velocities [vx, vy, ω]
```

**Inverse Kinematics:**

```
Given: desired [vx, vy, ω]
Compute: wheel velocities [ω_FL, ω_FR, ω_BL, ω_BR]
```

**Implementation:**

```cpp
// control/mecanum_kinematics.cpp
struct BodyVelocity {
    float vx;      // m/s forward
    float vy;      // m/s right
    float omega;   // rad/s CCW
};

struct WheelVelocities {
    float front_left;
    float front_right;
    float back_left;
    float back_right;
};

WheelVelocities inverseKinematics(BodyVelocity body, float wheel_radius, float wheel_base, float track_width);
BodyVelocity forwardKinematics(WheelVelocities wheels, float wheel_radius, float wheel_base, float track_width);
```

**File:** `control/mecanum_kinematics.cpp`

**Engineering Value:**

- Demonstrates robotics kinematics understanding
- Shows mathematical modeling skills
- Portfolio highlight: "Implemented forward/inverse kinematics for mecanum drive"

### 3.2 Closed-Loop Motor Control

**PID Controller per Wheel:**

```cpp
// control/pid_controller.cpp
class PIDController {
private:
    float kp, ki, kd;
    float integral, prev_error;
    float output_min, output_max;
public:
    float update(float setpoint, float feedback, float dt);
    void reset();
};
```

**Velocity Control Loop:**

1. Read encoder feedback (or estimate from current/PWM)
2. Compute error: `error = setpoint - feedback`
3. PID update: `pwm = pid.update(error, dt)`
4. Apply PWM with deadband compensation

**Encoder Options:**

- **Option A**: Add quadrature encoders (4x) → true velocity feedback
- **Option B**: Estimate from motor current (current sensing IC)
- **Option C**: Model-based estimation (PWM → velocity mapping with calibration)

**File:** `control/pid_controller.cpp`, `tasks/task_motor_control.cpp`

**Engineering Value:**

- Demonstrates closed-loop control systems
- Shows PID tuning and control theory
- Portfolio highlight: "Implemented 4-channel PID velocity control for mecanum drive"

### 3.3 Motion Primitives

**Primitives:**

- `moveForward(distance_m, speed_mps)`
- `strafeRight(distance_m, speed_mps)`
- `rotate(angle_rad, angular_speed_radps)`
- `arc(radius_m, angle_rad, speed_mps)`
- `followTrajectory(waypoints[])`

**Implementation:**

```cpp
// control/motion_planner.cpp
class MotionPlanner {
public:
    void executePrimitive(MotionPrimitive primitive);
    bool isComplete();
    BodyVelocity getCurrentSetpoint();  // For control loop
};
```

**File:** `control/motion_planner.cpp`

**Engineering Value:**

- Demonstrates motion planning and trajectory generation
- Shows abstraction of complex behaviors
- Portfolio highlight: "High-level motion primitives enabling autonomous navigation"

---

## Phase 4: Sensor Fusion & Perception

### 4.1 IMU Integration

**Hardware:** MPU6050 or BNO055 (I2C)

**Features:**

- Orientation estimation (roll, pitch, yaw)
- Angular velocity for odometry
- Complementary filter or Kalman filter for sensor fusion

**File:** `drivers/imu_driver.cpp`, `control/sensor_fusion.cpp`

**Engineering Value:**

- Demonstrates sensor fusion and filtering
- Shows understanding of IMU data processing
- Portfolio highlight: "IMU-based odometry with complementary filter"

### 4.2 Enhanced Obstacle Detection

**Multi-Sensor Fusion:**

- Ultrasonic (HC-SR04): Range 2-400cm, 50ms update
- Vision (camera): Object detection (optional, PC-side)
- IMU: Collision detection via acceleration spikes

**Fusion Strategy:**

```cpp
// control/sensor_fusion.cpp
struct ObstacleMap {
    float front_distance;
    float left_distance;
    float right_distance;
    float confidence;  // 0.0-1.0 based on sensor agreement
};

ObstacleMap fuseSensors(ultrasonic_data, imu_data, vision_data);
```

**File:** `control/sensor_fusion.cpp`, `tasks/task_sensor_fusion.cpp`

**Engineering Value:**

- Demonstrates multi-sensor data fusion
- Shows confidence scoring and uncertainty handling
- Portfolio highlight: "Sensor fusion combining ultrasonic, IMU, and vision"

### 4.3 Gesture Confidence Scoring

**PC-Side Enhancement:**

- MediaPipe confidence score
- Temporal filtering (moving average)
- Fallback to "Stop" if confidence < threshold

**File:** `Hand_Tracking/Hand_Tracker.py` (enhance existing)

**Engineering Value:**

- Demonstrates robustness in perception systems
- Shows handling of uncertain inputs

---

## Phase 5: Safety & Reliability

### 5.1 Watchdog Timer

**Hardware Watchdog:**

- ESP32 built-in WDT (configurable timeout)
- Task heartbeat monitoring
- Automatic reset on failure

**Implementation:**

```cpp
// safety/watchdog.cpp
class Watchdog {
public:
    void feed();  // Called by each task
    void enable(uint32_t timeout_ms);
};
```

**File:** `safety/watchdog.cpp`, `tasks/task_safety_monitor.cpp`

**Engineering Value:**

- Demonstrates fault tolerance and system reliability
- Shows understanding of watchdog patterns
- Portfolio highlight: "Hardware watchdog with task-level monitoring"

### 5.2 Emergency Stop System

**Triggers:**

- Command timeout (>500ms no command)
- Obstacle too close (<10cm)
- Communication loss (>2s)
- Manual E-stop command

**Behavior:**

- Immediate motor stop (brake)
- Enter safe state
- Flash status LED
- Log event

**File:** `safety/emergency_stop.cpp`

**Engineering Value:**

- Demonstrates safety-critical system design
- Shows fail-safe mechanisms

### 5.3 Timeout & Retry Logic

**Command Timeout:**

- Monitor last command timestamp
- Auto-stop if >500ms elapsed
- Prevents runaway behavior

**Communication Retry:**

- ESP-NOW retry (3 attempts)
- Exponential backoff
- Fallback to safe state

**File:** `safety/timeout_monitor.cpp`, `communication/espnow_handler.cpp`

---

## Phase 6: Autonomous & Hybrid Modes

### 6.1 State Machine

**States:**

- `MANUAL`: Gesture control active
- `AUTONOMOUS`: Obstacle avoidance + waypoint following
- `CALIBRATION`: Motor/IMU calibration mode
- `DIAGNOSTICS`: Self-test and sensor validation
- `EMERGENCY_STOP`: Safety state

**Transitions:**

- Manual ↔ Autonomous (command or gesture)
- Any → Emergency Stop (safety trigger)
- Calibration (special command)

**File:** `control/state_machine.cpp`

**Engineering Value:**

- Demonstrates state machine design
- Shows mode management and transitions
- Portfolio highlight: "State machine architecture supporting multiple operational modes"

### 6.2 Simple Obstacle Avoidance

**Algorithm:**

1. Read sensor fusion output
2. If obstacle detected:

   - Stop forward motion
   - Rotate to find clear path
   - Resume motion

3. If no obstacle: continue current trajectory

**File:** `control/obstacle_avoidance.cpp`

**Engineering Value:**

- Demonstrates reactive navigation
- Shows integration of perception and control

### 6.3 Calibration Mode

**Features:**

- Motor deadband calibration (find minimum PWM for motion)
- IMU zero-point calibration
- Encoder calibration (if added)
- Kinematic parameter tuning (wheel radius, base, track)

**File:** `control/calibration.cpp`

**Engineering Value:**

- Demonstrates system calibration and parameter tuning
- Shows understanding of manufacturing variations

---

## Phase 7: Testing & Validation

### 7.1 Unit Testing Strategy

**PC-Side Tests:**

- Kinematics: Forward/inverse consistency
- PID controller: Step response, stability
- Protocol: Packet encoding/decoding, CRC validation

**Embedded Tests:**

- HAL mocks for unit testing
- FreeRTOS task isolation testing
- State machine transition validation

**Tools:**

- PC: Google Test (C++) or pytest (Python)
- Embedded: Unity (C unit testing framework)

**File Structure:**

```
tests/
├── unit/
│   ├── test_kinematics.cpp
│   ├── test_pid.cpp
│   ├── test_protocol.cpp
│   └── test_state_machine.cpp
└── integration/
    └── test_hardware_in_loop.cpp
```

**Engineering Value:**

- Demonstrates test-driven development
- Shows quality assurance practices
- Portfolio highlight: "Comprehensive unit and integration test suite"

### 7.2 Hardware-in-the-Loop (HIL) Testing

**Setup:**

- PC simulator sends commands
- ESP32 runs real firmware
- Log motor outputs and sensor inputs
- Validate timing and correctness

**File:** `tests/integration/test_hil.py`

**Engineering Value:**

- Demonstrates HIL testing methodology
- Shows validation of real-time constraints

### 7.3 Performance Metrics

**Measure:**

- Gesture → motor latency (target: <100ms)
- Control loop jitter (target: <1ms)
- Communication packet loss rate
- Motor response time (step input)
- FPS of camera stream

**File:** `utils/performance_monitor.cpp`

**Engineering Value:**

- Demonstrates performance analysis
- Shows quantitative validation

---

## Phase 8: Documentation & Portfolio Presentation

### 8.1 Technical Documentation

**Documents:**

- `docs/ARCHITECTURE.md`: System architecture overview
- `docs/API.md`: Driver and control API reference
- `docs/PROTOCOL.md`: Communication protocol specification
- `docs/CALIBRATION.md`: Calibration procedures
- `docs/TESTING.md`: Testing strategy and results

### 8.2 GitHub Presentation

**Repository Structure:**

- Clean, modular code organization
- Comprehensive README with architecture diagram
- CI/CD badges (if applicable)
- Demo videos (gesture control, autonomous mode)

**README Sections:**

- System architecture (with diagram)
- Hardware requirements
- Build instructions
- Calibration guide
- Performance metrics
- Engineering highlights

### 8.3 CV/Resume Points

**Key Phrases:**

- "Multi-threaded embedded system with FreeRTOS"
- "Closed-loop PID control for 4-motor mecanum drive"
- "Robust binary communication protocol with CRC and ACK"
- "Sensor fusion combining ultrasonic, IMU, and vision"
- "State machine architecture with safety-critical design"
- "Hardware abstraction layer enabling test-driven development"

**Target Roles:**

- Embedded Software Engineer
- Robotics Engineer
- Control Systems Engineer
- Firmware Engineer

---

## Implementation Phases (Recommended Order)

### Phase 1: Foundation (Weeks 1-2)

1. Refactor to FreeRTOS task structure
2. Implement HAL layer
3. Basic binary protocol (no ACK yet)

### Phase 2: Control (Weeks 3-4)

1. Mecanum kinematics library
2. PID controller implementation
3. Closed-loop motor control (with encoder or estimation)

---

## Two-Person Work Split: Phase 1 & Phase 2

### Quick Reference Summary

| Person | Role | Primary Focus | Key Deliverables | Timeline |

|--------|------|--------------|------------------|----------|

| **Person 1** | Infrastructure/Embedded | FreeRTOS, HAL, Communication | HAL layer, MotorDriver, Binary protocol, ESP-NOW | Days 1-10 |

| **Person 2** | Control/Robotics | Control Algorithms, Kinematics | Kinematics, PID, Motor control task, Motion primitives | Days 1-10 |

### Overview

**Person 1 (Infrastructure/Embedded Lead)**:

- **Phase 1 Focus**: FreeRTOS Architecture, HAL Layer, Communication Stack
- **Skills Demonstrated**: RTOS, embedded systems architecture, protocol design
- **Dependencies**: None (can start immediately)

**Person 2 (Control/Robotics Lead)**:

- **Phase 2 Focus**: Control Algorithms, Kinematics, PID Control
- **Skills Demonstrated**: Control systems, robotics kinematics, motion planning
- **Dependencies**: Needs HAL interfaces (Day 3), MotorDriver (Day 4)

### Critical Coordination Points

| Day | Event | Person 1 Action | Person 2 Action | Deliverable |

|-----|-------|-----------------|-----------------|-------------|

| **Day 1** | Joint Session (2 hours) | Define HAL interfaces | Review kinematics requirements | Shared interface headers |

| **Day 3** | Interface Handoff | Deliver HAL `.h` files | Start kinematics (no dependencies) | `hal_pwm.h`, `hal_gpio.h` |

| **Day 4** | MotorDriver Ready | Deliver MotorDriver interface | Design PID interface | `motor_driver.h` |

| **Day 5** | Mock Integration | Provide mock HAL for testing | Test control algorithms with mocks | Mock implementations |

| **Day 8** | Integration Start | Ready for Person 2's code | Connect control to infrastructure | Integration branch |

| **Day 10** | Final Integration | Joint debugging & testing | Joint debugging & testing | Working end-to-end system |

### Parallel Work Opportunities

**Can Work in Parallel (Days 1-3):**

- Person 1: Project setup, HAL design
- Person 2: Kinematics math, PID algorithm design, unit test framework

**Requires Coordination (Days 4-10):**

- Person 1: MotorDriver interface → Person 2 uses it
- Person 2: Control task implementation → Person 1 integrates
- Both: Joint integration and testing

---

### Quick-Start Guide: Person 1 (Infrastructure Lead)

**Your First Steps (Day 1, after joint session):**

1. **Set up PlatformIO project** (Task 1.1)
   ```bash
   cd Vehicule_Controller
   pio project init --board esp32dev
   ```

2. **Create directory structure**
   ```bash
   mkdir -p src/{hal,drivers,tasks,communication,utils,shared}
   mkdir -p tests/unit
   ```

3. **Start with HAL_PWM** (Task 1.2)

   - Create abstract interface first
   - Then ESP32 implementation
   - Test with LED blink

4. **Share interfaces early** (Day 3)

   - Commit HAL headers to shared branch
   - Person 2 needs these to start

**Key Files You Own:**

- All files in `src/hal/`
- All files in `src/drivers/` (except encoder_driver if Person 2 adds it)
- All files in `src/communication/`
- `src/tasks/task_communication.cpp`
- `src/tasks/task_safety_monitor.cpp`
- `src/tasks/task_telemetry.cpp`
- `src/main.cpp` (FreeRTOS setup)

**Files You'll Integrate With:**

- `src/tasks/task_motor_control.cpp` (Person 2 implements, you provide skeleton)
- `src/control/` (Person 2 owns, but you may read for telemetry)

---

### Quick-Start Guide: Person 2 (Control/Robotics Lead)

**Your First Steps (Day 1, after joint session):**

1. **Set up unit testing framework** (can do immediately)
   ```bash
   # Add Unity testing framework to PlatformIO
   # Or use Google Test on PC for kinematics tests
   ```

2. **Start with kinematics** (Task 2.1)

   - Pure math, no dependencies
   - Write forward/inverse functions
   - Create unit tests
   - Validate with known test cases

3. **Design PID controller** (Task 2.2)

   - Can start immediately
   - Write unit tests
   - Test step response on PC

4. **Wait for Person 1's interfaces** (Day 3-4)

   - Once HAL headers available, create mocks
   - Test your control code with mocks

**Key Files You Own:**

- All files in `src/control/`
- `src/tasks/task_motor_control.cpp` (full implementation)
- `src/tasks/task_sensor_fusion.cpp` (if you add sensor reading)
- `tests/unit/test_kinematics.cpp`
- `tests/unit/test_pid.cpp`

**Files You'll Use (from Person 1):**

- `src/hal/hal_pwm.h` - PWM control
- `src/hal/hal_gpio.h` - GPIO control
- `src/drivers/motor_driver.h` - Motor interface
- `src/communication/protocol_types.h` - Command structures

---

### Person 1: Phase 1 Tasks (Infrastructure Lead)

#### Task 1.1: Project Structure Setup (Day 1-2)

**Deliverables:**

- Create PlatformIO project structure
- Set up build system (`platformio.ini`)
- Create directory structure matching architecture
- Set up version control branches

**Files to Create:**

```
Vehicule_Controller/
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── config.h
│   ├── hal/
│   ├── drivers/
│   ├── tasks/
│   ├── communication/
│   └── utils/
└── .gitignore
```

**Acceptance Criteria:**

- Project compiles with PlatformIO
- All directories created
- Basic `main.cpp` with `setup()` and `loop()` compiles

---

#### Task 1.2: HAL Layer Implementation (Day 2-4)

**Deliverables:**

- HAL_PWM class (abstract + ESP32 implementation)
- HAL_GPIO class (abstract + ESP32 implementation)
- HAL_Timer class (for timing/WDT)

**Files:**

- `src/hal/hal_pwm.h` / `hal_pwm.cpp`
- `src/hal/hal_gpio.h` / `hal_gpio.cpp`
- `src/hal/hal_timer.h` / `hal_timer.cpp`
- `src/hal/hal_esp32_pwm.cpp` (implementation)
- `src/hal/hal_esp32_gpio.cpp` (implementation)

**Interface Definition (Share with Person 2):**

```cpp
// hal/hal_pwm.h - SHARED INTERFACE
class HAL_PWM {
public:
    virtual ~HAL_PWM() = default;
    virtual bool init(uint8_t channel, uint8_t pin, uint32_t freq_hz) = 0;
    virtual void setDutyCycle(uint8_t channel, uint16_t duty) = 0;  // 0-1023
    virtual void setFrequency(uint8_t channel, uint32_t freq_hz) = 0;
};

// hal/hal_gpio.h - SHARED INTERFACE
class HAL_GPIO {
public:
    virtual ~HAL_GPIO() = default;
    virtual void setPin(uint8_t pin, bool state) = 0;
    virtual bool readPin(uint8_t pin) = 0;
    virtual void setPinMode(uint8_t pin, uint8_t mode) = 0;  // INPUT/OUTPUT
};
```

**Acceptance Criteria:**

- HAL interfaces compile
- ESP32 implementations work (test with LED blink)
- Unit test mocks can be created (for Person 2)

---

#### Task 1.3: Motor Driver Abstraction (Day 3-4)

**Deliverables:**

- MotorDriver class using HAL
- Supports 4 motors (FL, FR, BL, BR)
- Direction and speed control

**Files:**

- `src/drivers/motor_driver.h` / `motor_driver.cpp`

**Interface:**

```cpp
// drivers/motor_driver.h - SHARED INTERFACE
class MotorDriver {
public:
    struct MotorConfig {
        uint8_t in1_pin, in2_pin, ena_pin;
        uint8_t pwm_channel;
    };
    
    bool init(const MotorConfig motors[4]);
    void setMotorSpeed(uint8_t motor_id, int16_t speed);  // -1023 to +1023
    void stopAll();
};
```

**Acceptance Criteria:**

- Motors can be controlled via MotorDriver
- Works with existing L298N hardware
- Person 2 can use this interface

---

#### Task 1.4: FreeRTOS Task Structure (Day 4-6)

**Deliverables:**

- Main task creation in `main.cpp`
- Task skeletons for:
  - `task_motor_control` (10ms period, priority 4)
  - `task_sensor_fusion` (50ms period, priority 3)
  - `task_communication` (100ms period, priority 2)
  - `task_safety_monitor` (50ms period, priority 5)
  - `task_telemetry` (100ms period, priority 1)

**Files:**

- `src/main.cpp` (FreeRTOS init, task creation)
- `src/tasks/task_motor_control.cpp` (skeleton)
- `src/tasks/task_sensor_fusion.cpp` (skeleton)
- `src/tasks/task_communication.cpp` (skeleton)
- `src/tasks/task_safety_monitor.cpp` (skeleton)
- `src/tasks/task_telemetry.cpp` (skeleton)

**Task Template:**

```cpp
// tasks/task_motor_control.cpp
void task_motor_control(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(10);  // 10ms
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (1) {
        // TODO: Person 2 will implement control logic here
        vTaskDelayUntil(&lastWakeTime, period);
    }
}
```

**Acceptance Criteria:**

- All tasks run with correct periods
- No crashes or watchdog resets
- Task priorities verified (safety_monitor highest)

---

#### Task 1.5: Basic Binary Protocol (Day 5-7)

**Deliverables:**

- Packet structure definition
- Encoding/decoding functions
- CRC16-CCITT implementation
- Basic framing (SOF/EOF)

**Files:**

- `src/communication/protocol_binary.h` / `protocol_binary.cpp`
- `src/communication/protocol_types.h` (shared data structures)

**Packet Structure:**

```cpp
// communication/protocol_types.h - SHARED
#define PROTOCOL_SOF 0xAA
#define PROTOCOL_VERSION 0x01

struct MotionCommand {
    float vx;      // m/s forward
    float vy;      // m/s right
    float omega;   // rad/s CCW
    uint16_t duration_ms;
};

struct ProtocolPacket {
    uint8_t sof;
    uint8_t version;
    uint8_t cmd_id;
    uint16_t seq;
    uint8_t len;
    uint8_t data[32];
    uint16_t crc;
};
```

**Functions:**

```cpp
// communication/protocol_binary.h - SHARED
bool encodePacket(const uint8_t* data, uint8_t len, uint8_t cmd_id, 
                  uint16_t seq, uint8_t* buffer, uint16_t* out_len);
bool decodePacket(const uint8_t* buffer, uint16_t len, ProtocolPacket* packet);
uint16_t calculateCRC16(const uint8_t* data, uint16_t len);
```

**Acceptance Criteria:**

- Can encode/decode packets
- CRC detects errors
- Works with existing ESP-NOW (replace string commands)

---

#### Task 1.6: ESP-NOW Integration (Day 7-8)

**Deliverables:**

- Update ESP-NOW handler to use binary protocol
- Replace string commands with binary packets
- Basic receive/send functionality

**Files:**

- `src/communication/espnow_handler.cpp` (update existing)

**Acceptance Criteria:**

- Binary packets sent/received via ESP-NOW
- Replaces existing string-based system
- Works with Person 2's motion commands

---

#### Task 1.7: Integration & Testing (Day 9-10)

**Deliverables:**

- Integration with Person 2's control code
- End-to-end test (gesture → motor)
- Bug fixes and refinement

**Acceptance Criteria:**

- System works end-to-end
- No crashes or timing issues
- Documentation updated

---

### Person 2: Phase 2 Tasks (Control/Robotics Lead)

#### Task 2.1: Mecanum Kinematics Library (Day 1-3)

**Deliverables:**

- Forward kinematics (wheel velocities → body velocity)
- Inverse kinematics (body velocity → wheel velocities)
- Unit tests for consistency

**Files:**

- `src/control/mecanum_kinematics.h` / `mecanum_kinematics.cpp`
- `tests/unit/test_kinematics.cpp`

**Implementation:**

```cpp
// control/mecanum_kinematics.h - SHARED
struct BodyVelocity {
    float vx;      // m/s forward
    float vy;      // m/s right
    float omega;   // rad/s CCW
};

struct WheelVelocities {
    float front_left;   // rad/s
    float front_right;
    float back_left;
    float back_right;
};

struct KinematicParams {
    float wheel_radius;    // meters
    float wheel_base;       // meters (front-to-back wheel distance)
    float track_width;      // meters (left-to-right wheel distance)
};

WheelVelocities inverseKinematics(const BodyVelocity& body, 
                                   const KinematicParams& params);
BodyVelocity forwardKinematics(const WheelVelocities& wheels,
                                const KinematicParams& params);
```

**Mathematical Model:**

```
For mecanum wheels:
v_FL = vx - vy - ω*(L+W)/2
v_FR = vx + vy + ω*(L+W)/2
v_BL = vx + vy - ω*(L+W)/2
v_BR = vx - vy + ω*(L+W)/2

Where L = wheel_base, W = track_width
```

**Acceptance Criteria:**

- Forward/inverse are consistent (round-trip test)
- Unit tests pass
- Works with typical mecanum parameters

---

#### Task 2.2: PID Controller Implementation (Day 3-5)

**Deliverables:**

- Generic PID controller class
- Anti-windup protection
- Output clamping
- Reset functionality

**Files:**

- `src/control/pid_controller.h` / `pid_controller.cpp`
- `tests/unit/test_pid.cpp`

**Implementation:**

```cpp
// control/pid_controller.h - SHARED
class PIDController {
private:
    float kp_, ki_, kd_;
    float integral_;
    float prev_error_;
    float output_min_, output_max_;
    bool anti_windup_enabled_;
    
public:
    PIDController(float kp, float ki, float kd, 
                  float output_min, float output_max);
    
    float update(float setpoint, float feedback, float dt);
    void reset();
    void setGains(float kp, float ki, float kd);
};
```

**PID Algorithm:**

```
error = setpoint - feedback
integral += error * dt
derivative = (error - prev_error) / dt
output = kp*error + ki*integral + kd*derivative
output = clamp(output, output_min, output_max)
```

**Acceptance Criteria:**

- Step response test passes
- Anti-windup works correctly
- Unit tests validate behavior

---

#### Task 2.3: Velocity Estimation (Day 4-5)

**Deliverables:**

- Velocity estimation from PWM (if no encoders)
- OR encoder driver (if encoders available)
- Filtering for smooth feedback

**Option A: PWM-Based Estimation (No Encoders)**

```cpp
// control/velocity_estimator.h
class VelocityEstimator {
private:
    float pwm_to_velocity_map_[1024];  // Calibrated mapping
    float filter_alpha_;  // Low-pass filter coefficient
    
public:
    float estimateVelocity(uint16_t pwm_value, float dt);
    void calibrate(float pwm, float measured_velocity);
};
```

**Option B: Encoder Driver (If Encoders Added)**

```cpp
// drivers/encoder_driver.h
class EncoderDriver {
public:
    bool init(uint8_t pin_a, uint8_t pin_b);
    int32_t getCount();  // Total count
    float getVelocity(float dt);  // Counts per second → rad/s
    void reset();
};
```

**Acceptance Criteria:**

- Provides velocity feedback for PID
- Smooth, filtered output
- Works with PID controller

---

#### Task 2.4: Motor Control Task Implementation (Day 5-7)

**Deliverables:**

- Implement `task_motor_control` with PID loops
- 4x PID controllers (one per wheel)
- Integration with kinematics
- 10ms control loop

**Files:**

- `src/tasks/task_motor_control.cpp` (implement)

**Implementation Flow:**

```cpp
void task_motor_control(void *pvParameters) {
    // Initialize 4x PID controllers
    PIDController pid_fl(kp, ki, kd, -1023, 1023);
    PIDController pid_fr(kp, ki, kd, -1023, 1023);
    PIDController pid_bl(kp, ki, kd, -1023, 1023);
    PIDController pid_br(kp, ki, kd, -1023, 1023);
    
    // Get body velocity setpoint (from communication task)
    BodyVelocity setpoint = getCurrentSetpoint();  // From shared memory/queue
    
    // Inverse kinematics: body → wheel velocities
    WheelVelocities wheel_setpoints = inverseKinematics(setpoint, params);
    
    // Read feedback (from encoders or estimator)
    WheelVelocities wheel_feedback = getWheelVelocities();
    
    // PID update for each wheel
    float pwm_fl = pid_fl.update(wheel_setpoints.front_left, 
                                  wheel_feedback.front_left, dt);
    // ... repeat for FR, BL, BR
    
    // Apply PWM via MotorDriver
    motor_driver.setMotorSpeed(MOTOR_FL, pwm_fl);
    // ... repeat for other motors
}
```

**Acceptance Criteria:**

- 10ms loop period maintained
- PID controllers stabilize
- Motors respond to setpoint changes
- No oscillations or instability

---

#### Task 2.5: Motion Primitives (Day 6-8)

**Deliverables:**

- High-level motion commands
- Trajectory generation
- Integration with control loop

**Files:**

- `src/control/motion_planner.h` / `motion_planner.cpp`

**Primitives:**

```cpp
// control/motion_planner.h - SHARED
enum class MotionPrimitive {
    MOVE_FORWARD,
    STRAFE_RIGHT,
    ROTATE,
    ARC,
    STOP
};

class MotionPlanner {
private:
    MotionPrimitive current_primitive_;
    float progress_;  // 0.0 to 1.0
    BodyVelocity current_setpoint_;
    
public:
    void executePrimitive(MotionPrimitive primitive, 
                          float param1, float param2);
    bool isComplete();
    BodyVelocity getCurrentSetpoint();  // Called by control task
    void update(float dt);
};
```

**Acceptance Criteria:**

- Can execute forward, strafe, rotate motions
- Smooth setpoint generation
- Works with control loop

---

#### Task 2.6: Integration with Person 1's Infrastructure (Day 8-9)

**Deliverables:**

- Connect control code to HAL/MotorDriver
- Test with real hardware
- Tune PID gains

**Integration Points:**

- Use Person 1's `MotorDriver` interface
- Use Person 1's binary protocol for commands
- Use Person 1's FreeRTOS task structure

**Acceptance Criteria:**

- End-to-end control works
- PID gains tuned (no oscillations)
- Smooth motion achieved

---

#### Task 2.7: Testing & Documentation (Day 9-10)

**Deliverables:**

- Unit tests for kinematics and PID
- Performance measurements (step response, settling time)
- Documentation of control algorithms

**Acceptance Criteria:**

- All tests pass
- Performance metrics documented
- Code reviewed and clean

---

### Shared Interfaces & Data Structures (Define Together on Day 1)

**File: `src/shared/types.h`** - Must be agreed upon by both people

```cpp
// SHARED: Motion command structure (used by Person 1's protocol, Person 2's control)
struct BodyVelocity {
    float vx;      // m/s forward
    float vy;      // m/s right  
    float omega;   // rad/s CCW
};

// SHARED: Wheel velocities (Person 2 calculates, Person 1 uses for debugging)
struct WheelVelocities {
    float front_left;   // rad/s
    float front_right;
    float back_left;
    float back_right;
};

// SHARED: Motor IDs (used by both)
enum MotorID {
    MOTOR_FRONT_LEFT = 0,
    MOTOR_FRONT_RIGHT = 1,
    MOTOR_BACK_LEFT = 2,
    MOTOR_BACK_RIGHT = 3
};
```

**File: `src/shared/config.h`** - Build configuration

```cpp
// SHARED: Kinematic parameters (Person 2 uses, Person 1 may need for telemetry)
#define WHEEL_RADIUS_M     0.05f    // 5cm radius
#define WHEEL_BASE_M        0.20f    // 20cm front-to-back
#define TRACK_WIDTH_M       0.18f    // 18cm left-to-right

// SHARED: Control loop timing
#define MOTOR_CONTROL_PERIOD_MS  10   // 100 Hz
#define SENSOR_FUSION_PERIOD_MS  50   // 20 Hz
```

### Coordination & Integration Plan

#### Day 1: Joint Session (2 hours) - REQUIRED

**Agenda:**

1. **Review Architecture** (30 min)

   - Walk through system diagram
   - Understand data flow
   - Identify integration points

2. **Define Shared Interfaces** (45 min)

   - Create `src/shared/types.h` together
   - Agree on data structure formats
   - Define motor IDs and constants

3. **Coding Standards** (15 min)

   - Naming conventions (camelCase vs snake_case)
   - Code formatting (clang-format config)
   - Comment style

4. **Git Workflow** (15 min)

   - Branch strategy:
     - `main` - stable code
     - `person1/infrastructure` - Person 1's work
     - `person2/control` - Person 2's work
     - `integration/phase1-2` - integration branch
   - Merge process (PRs or direct merge)
   - Commit message format

5. **Integration Points** (15 min)

   - How Person 2 will use Person 1's MotorDriver
   - How Person 1 will receive Person 2's setpoints
   - Shared memory/queue structure for communication

**Deliverables from Joint Session:**

- [ ] `src/shared/types.h` created and committed
- [ ] `src/shared/config.h` created and committed
- [ ] Coding standards document (or `.clang-format` file)
- [ ] Git branches created
- [ ] Integration plan documented

#### Day 3: Interface Handoff

**Person 1 delivers:**

- HAL interface headers (`.h` files)
- MotorDriver interface
- Protocol packet structure

**Person 2 can:**

- Start kinematics implementation (no dependencies)
- Create mock HAL for unit testing
- Design PID controller interface

#### Day 5: Mock Integration

**Person 1 provides:**

- Mock HAL implementations for testing
- Basic MotorDriver that logs commands

**Person 2:**

- Tests control algorithms with mocks
- Validates kinematics and PID independently

#### Day 8-10: Full Integration

**Joint work:**

- Connect Person 2's control code to Person 1's infrastructure
- End-to-end testing
- Bug fixes and refinement
- Performance tuning

### Inter-Task Communication Structure

**Shared Data Structures (FreeRTOS Queues/Semaphores):**

```cpp
// src/shared/queues.h - Define together on Day 1
#include "types.h"

// Queue: Communication Task → Motor Control Task
// Person 1 creates, Person 2 reads from it
extern QueueHandle_t xMotionCommandQueue;  // Holds BodyVelocity

// Queue: Motor Control Task → Telemetry Task (optional)
// Person 2 writes, Person 1 reads for telemetry
extern QueueHandle_t xMotorStatusQueue;    // Holds WheelVelocities + PWM values

// Semaphore: Safety Monitor → Motor Control Task
// Person 1 controls, Person 2 respects
extern SemaphoreHandle_t xSafetySemaphore;  // Take = safe to move, Give = emergency stop
```

**Data Flow:**

```
Person 1 (Communication Task)
    ↓ [BodyVelocity via xMotionCommandQueue]
Person 2 (Motor Control Task)
    ↓ [Uses MotorDriver interface]
Person 1 (MotorDriver → Hardware)
```

### Dependency Diagram

```
Person 1 Tasks:
┌─────────────────┐
│  Communication  │──┐
│     Task        │  │
└─────────────────┘  │
                     │ [Queue: BodyVelocity]
┌─────────────────┐  │
│  Motor Control  │◄─┘
│     Task        │──┐
└─────────────────┘  │
                     │ [Uses: MotorDriver]
┌─────────────────┐  │
│  MotorDriver    │◄─┘
│   (Person 1)    │
└─────────────────┘

Person 2 Tasks:
┌─────────────────┐
│  Motor Control  │──┐
│     Task        │  │
└─────────────────┘  │
                     │ [Implements control logic]
┌─────────────────┐  │
│   Kinematics    │◄─┘
│   (Person 2)    │
└─────────────────┘
        │
        │ [Uses]
        ▼
┌─────────────────┐
│      PID        │
│  (Person 2)     │
└─────────────────┘
```

**Key Integration Points:**

1. **Person 1 → Person 2**: 

   - `MotorDriver` interface (Person 2 calls `setMotorSpeed()`)
   - `xMotionCommandQueue` (Person 2 reads `BodyVelocity`)

2. **Person 2 → Person 1**:

   - `BodyVelocity` setpoint (Person 2 calculates, Person 1 receives via queue)
   - Motor status (optional, for telemetry)

---

### Deliverables Checklist

**Person 1 (Infrastructure) - Detailed Checklist:**

**Week 1 (Days 1-5):**

- [ ] Day 1: Project structure setup (PlatformIO, directories)
- [ ] Day 1: Joint session - define shared interfaces
- [ ] Day 2-3: HAL_PWM implementation (abstract + ESP32)
- [ ] Day 2-3: HAL_GPIO implementation (abstract + ESP32)
- [ ] Day 3: HAL_Timer implementation
- [ ] Day 3: **DELIVER** HAL headers to Person 2
- [ ] Day 3-4: MotorDriver class implementation
- [ ] Day 4: **DELIVER** MotorDriver interface to Person 2
- [ ] Day 4-5: FreeRTOS task structure (5 task skeletons)
- [ ] Day 5: **DELIVER** Mock HAL for Person 2 testing

**Week 2 (Days 6-10):**

- [ ] Day 5-7: Binary protocol (packet structure, encoding/decoding, CRC)
- [ ] Day 7-8: ESP-NOW integration with binary protocol
- [ ] Day 8: Integration checkpoint with Person 2
- [ ] Day 9-10: Full integration, debugging, testing
- [ ] Day 10: End-to-end test passes

**Person 2 (Control) - Detailed Checklist:**

**Week 1 (Days 1-5):**

- [ ] Day 1: Joint session - review shared interfaces
- [ ] Day 1-3: Mecanum kinematics library (forward/inverse)
- [ ] Day 1-3: Unit tests for kinematics
- [ ] Day 3-5: PID controller implementation
- [ ] Day 3-5: Unit tests for PID (step response)
- [ ] Day 4-5: Velocity estimation (PWM-based or encoder)
- [ ] Day 5: Receive mock HAL from Person 1, test with mocks

**Week 2 (Days 6-10):**

- [ ] Day 5-7: Motor control task implementation (4x PID loops)
- [ ] Day 6-8: Motion primitives (forward, strafe, rotate)
- [ ] Day 8: Integration checkpoint with Person 1
- [ ] Day 8-9: Connect to Person 1's MotorDriver
- [ ] Day 8-9: PID tuning on real hardware
- [ ] Day 9-10: Full integration, debugging, testing
- [ ] Day 10: End-to-end test passes

**Shared Deliverables:**

- [ ] Day 1: `src/shared/types.h` - agreed upon and committed
- [ ] Day 1: `src/shared/config.h` - agreed upon and committed
- [ ] Day 1: Coding standards document
- [ ] Day 1: Git branch strategy defined
- [ ] Day 8-10: Integration test suite
- [ ] Day 10: Documentation updated (README, architecture notes)

### Phase 3: Communication (Week 5)

1. Complete binary protocol (CRC, ACK, retry)
2. Enhanced ESP-NOW handler
3. Telemetry system

### Phase 4: Sensors (Week 6)

1. IMU integration
2. Sensor fusion algorithm
3. Enhanced obstacle detection

### Phase 5: Safety & Modes (Week 7)

1. Watchdog and emergency stop
2. State machine implementation
3. Autonomous mode (basic obstacle avoidance)

### Phase 6: Testing & Polish (Week 8)

1. Unit tests
2. HIL testing
3. Performance validation
4. Documentation

---

## Hardware Additions (Optional but Recommended)

**Minimum (for portfolio):**

- IMU sensor (MPU6050, ~$5)
- Encoders (4x, if budget allows, ~$20-40)

**Enhanced:**

- Current sensors for motor feedback (alternative to encoders)
- Additional ultrasonic sensors (side-facing)
- Status LEDs for debugging

**Total Additional Cost:** ~$25-65

---

## Success Metrics

**Technical:**

- Control loop: 10ms ± 1ms jitter
- Gesture latency: <100ms end-to-end
- Zero motor runaway incidents
- 99%+ communication reliability

**Portfolio:**

- Clean, modular codebase
- Comprehensive documentation
- Demo videos showing all modes
- Quantitative performance data

---

## Engineering Skills Demonstrated

1. **Embedded Systems**: RTOS, task scheduling, real-time constraints
2. **Control Systems**: PID tuning, closed-loop control, kinematics
3. **Communication**: Protocol design, error handling, reliability
4. **Robotics**: Mecanum kinematics, motion planning, sensor fusion
5. **Software Architecture**: Layered design, HAL, modularity
6. **Safety Engineering**: Watchdog, E-stop, fault tolerance
7. **Testing**: Unit tests, HIL, performance validation

This upgrade plan transforms your functional project into a comprehensive embedded systems portfolio piece that demonstrates senior-level engineering thinking across multiple domains.
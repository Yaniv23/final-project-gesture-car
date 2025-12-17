# Phase 1 & Phase 2: Two-Person Task Breakdown

## Quick Reference

**Person 1 (Infrastructure/Embedded)**: FreeRTOS, HAL, Communication Protocol  
**Person 2 (Control/Robotics)**: Kinematics, PID, Motor Control

---

## Person 1: Infrastructure Tasks

### Week 1: Foundation Setup

#### Day 1-2: Project Structure & HAL Interfaces
**Priority: CRITICAL (blocks Person 2)**

**Tasks:**
1. Set up PlatformIO project
   - Create `platformio.ini` with ESP32 configuration
   - Create directory structure
   - Set up Git branches

2. Define HAL interfaces (SHARED with Person 2)
   - `hal/hal_pwm.h` - PWM abstraction
   - `hal/hal_gpio.h` - GPIO abstraction
   - `hal/hal_timer.h` - Timer/WDT abstraction

3. Create ESP32 implementations
   - `hal/hal_esp32_pwm.cpp`
   - `hal/hal_esp32_gpio.cpp`
   - `hal/hal_esp32_timer.cpp`

**Deliverable:** HAL interfaces that Person 2 can use for unit testing

---

#### Day 3-4: Motor Driver & FreeRTOS Setup
**Priority: HIGH**

**Tasks:**
1. Implement `MotorDriver` class
   - Uses HAL_PWM and HAL_GPIO
   - Supports 4 motors (FL, FR, BL, BR)
   - Interface: `setMotorSpeed(motor_id, speed)`

2. Set up FreeRTOS
   - Create 5 tasks with correct priorities
   - Task periods: 10ms (motor), 50ms (sensor/safety), 100ms (comm/telemetry)
   - Task priorities: 5 (safety), 4 (motor), 3 (sensor), 2 (comm), 1 (telemetry)

**Deliverable:** MotorDriver interface + FreeRTOS task skeletons

---

#### Day 5-7: Binary Protocol
**Priority: MEDIUM**

**Tasks:**
1. Define packet structure
   - SOF (0xAA), Version, CMD, SEQ, LEN, DATA, CRC16
   - Shared types in `communication/protocol_types.h`

2. Implement encoding/decoding
   - `encodePacket()` - creates packet with CRC
   - `decodePacket()` - validates and extracts data
   - `calculateCRC16()` - CRC16-CCITT

3. Update ESP-NOW handler
   - Replace string commands with binary packets
   - Basic send/receive (no ACK yet)

**Deliverable:** Binary protocol working with ESP-NOW

---

#### Day 8-10: Integration
**Priority: HIGH**

**Tasks:**
1. Integrate with Person 2's control code
2. End-to-end testing
3. Bug fixes and refinement

---

## Person 2: Control Tasks

### Week 1: Algorithms & Math

#### Day 1-3: Mecanum Kinematics
**Priority: HIGH (can start immediately)**

**Tasks:**
1. Implement forward kinematics
   - Input: wheel velocities [ω_FL, ω_FR, ω_BL, ω_BR]
   - Output: body velocity [vx, vy, ω]

2. Implement inverse kinematics
   - Input: body velocity [vx, vy, ω]
   - Output: wheel velocities [ω_FL, ω_FR, ω_BL, ω_BR]

3. Write unit tests
   - Round-trip consistency (forward then inverse)
   - Edge cases (pure rotation, pure translation)

**Mathematical Model:**
```
For mecanum wheels with wheel_base = L, track_width = W:
v_FL = vx - vy - ω*(L+W)/2
v_FR = vx + vy + ω*(L+W)/2
v_BL = vx + vy - ω*(L+W)/2
v_BR = vx - vy + ω*(L+W)/2
```

**Deliverable:** Kinematics library with unit tests

---

#### Day 3-5: PID Controller
**Priority: HIGH**

**Tasks:**
1. Implement PID class
   - Kp, Ki, Kd gains
   - Integral anti-windup
   - Output clamping
   - Reset functionality

2. Write unit tests
   - Step response
   - Anti-windup behavior
   - Reset functionality

**Deliverable:** PID controller with unit tests

---

#### Day 4-5: Velocity Estimation
**Priority: MEDIUM**

**Tasks:**
1. Choose approach:
   - **Option A**: PWM-based estimation (calibrated mapping)
   - **Option B**: Encoder driver (if encoders available)

2. Implement chosen approach
   - Smooth filtering (low-pass)
   - Calibration method

**Deliverable:** Velocity feedback for PID

---

### Week 2: Integration & Control Loop

#### Day 5-7: Motor Control Task
**Priority: CRITICAL**

**Tasks:**
1. Implement `task_motor_control`
   - 10ms period (100 Hz)
   - 4x PID controllers (one per wheel)
   - Read setpoint from communication task
   - Apply inverse kinematics
   - Update PID loops
   - Write PWM via MotorDriver

2. Integration points:
   - Use Person 1's `MotorDriver` interface
   - Use Person 1's binary protocol for commands
   - Use shared data structures

**Deliverable:** Working closed-loop motor control

---

#### Day 6-8: Motion Primitives
**Priority: MEDIUM**

**Tasks:**
1. Implement motion planner
   - `moveForward(distance, speed)`
   - `strafeRight(distance, speed)`
   - `rotate(angle, angular_speed)`
   - `stop()`

2. Integration with control loop
   - Generate setpoints over time
   - Track progress

**Deliverable:** High-level motion commands

---

#### Day 8-10: Testing & Tuning
**Priority: HIGH**

**Tasks:**
1. Tune PID gains
   - Start with conservative values
   - Test step response
   - Avoid oscillations

2. Integration testing
   - End-to-end with Person 1's code
   - Performance measurements

**Deliverable:** Tuned control system

---

## Coordination Schedule

### Day 1: Joint Session (2 hours)
**Agenda:**
- Review architecture
- Define shared interfaces
- Agree on coding standards
- Set up Git workflow

**Shared Files:**
- `src/shared/types.h` - Common data structures
- `src/shared/config.h` - Build configuration

### Day 3: Interface Handoff
**Person 1 delivers:**
- HAL interface headers
- MotorDriver interface
- Protocol packet structure

**Person 2 can:**
- Start kinematics (no dependencies)
- Create mock HAL for testing

### Day 5: Mock Integration
**Person 1 provides:**
- Mock HAL implementations

**Person 2:**
- Tests control with mocks
- Validates independently

### Day 8-10: Full Integration
**Joint work:**
- Connect Person 2's control to Person 1's infrastructure
- End-to-end testing
- Bug fixes
- Performance tuning

---

## File Structure (Final)

```
Vehicule_Controller/
├── platformio.ini
├── src/
│   ├── main.cpp                    # Person 1: FreeRTOS init
│   ├── config.h                    # Person 1: Build config
│   ├── shared/
│   │   ├── types.h                 # BOTH: Shared data structures
│   │   └── config.h               # BOTH: Shared config
│   ├── hal/                        # Person 1: HAL layer
│   │   ├── hal_pwm.h/cpp
│   │   ├── hal_gpio.h/cpp
│   │   ├── hal_timer.h/cpp
│   │   └── hal_esp32_*.cpp
│   ├── drivers/                    # Person 1: Device drivers
│   │   ├── motor_driver.h/cpp
│   │   └── ...
│   ├── tasks/                      # BOTH: FreeRTOS tasks
│   │   ├── task_motor_control.cpp  # Person 2: Control logic
│   │   ├── task_sensor_fusion.cpp  # Person 1: Skeleton
│   │   ├── task_communication.cpp  # Person 1: ESP-NOW
│   │   ├── task_safety_monitor.cpp # Person 1: Skeleton
│   │   └── task_telemetry.cpp     # Person 1: Skeleton
│   ├── control/                    # Person 2: Control algorithms
│   │   ├── mecanum_kinematics.h/cpp
│   │   ├── pid_controller.h/cpp
│   │   ├── motion_planner.h/cpp
│   │   └── velocity_estimator.h/cpp
│   ├── communication/              # Person 1: Protocol
│   │   ├── protocol_binary.h/cpp
│   │   ├── protocol_types.h       # BOTH: Shared types
│   │   └── espnow_handler.cpp
│   └── utils/
│       └── logger.cpp
└── tests/
    └── unit/
        ├── test_kinematics.cpp     # Person 2
        └── test_pid.cpp            # Person 2
```

---

## Critical Interfaces (Must Agree On)

### 1. MotorDriver Interface
```cpp
class MotorDriver {
public:
    enum MotorID { MOTOR_FL = 0, MOTOR_FR = 1, MOTOR_BL = 2, MOTOR_BR = 3 };
    bool init();
    void setMotorSpeed(MotorID motor, int16_t speed);  // -1023 to +1023
    void stopAll();
};
```

### 2. Body Velocity Structure
```cpp
struct BodyVelocity {
    float vx;      // m/s forward
    float vy;      // m/s right
    float omega;   // rad/s CCW
};
```

### 3. Protocol Packet Structure
```cpp
struct MotionCommand {
    BodyVelocity velocity;
    uint16_t duration_ms;
};

struct ProtocolPacket {
    uint8_t sof;        // 0xAA
    uint8_t version;    // 0x01
    uint8_t cmd_id;     // 0x01 = motion
    uint16_t seq;
    uint8_t len;
    uint8_t data[32];
    uint16_t crc;
};
```

---

## Success Criteria

### Person 1 (Infrastructure)
- [ ] HAL layer compiles and works
- [ ] MotorDriver controls all 4 motors
- [ ] FreeRTOS tasks run with correct timing
- [ ] Binary protocol encodes/decodes correctly
- [ ] ESP-NOW sends/receives binary packets
- [ ] Integration with Person 2's code works

### Person 2 (Control)
- [ ] Kinematics forward/inverse are consistent
- [ ] PID controller stabilizes
- [ ] Motor control task runs at 10ms
- [ ] Motors respond smoothly to commands
- [ ] Motion primitives work
- [ ] Integration with Person 1's infrastructure works

### Joint
- [ ] End-to-end: Gesture → Motor works
- [ ] No crashes or timing issues
- [ ] Performance meets targets (<100ms latency)
- [ ] Code is clean and documented

---

## Getting Started Checklist

### Person 1
1. [ ] Install PlatformIO
2. [ ] Create project structure
3. [ ] Set up Git branch
4. [ ] Create HAL interfaces (Day 1)
5. [ ] Share interfaces with Person 2 (Day 3)

### Person 2
1. [ ] Set up development environment
2. [ ] Create Git branch
3. [ ] Start kinematics implementation (Day 1)
4. [ ] Create mock HAL for testing (Day 3)
5. [ ] Wait for Person 1's interfaces (Day 3)

---

## Questions to Resolve (Day 1 Session)

1. **Encoders**: Do we have encoders? (affects velocity estimation approach)
2. **IMU**: Will we add IMU in Phase 4? (affects sensor task design)
3. **Git Workflow**: Feature branches? Merge strategy?
4. **Coding Style**: Naming conventions? Indentation?
5. **Testing**: Unit test framework? (Unity for embedded?)

---

## Timeline Summary

| Day | Person 1 | Person 2 | Coordination |
|-----|----------|----------|--------------|
| 1 | Project setup, HAL interfaces | Kinematics start | Joint session |
| 2 | HAL implementation | Kinematics continue | - |
| 3 | MotorDriver, FreeRTOS | PID controller | Interface handoff |
| 4 | FreeRTOS tasks | PID + velocity est | - |
| 5 | Binary protocol start | Motion control task | Mock integration |
| 6 | Binary protocol continue | Motion primitives | - |
| 7 | ESP-NOW integration | Motion primitives | - |
| 8 | Integration prep | Testing & tuning | - |
| 9 | Integration | Integration | Full integration |
| 10 | Testing & fixes | Testing & fixes | Final testing |

---

## Next Steps After Phase 1 & 2

**Phase 3**: Communication enhancements (ACK, retry, telemetry)  
**Phase 4**: Sensor fusion (IMU, enhanced obstacle detection)  
**Phase 5**: Safety & autonomous modes  
**Phase 6**: Testing & documentation

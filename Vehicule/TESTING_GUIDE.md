# Testing Guide - First Test on Real Vehicle

## ✅ Pre-Test Checklist

### 1. Hardware Connections
- [ ] All motor direction pins connected correctly (IN1, IN2 for each motor)
- [ ] Common PWM pin (34) connected to all motor drivers' ENA pins
- [ ] Servo connected to pin 4
- [ ] Ultrasonic sensor: TRIG on pin 18, ECHO on pin 16
- [ ] Power supply adequate for all motors
- [ ] ESP32 powered and stable

### 2. Software Configuration
- [ ] Verify pin assignments in `config.h` match your hardware
- [ ] Check `platformio.ini` upload port is correct (`/dev/ttyUSB0` or your COM port)
- [ ] Serial monitor speed set to 115200 baud

### 3. Safety Precautions
- [ ] Vehicle elevated (wheels off ground) for first test
- [ ] Emergency stop mechanism ready (unplug power if needed)
- [ ] Clear area around vehicle
- [ ] Battery/motor power can be quickly disconnected

---

## 🚀 Step-by-Step Testing Procedure

### Step 1: Compile and Upload

```bash
cd /home/yaniv/Projects/final-project-gesture-car/Vehicule
pio run -t upload
```

**Expected Serial Output:**
```
========================================
Gesture Car - ESP32 Vehicle Controller
Phase 1: Infrastructure Setup
========================================
FreeRTOS Version: ...
CPU Frequency: ... MHz
Free Heap: ... bytes
========================================

[SETUP] Initializing shared queues...
[SETUP] Shared queues initialized
[SETUP] Initializing MotorDriver...
[SETUP] MotorDriver initialized (common PWM on pin 34)
[SETUP] Safety systems initialized
[SETUP] Creating FreeRTOS tasks...
[SETUP] Created task: SafetyMonitor (Priority 5)
[SETUP] Created task: MotorControl (Priority 4)
[SETUP] Created task: SensorFusion (Priority 3)
[SETUP] Created task: Communication (Priority 2)
[SETUP] Created task: Telemetry (Priority 1)

[SETUP] All tasks created successfully!
[SETUP] System ready - FreeRTOS scheduler starting...

[TASK_SAFETY] Safety monitor task started
[TASK_MOTOR] Motor control task started
[TASK_SENSOR] Sensor fusion task started
[TASK_COMM] Communication task started
[TASK_TELEMETRY] Telemetry task started
```

### Step 2: Verify ESP-NOW Communication

**Look for this in Serial Monitor:**
```
[COMM] ESP-NOW initialized
[COMM] MAC Address: XX:XX:XX:XX:XX:XX
```

**Note the MAC address** - you'll need it for your PC/controller to send commands.

### Step 3: Test Without Motors (Safe Mode)

**First, test with vehicle elevated (wheels off ground):**

1. **Send STOP command** (0x00) - Motors should not move
2. **Send FORWARD command** (0x01) - Check Serial for `[MOTOR] FORWARD`
3. **Send STOP command** (0x00) - Motors should stop

**Expected Serial Output:**
```
[COMM] Received command byte: 0x01
[MOTOR] FORWARD
[COMM] Received command byte: 0x00
[MOTOR] STOP
```

### Step 4: Test Basic Movements

**With vehicle elevated, test each movement:**

| Command | Byte | Expected Behavior |
|---------|------|-------------------|
| STOP | 0x00 | All motors stop |
| FORWARD | 0x01 | All wheels forward |
| BACKWARD | 0x02 | All wheels backward |
| STRAFE_LEFT | 0x03 | Strafe left |
| STRAFE_RIGHT | 0x04 | Strafe right |
| ROTATE_CW | 0x05 | Rotate clockwise |
| ROTATE_CCW | 0x06 | Rotate counter-clockwise |

**Watch Serial Monitor for:**
- Command received confirmation
- Motor action confirmation
- Any error messages

### Step 5: Test Emergency Stop

**Test obstacle detection:**
1. Place object < 10cm in front of ultrasonic sensor
2. Send any movement command
3. **Expected:** Vehicle should stop immediately
4. **Serial should show:** `⚠️ Object detected close!`

### Step 6: Ground Test (Final)

**Only after all above tests pass:**

1. Place vehicle on ground
2. Start with STOP command
3. Test FORWARD for 1-2 seconds
4. Send STOP immediately
5. Gradually test other movements

---

## 📋 Command Byte Reference

For testing, you can send these bytes via ESP-NOW:

```cpp
// Basic movements
0x00 = STOP
0x01 = FORWARD
0x02 = BACKWARD
0x03 = STRAFE_LEFT
0x04 = STRAFE_RIGHT
0x05 = ROTATE_CW
0x06 = ROTATE_CCW

// Diagonal movements
0x07 = DIAGONAL_FORWARD_LEFT
0x08 = DIAGONAL_FORWARD_RIGHT
0x09 = DIAGONAL_BACKWARD_LEFT
0x0A = DIAGONAL_BACKWARD_RIGHT

// Pivot movements
0x0B = PIVOT_LEFT
0x0C = PIVOT_RIGHT
```

---

## ⚠️ Troubleshooting

### Problem: Motors don't move
- **Check:** Common PWM pin (34) connected correctly
- **Check:** Motor driver power supply
- **Check:** Direction pins connected correctly
- **Check:** Serial monitor for error messages

### Problem: Wrong direction
- **Check:** IN1/IN2 pins swapped in `config.h`
- **Swap:** Direction pins in motor config

### Problem: ESP-NOW not receiving
- **Check:** MAC address printed in Serial
- **Check:** PC/controller sending to correct MAC
- **Check:** ESP-NOW channel matches (default: 0)

### Problem: Emergency stop always active
- **Check:** Ultrasonic sensor reading (should be > 10cm normally)
- **Check:** `EMERGENCY_STOP_DISTANCE_CM` in `config.h`
- **Temporary fix:** Increase distance threshold

### Problem: Vehicle moves too fast/slow
- **Adjust:** `MOTOR_SPEED_SLOW` in `config.h` (0-255 range)
- **Note:** This is scaled to 0-1023 for PWM

---

## 📊 What to Monitor

### Serial Monitor Output:
- Task startup messages
- Command reception (`[COMM] Received command byte: 0xXX`)
- Motor actions (`[MOTOR] FORWARD`, etc.)
- Emergency stop triggers (`⚠️ Object detected close!`)
- Error messages

### Hardware Checks:
- Motor direction (correct rotation)
- Motor speed (consistent across all motors)
- Servo sweep (should move 0-60 degrees)
- Ultrasonic readings (distance measurements)

---

## ✅ Success Criteria

Your first test is successful if:
1. ✅ All tasks start without errors
2. ✅ ESP-NOW receives commands
3. ✅ Motors respond to commands
4. ✅ Emergency stop works (obstacle detection)
5. ✅ All movements execute correctly
6. ✅ Vehicle stops when STOP command sent

---

## 🔧 Next Steps After Successful Test

1. **Fine-tune motor speeds** in `config.h`
2. **Calibrate ultrasonic sensor** distance threshold
3. **Test all movement patterns** thoroughly
4. **Implement missing safety features** (watchdog, timeout monitor)
5. **Add telemetry reporting** for debugging

---

## 🆘 Emergency Procedures

**If vehicle behaves unexpectedly:**
1. **Immediately send STOP command (0x00)**
2. **Disconnect power supply**
3. **Check Serial Monitor for errors**
4. **Review pin connections**

**If motors run continuously:**
- Emergency stop may be triggered
- Check ultrasonic sensor distance
- Send STOP command multiple times

---

Good luck with your first test! 🚗💨

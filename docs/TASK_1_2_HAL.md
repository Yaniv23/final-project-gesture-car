# Task 1.2: HAL Layer Implementation - Documentation

**Assigned to:** Person 1 (Infrastructure Lead)  
**Timeline:** Day 1-2 (2-3 hours total)  
**Status:** ✅ Complete

---

## What is a HAL (Hardware Abstraction Layer)?

Think of the HAL as a **universal translator** between your code and the hardware. Instead of talking directly to the ESP32 (which uses functions like `analogWrite()` and `digitalWrite()`), your code talks to the HAL using simple, consistent functions.

### Why Do We Need It?

**Without HAL:**
- Code is tied to ESP32-specific functions
- Hard to test (can't easily create fake hardware)
- Hard to port to other microcontrollers
- Code is messy and hard to understand

**With HAL:**
- Code uses simple, consistent functions
- Easy to test (can create mock/fake versions)
- Easy to port to other hardware
- Code is clean and organized

**Real-World Analogy:** 
Imagine you have a universal remote control. Instead of learning different remotes for each TV brand, you have ONE remote that works with all of them. The HAL is that universal remote for hardware control.

---

## HAL Components Overview

The HAL layer consists of **3 main modules**, each handling a different aspect of hardware control:

### 1. **HAL_PWM** - Speed Control
Controls motor speed using PWM (Pulse Width Modulation)

### 2. **HAL_GPIO** - Digital On/Off Control  
Controls digital pins (for motor direction, sensors, etc.)

### 3. **HAL_Timer** - Time & Safety
Handles timing operations and watchdog timer (safety mechanism)

---

## 1. HAL_PWM - Speed Control

### What Does It Do?
Controls the **speed** of motors using PWM signals. PWM is like turning a light switch on and off very quickly - the faster you do it, the brighter the light. For motors, faster PWM = faster motor speed.

### Key Functions

#### `HAL_PWM_Init(pin, frequency, resolution)`
**What it does:** Sets up a pin for speed control

**Parameters:**
- `pin` - Which GPIO pin to use (e.g., pin 34 for motor speed)
- `frequency` - How fast to pulse (500 Hz = 500 times per second)
- `resolution` - How many speed levels (8 bits = 256 levels, 0-255)

**Example:**
```cpp
// Set up pin 34 for motor speed control
// 500 Hz frequency, 256 speed levels (0-255)
HAL_PWM_Init(34, 500, 8);
```

#### `HAL_PWM_SetDuty(pin, duty_cycle)`
**What it does:** Sets the motor speed

**Parameters:**
- `pin` - The pin you initialized
- `duty_cycle` - Speed value (0 = stopped, 255 = full speed for 8-bit)

**Example:**
```cpp
HAL_PWM_SetDuty(34, 128);  // 50% speed (128 out of 255)
HAL_PWM_SetDuty(34, 255); // Full speed
HAL_PWM_SetDuty(34, 0);   // Stop motor
```

#### `HAL_PWM_Stop(pin)`
**What it does:** Stops the motor (sets speed to 0)

**Example:**
```cpp
HAL_PWM_Stop(34);  // Stop motor on pin 34
```

### Real-World Usage

**In our car:**
- Pin 34 controls speed of Front Right and Front Left motors
- Pin 26 controls speed of Back Right and Back Left motors
- Speed range: 0-255 (8-bit resolution)

**Code Example:**
```cpp
// Initialize motor speed pins
HAL_PWM_Init(FRONT_RIGHT_ENA, 500, 8);
HAL_PWM_Init(BACK_RIGHT_ENA, 500, 8);

// Set motor to 50% speed
HAL_PWM_SetDuty(FRONT_RIGHT_ENA, 128);

// Stop motor
HAL_PWM_Stop(FRONT_RIGHT_ENA);
```

---

## 2. HAL_GPIO - Digital On/Off Control

### What Does It Do?
Controls digital pins that can be either **ON** (HIGH, 3.3V) or **OFF** (LOW, 0V). Used for:
- Motor direction control (forward/backward)
- Sensor triggers
- LED indicators
- Reading button presses

### Key Functions

#### `HAL_GPIO_Init(pin, direction, pull_mode)`
**What it does:** Sets up a pin as input or output

**Parameters:**
- `pin` - GPIO pin number
- `direction` - `HAL_GPIO_INPUT` (read) or `HAL_GPIO_OUTPUT` (write)
- `pull_mode` - For input pins: `HAL_GPIO_PULLUP`, `HAL_GPIO_PULLDOWN`, or `HAL_GPIO_FLOATING`

**Example:**
```cpp
// Set pin 35 as output (for motor direction)
HAL_GPIO_Init(35, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);

// Set pin 16 as input (for sensor reading)
HAL_GPIO_Init(16, HAL_GPIO_INPUT, HAL_GPIO_FLOATING);
```

#### `HAL_GPIO_Write(pin, state)`
**What it does:** Turns a pin ON or OFF (for output pins)

**Parameters:**
- `pin` - The pin you initialized as output
- `state` - `HAL_GPIO_HIGH` (ON) or `HAL_GPIO_LOW` (OFF)

**Example:**
```cpp
HAL_GPIO_Write(35, HAL_GPIO_HIGH);  // Turn pin ON
HAL_GPIO_Write(35, HAL_GPIO_LOW);   // Turn pin OFF
```

#### `HAL_GPIO_Read(pin)`
**What it does:** Reads if a pin is ON or OFF (for input pins)

**Returns:** `HAL_GPIO_HIGH` or `HAL_GPIO_LOW`

**Example:**
```cpp
HAL_GPIO_State state = HAL_GPIO_Read(16);
if (state == HAL_GPIO_HIGH) {
    Serial.println("Pin is ON");
}
```

#### `HAL_GPIO_Toggle(pin)`
**What it does:** Flips the pin state (ON becomes OFF, OFF becomes ON)

**Example:**
```cpp
HAL_GPIO_Toggle(35);  // If it was ON, now it's OFF (and vice versa)
```

### Real-World Usage

**In our car:**
- Pins 35, 32 control Front Right motor direction
- Pins 33, 25 control Front Left motor direction
- Pin 18 triggers ultrasonic sensor
- Pin 16 reads ultrasonic sensor echo

**Code Example:**
```cpp
// Initialize motor direction pins
HAL_GPIO_Init(FRONT_RIGHT_IN1, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
HAL_GPIO_Init(FRONT_RIGHT_IN2, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);

// Set motor direction: Forward
HAL_GPIO_Write(FRONT_RIGHT_IN1, HAL_GPIO_HIGH);
HAL_GPIO_Write(FRONT_RIGHT_IN2, HAL_GPIO_LOW);

// Set motor direction: Backward
HAL_GPIO_Write(FRONT_RIGHT_IN1, HAL_GPIO_LOW);
HAL_GPIO_Write(FRONT_RIGHT_IN2, HAL_GPIO_HIGH);
```

---

## 3. HAL_Timer - Time & Safety

### What Does It Do?
Provides timing functions and a **watchdog timer** (safety mechanism that resets the system if it freezes).

### Key Functions

#### `HAL_Timer_GetMillis()`
**What it does:** Gets current time in milliseconds since the system started

**Returns:** Number of milliseconds (wraps around after ~49 days)

**Example:**
```cpp
uint32_t start_time = HAL_Timer_GetMillis();
// ... do something ...
uint32_t elapsed = HAL_Timer_GetMillis() - start_time;
Serial.print("Took ");
Serial.print(elapsed);
Serial.println(" milliseconds");
```

#### `HAL_Timer_GetMicros()`
**What it does:** Gets current time in microseconds (more precise than milliseconds)

**Example:**
```cpp
uint64_t start = HAL_Timer_GetMicros();
// ... do something ...
uint64_t elapsed_us = HAL_Timer_GetMicros() - start;
```

#### `HAL_Timer_DelayMs(ms)`
**What it does:** Waits for a specified number of milliseconds

**⚠️ Warning:** This is a **blocking** function - it stops everything until the delay is done. In FreeRTOS tasks, prefer `vTaskDelay()` instead.

**Example:**
```cpp
HAL_Timer_DelayMs(1000);  // Wait 1 second
```

#### `HAL_Timer_WatchdogInit(config)`
**What it does:** Sets up the watchdog timer (safety mechanism)

**What is a watchdog?**
Imagine a dog that needs to be fed every 5 seconds. If you forget to feed it, it barks (resets the system). This prevents the system from freezing forever.

**Example:**
```cpp
HAL_Timer_WatchdogConfig wdt_config = {
    .timeout_ms = 5000,  // Reset if not fed for 5 seconds
    .enable = true
};
HAL_Timer_WatchdogInit(&wdt_config);
```

#### `HAL_Timer_WatchdogFeed()`
**What it does:** "Feeds" the watchdog (tells it "I'm still alive!")

**⚠️ Important:** Must be called regularly (before timeout expires) or the system will reset!

**Example:**
```cpp
void loop() {
    // Do your work...
    
    // Feed watchdog to prevent reset
    HAL_Timer_WatchdogFeed();
    
    // Continue...
}
```

### Real-World Usage

**Timing Example:**
```cpp
// Measure how long something takes
uint32_t start = HAL_Timer_GetMillis();
doSomething();
uint32_t duration = HAL_Timer_GetMillis() - start;
Serial.print("Operation took: ");
Serial.print(duration);
Serial.println(" ms");
```

**Watchdog Example:**
```cpp
// In setup()
HAL_Timer_WatchdogConfig wdt = {
    .timeout_ms = 5000,  // 5 second timeout
    .enable = true
};
HAL_Timer_WatchdogInit(&wdt);

// In loop() or main task
void loop() {
    // Do work...
    processCommands();
    updateMotors();
    
    // Feed watchdog (must do this regularly!)
    HAL_Timer_WatchdogFeed();
}
```

---

## Complete Example: Controlling a Motor

Here's how you'd use all three HAL modules together to control a motor:

```cpp
#include "hal/hal_gpio.h"
#include "hal/hal_pwm.h"
#include "hal/hal_timer.h"
#include "config.h"

void setup() {
    // 1. Initialize PWM for speed control
    HAL_PWM_Init(FRONT_RIGHT_ENA, 500, 8);
    
    // 2. Initialize GPIO for direction control
    HAL_GPIO_Init(FRONT_RIGHT_IN1, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    HAL_GPIO_Init(FRONT_RIGHT_IN2, HAL_GPIO_OUTPUT, HAL_GPIO_FLOATING);
    
    // 3. Initialize watchdog for safety
    HAL_Timer_WatchdogConfig wdt = {
        .timeout_ms = 5000,
        .enable = true
    };
    HAL_Timer_WatchdogInit(&wdt);
}

void moveMotorForward(uint8_t speed) {
    // Set direction: Forward
    HAL_GPIO_Write(FRONT_RIGHT_IN1, HAL_GPIO_HIGH);
    HAL_GPIO_Write(FRONT_RIGHT_IN2, HAL_GPIO_LOW);
    
    // Set speed
    HAL_PWM_SetDuty(FRONT_RIGHT_ENA, speed);
}

void stopMotor() {
    // Stop PWM
    HAL_PWM_Stop(FRONT_RIGHT_ENA);
    
    // Set direction pins to LOW
    HAL_GPIO_Write(FRONT_RIGHT_IN1, HAL_GPIO_LOW);
    HAL_GPIO_Write(FRONT_RIGHT_IN2, HAL_GPIO_LOW);
}

void loop() {
    // Move forward at 50% speed
    moveMotorForward(128);
    HAL_Timer_DelayMs(2000);  // Run for 2 seconds
    
    // Stop
    stopMotor();
    HAL_Timer_DelayMs(1000);
    
    // Feed watchdog
    HAL_Timer_WatchdogFeed();
}
```

---

## File Structure

```
Vehicule/src/hal/
├── hal_pwm.h          # PWM interface (what functions are available)
├── hal_pwm.cpp         # ESP32 implementation (how it actually works)
├── hal_gpio.h          # GPIO interface
├── hal_gpio.cpp        # ESP32 implementation
├── hal_timer.h         # Timer interface
└── hal_timer.cpp       # ESP32 implementation
```

**Key Point:** 
- `.h` files = **Interface** (what you can do)
- `.cpp` files = **Implementation** (how it actually works on ESP32)

---

## Benefits Summary

### ✅ Portability
- Code works on ESP32 now
- Can easily port to Arduino, STM32, Raspberry Pi, etc.
- Just change the `.cpp` files, keep the same `.h` interface

### ✅ Testability
- Can create "mock" versions for testing
- Test your code without real hardware
- Person 2 can test control algorithms without motors

### ✅ Clean Code
- Simple, consistent function names
- Easy to understand and maintain
- Clear separation between hardware and logic

### ✅ Safety
- Watchdog timer prevents system freezes
- Consistent error handling
- Well-documented functions

---

## Common Patterns

### Pattern 1: Initialize Everything in setup()
```cpp
void setup() {
    // Initialize all HAL modules you need
    initHAL_Motors();
    initHAL_Sensors();
    initHAL_Watchdog();
}
```

### Pattern 2: Use HAL Functions Instead of Direct Hardware Calls
**❌ Bad (direct hardware):**
```cpp
analogWrite(34, 128);
digitalWrite(35, HIGH);
delay(1000);
```

**✅ Good (using HAL):**
```cpp
HAL_PWM_SetDuty(34, 128);
HAL_GPIO_Write(35, HAL_GPIO_HIGH);
HAL_Timer_DelayMs(1000);
```

### Pattern 3: Always Feed Watchdog in Main Loop
```cpp
void loop() {
    // Your main code
    processCommands();
    updateMotors();
    
    // Always feed watchdog!
    HAL_Timer_WatchdogFeed();
}
```

---

## Troubleshooting

### Problem: Motor doesn't move
**Check:**
1. Did you call `HAL_PWM_Init()` first?
2. Is the speed value correct (0-255 for 8-bit)?
3. Are the direction pins set correctly?

### Problem: Watchdog keeps resetting
**Solution:** Make sure you call `HAL_Timer_WatchdogFeed()` regularly (at least once per second if timeout is 5 seconds)

### Problem: GPIO pin doesn't work
**Check:**
1. Did you call `HAL_GPIO_Init()` first?
2. Is the pin direction correct (INPUT vs OUTPUT)?
3. Is the pin number correct?

---

## Next Steps

Now that the HAL layer is complete:

1. **Task 1.3:** Build the MotorDriver class that uses these HAL functions
2. **Person 2:** Can start writing control algorithms using HAL interfaces
3. **Testing:** Can create mock HAL implementations for unit tests

---

## Quick Reference Card

### PWM Functions
```cpp
HAL_PWM_Init(pin, frequency, resolution)  // Setup
HAL_PWM_SetDuty(pin, speed)               // Set speed (0-255)
HAL_PWM_Stop(pin)                         // Stop
```

### GPIO Functions
```cpp
HAL_GPIO_Init(pin, direction, pull)       // Setup
HAL_GPIO_Write(pin, state)                // Set ON/OFF
HAL_GPIO_Read(pin)                        // Read state
HAL_GPIO_Toggle(pin)                      // Flip state
```

### Timer Functions
```cpp
HAL_Timer_GetMillis()                     // Get time (ms)
HAL_Timer_DelayMs(ms)                     // Wait (ms)
HAL_Timer_WatchdogInit(config)            // Setup watchdog
HAL_Timer_WatchdogFeed()                  // Feed watchdog
```

---

**Last Updated:** Task 1.2 Implementation  
**Status:** ✅ Complete  
**Next Task:** Task 1.3 - MotorDriver Implementation


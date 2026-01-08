# Motor + ESP-NOW Command Reception Test

This test project focuses on testing motor control and ESP-NOW command reception **without** servo motor or ultrasonic sensor components.

## Purpose

This simplified test allows you to:
- Test motor control functionality in isolation
- Test ESP-NOW command reception and processing
- Debug motor issues without interference from servo/ultrasonic sensors
- Verify command protocol communication

## What's Included

- **Motor Control**: All 4 motors (front left, front right, back left, back right)
- **ESP-NOW Communication**: Receives commands from sender and forwards to motor control
- **Safety Systems**: Minimal safety (timeout monitoring, emergency stop) - **NO sensor-based stops**
- **FreeRTOS Tasks**: Communication, Motor Control, Safety Monitor

## What's Excluded

- ❌ Servo Motor
- ❌ Ultrasonic Sensor
- ❌ Sensor Fusion Task
- ❌ Obstacle Detection

## Building

```bash
cd test_motor_espnow
pio run
```

## Uploading

```bash
pio run -t upload
```

## Monitoring

```bash
pio device monitor
```

## Configuration

- **SIMULATION_MODE**: Set to `0` in `platformio.ini` (real ESP-NOW communication)
- **Motor Pins**: Uses same pin definitions from `Vehicule/src/config.h`
- **Command Protocol**: Uses same binary command protocol as main vehicle

## Architecture

The test uses a simplified FreeRTOS structure:

1. **Communication Task** (Priority 2): Receives ESP-NOW commands, validates them, and forwards to command queue
2. **Motor Control Task** (Priority 4): Reads commands from queue and executes motor movements
3. **Safety Monitor Task** (Priority 5): Monitors timeouts and feeds watchdog (no sensor-based safety)

## Testing Flow

1. ESP32 initializes and waits for ESP-NOW connection from sender
2. Sender sends commands via ESP-NOW
3. Communication task receives and queues commands
4. Motor control task executes commands on motors
5. Serial output shows received commands and motor actions

## Serial Output

The test provides detailed serial output showing:
- ESP-NOW connection status
- Received commands
- Motor actions
- Safety status

## Notes

- **Automatic Source Inclusion**: This project automatically includes source files from `../Vehicule/src/` using a Python build script (`extra_script.py`)
- The script automatically scans directories and includes only needed `.cpp` files (excludes servo, ultrasonic, and sensor fusion)
- All motor commands from the main vehicle are supported
- Emergency stop can still be triggered manually if needed
- No automatic obstacle detection or avoidance

## How Source Files Are Linked

The project uses PlatformIO's `extra_scripts` feature to automatically include source files from `../Vehicule/src/`:

1. **Headers**: Automatically found via `-I` flags in `platformio.ini` pointing to `../Vehicule/src/`
2. **Source Files**: Automatically compiled via `extra_script.py` which:
   - Scans `control/`, `drivers/`, `communication/`, `shared/`, and `safety/` directories
   - Includes all `.cpp` files except excluded ones (servo_driver, ultrasonic_driver, etc.)
   - No need to manually list each file - just add new files to Vehicule and they'll be included automatically

This means you only need to maintain source files in `Vehicule/src/` - no duplication needed!



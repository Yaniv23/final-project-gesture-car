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


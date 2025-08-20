# car_gesture_project

A hands-on project that connects a webcam + MediaPipe hand-gesture tracker to microcontroller sketches (ESP32 / Arduino) to control a mecanum-wheeled car and supporting demo sketches.

This README explains project purpose, dependencies, how to run the hand tracker and serial monitor, and a per-file explanation of the repository contents.


## Short description

- The Python code captures hand gestures using MediaPipe and OpenCV, maps gestures to simple commands, and sends those commands over serial to an ESP32 and to microcontroller sketches that drive motors or forward the commands via ESP-NOW.

- There is also a Video broadcasting that is under developement who as to bee send from ESP32-S3 on board to the PC.yaniv

## Project file map and explanations

Top-level files/folders (each entry explains what the file does):

- `Serial_Frompc_to_arduino.java`
  - Java program whose name indicates it acts as a serial bridge from PC to Arduino. Inspect the file for exact behavior.

- `mediapipe_hand_direction/`
  - `hand_direction_tracker.py`  Main Python hand tracker. Uses MediaPipe to detect hand keypoints, derives gesture labels (e.g., Forward, Backward, Stop, rotate_cw, rotate_ccw, Center, etc.). Stabilizes labels across frames and writes chosen labels to the configured serial port (change COM port and baud inside the script).
  - `serial_monitor.py`  Simple Python script to open a serial port and print incoming lines. Useful for debugging what the microcontroller prints back.

- `Wifi_ESP32_Com_Serial/`
  - `Wifi_ESP32_Com_Serial.ino`  ESP32 sketch that connects to WiFi and exposes an HTTP endpoint (like `/send?cmd=`). It forwards received HTTP query commands to `Serial1` (hardware UART) and supports a Serial <-> Serial1 passthrough. Use when you want to control the car remotely via HTTP.

- `Servo_and_Sensor/`
  - `Servo_and_Sensor.ino`  Example Arduino sketch demonstrating a servo sweep and an ultrasonic distance sensor (HC-SR04). Prints angles and measured distances and flags when an object is within a threshold.

- `Sender_Code/`
  - `Sender_Code.ino`  ESP32 ESP-NOW sender example. Reads lines from Serial and sends them as `struct_message` via ESP-NOW to a configured receiver MAC address. Includes send callback and peer configuration logic.

- `Reciver_Code/`
  - `Reciver_Code.ino`  ESP32 ESP-NOW receiver example. Receives ESP-NOW messages, prints sender MAC and received payload, and can trigger local motion routines.

- `Motor_Control/`
  - `Motor_Control.ino`  ESP32 sketch that drives a 4-motor mecanum drive (L298N or similar drivers). It maps incoming commands (strings or structured messages) to movement functions such as:
    - `moveForward`, `moveBackward`
    - `slideLeft`, `slideRight`
    - `rotateLeft`, `rotateRight`
    - `stopAll`
  - Uses ESP32 `ledc` PWM API to control motor speed. This sketch can receive commands via ESP-NOW (receiver) or other serial transports depending on how you wire it up.

- `Motor_Control_Serial_Com/`
  - `Motor_Control_Serial_Com.ino`  Arduino/MCU sketch that accepts numeric ASCII commands over Serial (integers 0..12) and executes corresponding motion routines (stop, forward, backward, sideways, pivots, rotates, diagonals). It parses lines from the serial stream and dispatches to functions like `Forward()`, `Backward()`, `rotate_cw()`, etc. This is useful when your controlling host sends simple numeric codes.

- `Motor_joystick_connection_test/`
  - `platformio.ini`  PlatformIO project configuration for the joystick test.
  - `src/main.cpp`  Maps two analog inputs (joystick/potentiometers) into simulated drive messages (power/steer) and prints them on Serial for testing.

- `Motor_Test/`
  - `platformio.ini` and `src/main.cpp`  Small test project to toggle motor pins and print simple status strings like `FORWARD`, `STOP`, `BACKWARD` for wiring verification.

- `Reciver_Code/`, `Sender_Code/`, `Motor_Control/`, `Motor_Control_Serial_Com/`
  - These sketches are complementary: choose one messaging pattern for your project (ESP-NOW string messages, WiFi HTTP bridge, or numeric serial commands) and ensure the tracker script emits the matching format.

## Gesture labels vs motor command formats

- The Python tracker typically writes human-readable labels (e.g., `Forward`, `Stop`, `rotate_ccw`, `rotate_cw`, `Center`, or directional labels from `get_direction_label`).
- The motor sketches accept either:
  - String commands (ESP-NOW or Serial strings)  used by `Sender_Code.ino`/`Reciver_Code.ino`/`Motor_Control.ino`.
  - Numeric ASCII commands  consumed by `Motor_Control_Serial_Com.ino`.

Integration options:
- Map labels to integers in `mediapipe_hand_direction/hand_direction_tracker.py` and send numeric ASCII codes matching `Motor_Control_Serial_Com.ino`.
- Or modify the motor sketch to parse string labels instead of numeric codes.

Suggested mapping (example mapping you can adopt):

- 0 = Stop
- 1 = Forward
- 2 = Backward
- 3 = Sideway_Left
- 4 = Sideway_Right
- 5 = pivot_left
- 6 = pivot_right
- 7 = rotate_cw
- 8 = rotate_ccw
- 9 = diagonal_forward_left
- 10 = diagonal_forward_right
- 11 = diagonal_backward_left
- 12 = diagonal_backward_right

## Tips and debugging

- Ensure serial port and baud rate match across sender and receiver. Update COM port in `hand_direction_tracker.py` and `serial_monitor.py`.
- Use `mediapipe_hand_direction/serial_monitor.py` to confirm what the microcontroller prints back.
- Use the Arduino / PlatformIO serial monitor (e.g., 115200 baud) to view debug prints from ESP32 sketches.
- If using ESP-NOW, verify both sender and receiver MAC addresses and WiFi mode (STA for ESP-NOW peers).

## Suggested small improvements

- Add a config JSON/TOML to map gestures to command formats (string vs numeric) and to store serial port/baud settings.
- Add a small GUI to display recognized gestures and provide manual override buttons.
- Translate gesture displacement or distance-to-center into motor speed PWM values for proportional control.

## License and attribution

- No license is included in this repository. Add an appropriate `LICENSE` file if you want to publish or share this project under a specific license.

## Completion

- Created `README.md` with an overview, setup, per-file explanations, run instructions, and tips. If you want, I can also:
  - Patch `mediapipe_hand_direction/hand_direction_tracker.py` to emit numeric codes matching `Motor_Control_Serial_Com.ino`.
  - Commit the README and open a PR.

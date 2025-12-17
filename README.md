# Gesture-Controlled Car Project

A hands-on project that connects a webcam + MediaPipe hand-gesture tracker to ESP32 microcontrollers to control a mecanum-wheeled car with onboard camera streaming.

## System Architecture

The project consists of three main components:

1. **PC Module** - Runs hand tracking and sends control commands
2. **Vehicle Controller** - ESP32 on the car that receives commands and controls motors + transmission
3. **Camera Module** - ESP32-S3 on the car that streams video to the PC

## Communication Architecture


### Connection Details

| Connection | Protocol | Details |
|------------|----------|---------|
| **PC → ESP32 Sender** | USB Serial | Baud rate: 115200, Port: Configured in `Hand_Tracking/constant.py` |
| **ESP32 Sender → Vehicle Controller** | ESP-NOW | Wireless 2.4GHz, MAC address configured in sender code |
| **ESP32-S3 Camera → PC** | WiFi HTTP | MJPEG stream on port 80, IP address displayed in Serial Monitor |

## Main Project Components

### `Hand_Tracking/`
The PC-side hand gesture recognition system.

- **`Hand_Tracker.py`** - Main Python script that:
  - Captures video from webcam using OpenCV
  - Uses MediaPipe to detect hand landmarks and gestures
  - Recognizes gestures: Forward, Backward, Stop, rotate_cw, rotate_ccw, Sideway_Left, Sideway_Right, diagonal movements, Center
  - Applies stability filtering to prevent command jitter
  - Sends gesture commands via USB Serial to ESP32 sender
  - Configure serial port in `constant.py`

- **`constant.py`** - Configuration file for serial communication (COM port and baud rate)

- **`serial_monitor.py`** - Debugging tool to monitor serial communication

- **`requirements.txt`** - Python dependencies (MediaPipe, OpenCV, pyserial)

### `Vehicule_Controller/`
The main ESP32 controller on the car.

- **`Vehicule_Controller.ino`** - ESP32 sketch that:
  - Receives ESP-NOW commands from the sender ESP32
  - Parses string commands (e.g., "Forward", "Stop", "rotate_cw") into integer codes
  - Controls 4-motor mecanum drive system (L298N drivers)
  - Controls servo motor for scanning
  - Reads ultrasonic distance sensor (HC-SR04)
  - Supports all movement types: forward, backward, strafe, rotate, diagonal, pivot

**Motor Control:**
- Front Right, Front Left, Back Right, Back Left wheels
- PWM speed control (slow: 150, fast: 255)
- Individual wheel control for mecanum movement

**Sensor Integration:**
- Servo sweeps 0-60° for obstacle scanning
- Ultrasonic sensor detects obstacles within 20cm threshold

### `ESP_Camera_Module/`
Onboard camera system for video streaming.

- **`src/main.cpp`** - ESP32-S3 sketch that:
  - Initializes OV2640 camera module
  - Connects to WiFi network (configure SSID/password in code)
  - Serves MJPEG video stream via HTTP on port 80
  - Provides web interface at root URL (`http://<ESP32_IP>/`)
  - Stream endpoint: `http://<ESP32_IP>/stream`

- **`src/camera_viewer.py`** - PC-side Python viewer that:
  - Connects to ESP32-S3 camera stream via HTTP
  - Displays live video feed using OpenCV
  - Configure ESP32 IP address in script or via `--ip` argument

## Testing/Development Folders

These folders contain individual test sketches for developing and debugging specific functions separately:

### `Transmission/`
- **`Sender_Code/`** - ESP32 ESP-NOW sender (used in main system)
- **`Reciver_Code/`** - ESP32 ESP-NOW receiver test example
- **`Wifi_ESP32_Com_Serial/`** - WiFi HTTP bridge test (alternative to ESP-NOW)

### `Motors/`
- **`Motor_Control/`** - Basic motor control test sketch
- **`Motor_Control_Serial_Com/`** - Serial command parser test
- **`Motor_Test/`** - Simple motor pin toggle test
- **`Motor_joystick_connection_test/`** - Joystick input mapping test

### `Sensors/`
- **`Servo_and_Sensor/`** - Servo and ultrasonic sensor test sketch

## Setup Instructions

### 1. PC Setup (Hand Tracking)

```bash
cd Hand_Tracking
pip install -r requirements.txt
```

Edit `constant.py` to set your ESP32 sender's COM port:
```python
COM_PORT = 'COM11'  # Change to your port (e.g., '/dev/ttyUSB0' on Linux)
BAUD_RATE = 115200
```

### 2. ESP32 Sender Setup

1. Upload `Transmission/Sender_Code/Sender_Code.ino` to an ESP32
2. Connect ESP32 to PC via USB
3. Update receiver MAC address in sender code (line 4) to match Vehicle Controller MAC
4. Open Serial Monitor (115200 baud) to verify connection

### 3. Vehicle Controller Setup

1. Upload `Vehicule_Controller/Vehicule_Controller.ino` to ESP32 on car
2. Wire motors, servo, and sensor according to pin definitions in code
3. Open Serial Monitor (115200 baud) to see MAC address
4. Copy MAC address to ESP32 sender code

### 4. Camera Module Setup

1. Upload `ESP_Camera_Module/src/main.cpp` to ESP32-S3
2. Configure WiFi SSID and password in code (lines 8-9)
3. Open Serial Monitor to see assigned IP address
4. Update IP in `camera_viewer.py` or use `--ip` argument

## Running the System

### Start Hand Tracking
```bash
cd Hand_Tracking
python Hand_Tracker.py
```

The script will:
- Open webcam feed
- Detect hand gestures
- Send commands to ESP32 sender via Serial
- Display recognized gesture on screen

### View Camera Stream
```bash
cd ESP_Camera_Module/src
python camera_viewer.py
# Or with custom IP:
python camera_viewer.py --ip 192.168.1.100
```

## Gesture Commands

The hand tracker recognizes the following gestures:

| Gesture | Command String | Motor Action |
|---------|---------------|--------------|
| Closed fist | `Stop` | All motors stop |
| Index finger up | `Forward` | Move forward |
| Index finger down | `Backward` | Move backward |
| Hand open, centered | `Center` | Stop (neutral) |
| Thumb-index circle (right) | `rotate_cw` | Rotate clockwise |
| Thumb-index circle (left) | `rotate_ccw` | Rotate counter-clockwise |
| Hand position-based | `Sideway_Left/Right` | Strafe left/right |
| Hand position-based | `diagonal_forward_left/right` | Diagonal forward |
| Hand position-based | `diagonal_backward_left/right` | Diagonal backward |

## Troubleshooting

### Hand Tracking Issues
- **No serial connection**: Check COM port in `constant.py` matches your ESP32 sender
- **Commands not sending**: Verify ESP32 sender is connected and Serial Monitor shows "🟢 Sender ready"
- **Gesture not recognized**: Ensure good lighting and clear hand visibility

### ESP-NOW Communication Issues
- **Commands not received**: Verify MAC addresses match between sender and receiver
- **Connection fails**: Ensure both ESP32s are in WiFi STA mode
- **Check Serial Monitor**: Vehicle Controller prints received commands with MAC address
ands are being received (check Serial Monitor)

## Hardware Requirements

### PC
- Webcam (USB)
- Python 3.7+
- USB port for ESP32 sender

### Vehicle Controller (ESP32)
- ESP32 development board
- 4x DC motors with L298N drivers (or similar)
- Servo motor (for scanning)
- HC-SR04 ultrasonic sensor
- Power supply for motors

### Camera Module (ESP32-S3)
- ESP32-S3 development board
- OV2640 camera module
- WiFi network access

### ESP32 Sender
- ESP32 development board
- USB connection to PC

## License

No license is included in this repository. Add an appropriate `LICENSE` file if you want to publish or share this project under a specific license.

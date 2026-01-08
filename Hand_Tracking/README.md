# Hand Tracking System

PC-side hand gesture recognition system using MediaPipe and OpenCV. This component captures video from a webcam, detects hand gestures, and sends control commands to the ESP32 sender via USB Serial.

## Overview

The hand tracking system uses **Google MediaPipe** to detect hand landmarks in real-time and recognizes specific gestures to control the vehicle. Gestures are filtered for stability before being sent as commands.

### Key Features

- **Real-time Hand Detection**: MediaPipe hand tracking at 30+ FPS
- **Gesture Recognition**: 12+ gesture types (forward, backward, stop, rotate, strafe, diagonal)
- **Stability Filtering**: Prevents command jitter with frame-based confirmation
- **Serial Communication**: USB Serial connection to ESP32 sender
- **Visual Feedback**: On-screen display of detected gestures and hand landmarks

## Installation

### Prerequisites

- **Python**: Version 3.7+ (3.10 recommended for MediaPipe compatibility)
- **Webcam**: USB webcam (built-in or external)
- **USB Port**: For ESP32 sender connection

### Install Dependencies

**Option 1: Using pip (system-wide)**
```bash
# From project root
pip install -r requirements.txt
```

**Option 2: Using virtual environment (recommended)**
```bash
# From project root or Hand_Tracking directory
cd Hand_Tracking
python -m venv venv

# Linux/Mac:
source venv/bin/activate

# Windows:
venv\Scripts\activate

# Install dependencies (from project root)
cd ..  # Go to project root if in Hand_Tracking
pip install -r requirements.txt
```

### Required Packages

- `opencv-python>=4.5.0` - Computer vision and video capture
- `numpy>=1.19.0` - Numerical computing (used by camera viewer)
- `mediapipe==0.10.9` - Hand tracking (specific version for compatibility)
- `pyserial>=3.5` - Serial communication with ESP32

## Configuration

### Serial Port Setup

Edit `constant.py` to configure the serial port:

```python
# Linux/Mac:
COM_PORT = '/dev/ttyUSB0'  # or '/dev/ttyACM0'

# Windows:
COM_PORT = 'COM3'  # or 'COM11', etc.

BAUD_RATE = 115200  # Must match ESP32 sender baud rate
```

### Finding Your COM Port

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**Windows:**
- Open Device Manager → Ports (COM & LPT)
- Look for "USB Serial Port" or "CP210x" or "CH340"

**macOS:**
```bash
ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART*
```

## Usage

### Basic Usage

```bash
cd Hand_Tracking
python Hand_Tracker.py
```

### With Virtual Environment

```bash
cd Hand_Tracking
source venv/bin/activate  # Linux/Mac
# or: venv\Scripts\activate  # Windows
python Hand_Tracker.py
```

### Running Without Serial Connection

The script will run in "camera-only mode" if the ESP32 sender is not connected. Hand tracking will still work, but commands won't be sent.

## Gesture Recognition

### Supported Gestures

| Gesture | Command | Description |
|---------|---------|-------------|
| 👊 Closed Fist | `Stop` | All fingers bent (thumb-index circle not detected) |
| 👆 Index Up | `Forward` | Index finger extended, others bent |
| 👇 Index Down | `Backward` | Index finger pointing down |
| ✋ Open Hand (Center) | `Center` | All fingers extended, hand near center |
| 🔄 Circle Right | `rotate_cw` | Thumb-index circle gesture (clockwise) |
| 🔄 Circle Left | `rotate_ccw` | Thumb-index circle gesture (counter-clockwise) |
| Hand Position | `Sideway_Left` | Open hand, positioned left of center |
| Hand Position | `Sideway_Right` | Open hand, positioned right of center |
| Hand Position | `diagonal_forward_left` | Open hand, diagonal forward-left |
| Hand Position | `diagonal_forward_right` | Open hand, diagonal forward-right |
| Hand Position | `diagonal_backward_left` | Open hand, diagonal backward-left |
| Hand Position | `diagonal_backward_right` | Open hand, diagonal backward-right |

### Gesture Detection Logic

1. **Hand Detection**: MediaPipe detects hand landmarks (21 points)
2. **Gesture Classification**: 
   - Finger states (extended/bent) determine basic gestures
   - Hand position relative to center determines direction
   - Circle gestures detected by thumb-index proximity
3. **Stability Filter**: Gesture must be detected for 10 consecutive frames before sending
4. **Command Transmission**: Stable gesture sent via Serial to ESP32 sender

### Visual Feedback

The display shows:
- **Hand Landmarks**: Green dots and connections showing hand skeleton
- **Hand Center**: Green circle at hand center point
- **Direction Line**: Red line from screen center to hand center
- **Detected Gesture**: Text label showing current command
- **Center Zone**: Circle showing neutral zone

## Serial Communication

### Protocol

Commands are sent as **text strings** terminated with newline (`\n`):

```
Forward\n
Stop\n
rotate_cw\n
```

### Command Format

- **Format**: ASCII text string
- **Termination**: Newline character (`\n`)
- **Encoding**: UTF-8
- **Baud Rate**: 115200

### Stability Filtering

To prevent command jitter:
- Gesture must be detected for **10 consecutive frames** before sending
- New gesture resets the counter
- Only stable gestures are transmitted

## Troubleshooting

### Camera Issues

**Problem**: "Could not open video stream"
- **Solution**: Check webcam is connected and not used by another application
- **Check**: Try different camera indices (0, 1, 2) in code if multiple cameras

**Problem**: Low FPS or laggy video
- **Solution**: Reduce camera resolution in code
- **Check**: Close other applications using the camera
- **Check**: Ensure adequate lighting

### Serial Connection Issues

**Problem**: "Could not connect to COM_PORT"
- **Solution**: Verify COM port in `constant.py` matches your ESP32 sender
- **Check**: ESP32 sender is connected via USB
- **Check**: No other program is using the serial port (close Serial Monitor)

**Problem**: "Serial write error"
- **Solution**: Check ESP32 sender is powered on
- **Check**: USB cable supports data (not just power)
- **Solution**: Try reconnecting USB cable

**Problem**: Commands not being sent
- **Solution**: Check Serial Monitor on ESP32 sender shows "🟢 Sender ready"
- **Check**: Hand tracking window shows detected gestures
- **Check**: Stability filter may be preventing transmission (wait longer)

### Gesture Recognition Issues

**Problem**: Gesture not recognized
- **Solution**: Ensure good lighting (avoid backlighting)
- **Solution**: Keep hand clearly visible in frame
- **Solution**: Move hand away from edges of frame
- **Check**: All fingers should be clearly visible

**Problem**: Wrong gesture detected
- **Solution**: Make gestures more distinct
- **Solution**: Hold gesture steady for stability filter
- **Check**: Hand position relative to center affects direction commands

**Problem**: Commands sent too frequently
- **Solution**: Increase `stable_threshold` in code (default: 10 frames)
- **Note**: Higher threshold = more stable but slower response

### Performance Issues

**Problem**: High CPU usage
- **Solution**: Reduce camera resolution
- **Solution**: Lower MediaPipe model complexity (if available)
- **Check**: Close other applications

**Problem**: Frame drops
- **Solution**: Ensure adequate lighting
- **Solution**: Use USB 3.0 port for webcam if available
- **Check**: Webcam driver is up to date

## Code Structure

```
Hand_Tracking/
├── Hand_Tracker.py      # Main tracking script
├── constant.py          # Serial port configuration
├── serial_monitor.py    # Debugging tool (optional)
└── README.md           # This file

Note: requirements.txt is located in the project root directory.
```

### Key Functions

- `is_hand_closed()` - Detects closed fist
- `is_index_finger_up()` - Detects index finger up
- `is_index_finger_down()` - Detects index finger down
- `is_hand_open()` - Detects open hand
- `is_circle_cw()` / `is_circle_ccw()` - Detects circle gestures
- `get_direction_label()` - Determines direction based on hand position

## Advanced Configuration

### Adjusting Stability Threshold

Edit `Hand_Tracker.py`:

```python
stable_threshold = 10  # Frames needed to confirm gesture
```

- **Lower value** (e.g., 5): Faster response, more jitter
- **Higher value** (e.g., 15): Slower response, more stable

### Changing Camera Index

If you have multiple cameras, change the camera index:

```python
cap = cv2.VideoCapture(0)  # Try 1, 2, etc. for other cameras
```

### Adjusting Center Threshold

Change the neutral zone size:

```python
center_threshold = 60  # Pixels from center for "Center" gesture
```

## Integration with System

The hand tracker is part of the complete system:

1. **Hand Tracker** → USB Serial → **ESP32 Sender**
2. **ESP32 Sender** → ESP-NOW → **Vehicle Controller**
3. **Vehicle Controller** → Controls motors

See [Main README](../README.md) for complete system setup.

## Technical Details

For technical information about the hand tracking algorithm and integration:
- [PC Side Components Technical Details](../docs/PC_SIDE_COMPONENTS.md)

---

**Press 'q' in the hand tracking window to quit the application.**


# ESP32 Sender

ESP32-based wireless bridge that receives commands from the PC via USB Serial and forwards them to the vehicle controller via ESP-NOW.

## Overview

The ESP32 sender acts as a **wireless bridge** between the PC and the vehicle:

1. **Receives** text commands from PC via USB Serial
2. **Converts** text commands to binary format
3. **Transmits** commands to vehicle controller via ESP-NOW

### Key Features

- **USB Serial Interface**: Receives commands from PC hand tracker
- **ESP-NOW Protocol**: Fast, low-latency wireless communication
- **Simple Protocol**: Text string to binary command conversion
- **Reliable**: Automatic retry and error handling

## Hardware Requirements

- **ESP32 Development Board**: Any ESP32 board (e.g., ESP32 DevKit)
- **USB Cable**: For PC connection (data-capable, not just power)

## Software Setup

### Option 1: Arduino IDE

1. **Install Arduino IDE**: Download from [arduino.cc](https://www.arduino.cc/)
2. **Install ESP32 Board Support**:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install
3. **Select Board**: Tools → Board → ESP32 Arduino → Your board model
4. **Select Port**: Tools → Port → Your COM port

### Option 2: PlatformIO

Create a `platformio.ini` in this directory:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
upload_port = /dev/ttyUSB0  # Adjust for your system
monitor_speed = 115200
```

Then:
```bash
pio run -t upload
```

## Configuration

### MAC Address Setup

**Critical**: You must configure the vehicle controller's MAC address in the sender code.

1. **Get Vehicle MAC Address**:
   - Upload vehicle controller code
   - Open Serial Monitor (115200 baud)
   - Look for: `[COMM] MAC Address: XX:XX:XX:XX:XX:XX`
   - Copy this MAC address

2. **Update Sender Code**:

Edit `Sender_Code.ino`:

```cpp
// Replace with your vehicle controller's MAC address
uint8_t receiverMAC[] = {0x00, 0x4B, 0x12, 0x34, 0xF7, 0xF4};
```

**Format**: 6 bytes in hexadecimal, separated by commas

### Serial Port Configuration

The sender uses:
- **Baud Rate**: 115200 (fixed in code)
- **Port**: Configured in PC hand tracker (`Hand_Tracking/constant.py`)

## Upload Instructions

### Using Arduino IDE

1. Open `Sender_Code.ino` in Arduino IDE
2. Select your ESP32 board (Tools → Board)
3. Select COM port (Tools → Port)
4. Click **Upload** button
5. Wait for "Done uploading" message

### Using PlatformIO

```bash
cd Transmission/Sender_Code
pio run -t upload
```

### Finding Your COM Port

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**Windows:**
- Device Manager → Ports (COM & LPT)
- Look for "USB Serial Port" or "CP210x" or "CH340"

**macOS:**
```bash
ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART*
```

## Verification

### Serial Monitor Test

1. **Open Serial Monitor** (115200 baud)
2. **Look for**: `🟢 Sender ready`
3. **Send test command** from PC hand tracker
4. **Verify**: Serial Monitor shows `📤 Sent: <command>`

### Expected Serial Output

```
🟢 Sender ready
📤 Sent: Forward
✅ Sent
📤 Sent: Stop
✅ Sent
```

## Communication Protocol

### PC → Sender (USB Serial)

**Format**: Text string terminated with newline

```
Forward\n
Stop\n
rotate_cw\n
```

**Commands**: See [Hand Tracking README](../Hand_Tracking/README.md#gesture-recognition) for full list.

### Sender → Vehicle (ESP-NOW)

**Format**: Binary struct with command string

```cpp
struct struct_message {
  char command[32];  // Text command string
};
```

The vehicle controller converts text commands to binary command bytes internally.

## Troubleshooting

### Upload Issues

**Problem**: "Failed to connect to ESP32"
- **Solution**: Press and hold BOOT button during upload
- **Check**: USB cable supports data (not just power)
- **Solution**: Try different USB port/cable
- **Check**: Correct COM port selected

**Problem**: "Board not found"
- **Solution**: Install ESP32 board support in Arduino IDE
- **Check**: Correct board selected in Tools → Board
- **Solution**: Install USB drivers (CP210x or CH340)

### Serial Communication Issues

**Problem**: "Sender ready" not appearing
- **Solution**: Check Serial Monitor baud rate is 115200
- **Check**: ESP32 is powered on
- **Solution**: Try resetting ESP32 (press RESET button)

**Problem**: Commands not being sent
- **Solution**: Verify PC hand tracker is connected to correct COM port
- **Check**: Serial Monitor shows "🟢 Sender ready"
- **Check**: Hand tracker is detecting gestures

**Problem**: "❌ Failed" in Serial Monitor
- **Solution**: Verify vehicle controller MAC address is correct
- **Check**: Vehicle controller is powered on
- **Check**: Both ESP32s are on same WiFi channel (default: 0)
- **Solution**: Check vehicle controller Serial Monitor for reception

### ESP-NOW Issues

**Problem**: Commands not received by vehicle
- **Solution**: Verify MAC address matches vehicle controller
- **Check**: Vehicle controller Serial Monitor shows ESP-NOW initialization
- **Check**: Both ESP32s are powered on
- **Solution**: Ensure both ESP32s are in WiFi STA mode

**Problem**: Intermittent communication
- **Solution**: Check WiFi interference (2.4GHz devices)
- **Solution**: Move ESP32s closer together
- **Check**: Power supply is stable

## Code Structure

```
Sender_Code/
├── Sender_Code.ino    # Main sender code
└── README.md          # This file
```

### Key Components

- **ESP-NOW Initialization**: Sets up wireless communication
- **Serial Handler**: Reads commands from PC
- **Command Forwarding**: Sends commands to vehicle via ESP-NOW

## Integration with System

The sender is part of the complete communication chain:

1. **PC Hand Tracker** → USB Serial → **ESP32 Sender** (this component)
2. **ESP32 Sender** → ESP-NOW → **Vehicle Controller**
3. **Vehicle Controller** → Controls motors

See [Main README](../../README.md) for complete system setup.

## Testing

### Manual Test

1. **Upload sender code** to ESP32
2. **Open Serial Monitor** (115200 baud)
3. **Type command** in Serial Monitor input:
   ```
   Forward
   ```
4. **Press Enter**
5. **Verify**: Shows "📤 Sent: Forward" and "✅ Sent"

### Integration Test

1. **Configure MAC address** in sender code
2. **Upload sender code**
3. **Start hand tracker** on PC
4. **Make gestures** in front of webcam
5. **Verify**: Serial Monitor shows commands being sent
6. **Check**: Vehicle responds to commands

## Advanced Configuration

### ESP-NOW Channel

Default channel is 0. To change:

```cpp
peerInfo.channel = 0;  // Change to 1-13 if needed
```

**Note**: Vehicle controller must use the same channel.

### Serial Timeout

Adjust serial read timeout if needed:

```cpp
String input = Serial.readStringUntil('\n');  // Default timeout
```

### Command Buffer Size

Current buffer is 32 characters. To change:

```cpp
char command[32];  // Increase if needed for longer commands
```

## Performance

- **Latency**: < 10ms typical (Serial + ESP-NOW)
- **Reliability**: High (ESP-NOW is connectionless but reliable)
- **Range**: ~100-200m line-of-sight (depends on environment)

## Safety Notes

- **No Safety Features**: The sender is a simple bridge with no safety checks
- **Command Validation**: Vehicle controller validates commands
- **Emergency Stop**: Use vehicle controller's emergency stop feature

---

**The sender must be configured with the correct vehicle MAC address to work!**


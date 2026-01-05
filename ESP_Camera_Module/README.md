# ESP32-S3 Camera Module

Onboard camera system for live video streaming. The ESP32-S3 captures video from an OV2640 camera module and streams it to your PC over WiFi using MJPEG over HTTP.

## Overview

The camera module provides real-time video streaming from the vehicle, allowing you to see what the car sees. The stream is accessible via web browser or the included Python viewer.

### Key Features

- **Live Video Streaming**: MJPEG stream over HTTP
- **WiFi Connectivity**: Connects to your local WiFi network
- **Web Interface**: View stream in any web browser
- **Python Viewer**: Optional OpenCV-based viewer included
- **Low Latency**: Optimized for real-time viewing

## Hardware Setup

### Required Components

- **ESP32-S3 Development Board**: With camera support (e.g., ESP32-S3-CAM)
- **OV2640 Camera Module**: Connected to ESP32-S3
- **WiFi Access**: 2.4GHz WiFi network

### Camera Pin Connections (OV2640)

The camera module connects to ESP32-S3 via the following pins:

| Camera Pin | ESP32-S3 GPIO | Function |
|------------|--------------|----------|
| PWDN | -1 (not used) | Power down |
| RESET | -1 (not used) | Reset |
| XCLK | GPIO 15 | Clock |
| SIOD | GPIO 4 | I2C Data |
| SIOC | GPIO 5 | I2C Clock |
| Y9 | GPIO 16 | Data bit 9 |
| Y8 | GPIO 17 | Data bit 8 |
| Y7 | GPIO 18 | Data bit 7 |
| Y6 | GPIO 12 | Data bit 6 |
| Y5 | GPIO 10 | Data bit 5 |
| Y4 | GPIO 8 | Data bit 4 |
| Y3 | GPIO 9 | Data bit 3 |
| Y2 | GPIO 11 | Data bit 2 |
| VSYNC | GPIO 6 | Vertical sync |
| HREF | GPIO 7 | Horizontal reference |
| PCLK | GPIO 13 | Pixel clock |

> **Note**: Pin assignments may vary depending on your ESP32-S3 board. Check your board's documentation and adjust in `src/main.cpp` if needed.

## WiFi Configuration

### Setting WiFi Credentials

Edit `src/main.cpp` and update the WiFi credentials:

```cpp
const char *WIFI_SSID     = "YourWiFiNetwork";
const char *WIFI_PASSWORD = "YourWiFiPassword";
```

### Network Requirements

- **WiFi Standard**: 2.4GHz (ESP32-S3 doesn't support 5GHz)
- **Network Type**: WPA2 or WPA (most home networks)
- **Same Network**: PC and ESP32-S3 must be on the same WiFi network

## Building and Uploading

### Prerequisites

- **PlatformIO**: Install with `pip install platformio`
- **USB Cable**: For connecting ESP32-S3 to PC
- **Board Support**: ESP32-S3 platform support in PlatformIO

### Build Instructions

```bash
cd ESP_Camera_Module

# Build the project
pio run

# Build and upload to ESP32-S3
pio run -t upload

# Monitor serial output (to see IP address)
pio device monitor
```

### Port Configuration

Edit `platformio.ini` to set your upload port:

```ini
upload_port = /dev/ttyACM0  # Linux (adjust as needed)
# upload_port = COM3        # Windows
# upload_port = /dev/cu.usbserial-*  # macOS
```

Or specify at upload time:
```bash
pio run -t upload --upload-port /dev/ttyACM0
```

### Board Configuration

The project uses a custom board configuration. Ensure `boards/esp32cam_s3_wroom_n16r8.json` exists, or update `platformio.ini` with your board:

```ini
[env:esp32cam_s3_wroom_n16r8]
platform = espressif32@6.9.0
board = esp32cam_s3_wroom_n16r8
framework = arduino
```

## IP Address Discovery

After uploading and connecting to WiFi, the ESP32-S3 will print its IP address to the Serial Monitor:

```
WiFi connected!
IP address: 192.168.1.100
Starting web server on port 80...
```

**Important**: Note this IP address - you'll need it to access the stream!

### Finding IP Address

**Method 1: Serial Monitor**
- Open Serial Monitor (115200 baud)
- Look for "IP address: XXX.XXX.XXX.XXX"

**Method 2: Router Admin Panel**
- Check connected devices in your router's admin panel
- Look for device named "ESP32" or similar

**Method 3: Network Scanner**
- Use network scanning tools (e.g., `nmap` on Linux)
- Scan your local network for new devices

## Viewing the Stream

### Method 1: Web Browser

Simply open the stream URL in any web browser:

```
http://<ESP32_IP>/stream
```

Or view the web interface:

```
http://<ESP32_IP>/
```

The web interface provides a simple HTML page with the video stream embedded.

### Method 2: Python Viewer

Use the included Python viewer for OpenCV-based display:

```bash
cd ESP_Camera_Module/src
python camera_viewer.py --ip 192.168.1.100
```

Or edit `camera_viewer.py` to set default IP:

```python
ESP32_IP = "192.168.1.100"  # Your ESP32-S3 IP address
```

Then run:
```bash
python camera_viewer.py
```

### Python Viewer Requirements

The viewer requires OpenCV:

```bash
pip install opencv-python
```

## Stream Endpoints

The camera module provides two HTTP endpoints:

### Root Endpoint (`/`)
- **Type**: HTML page
- **Content**: Web interface with embedded video stream
- **URL**: `http://<ESP32_IP>/`

### Stream Endpoint (`/stream`)
- **Type**: MJPEG stream
- **Content**: Continuous video stream
- **URL**: `http://<ESP32_IP>/stream`
- **Format**: multipart/x-mixed-replace (MJPEG)

## Troubleshooting

### WiFi Connection Issues

**Problem**: "WiFi connection failed"
- **Solution**: Verify SSID and password in `src/main.cpp`
- **Check**: WiFi network is 2.4GHz (not 5GHz)
- **Check**: WiFi signal strength (move ESP32-S3 closer to router)
- **Solution**: Check Serial Monitor for specific error messages

**Problem**: "IP address not assigned"
- **Solution**: Wait longer for DHCP (can take 10-30 seconds)
- **Check**: Router has available IP addresses
- **Check**: WiFi credentials are correct

### Camera Issues

**Problem**: "Camera init failed"
- **Solution**: Check camera pin connections
- **Check**: Camera module is properly powered
- **Solution**: Verify pin assignments match your board
- **Check**: Camera module is compatible (OV2640)

**Problem**: "No image" or black screen
- **Solution**: Check camera lens is not covered
- **Check**: Adequate lighting
- **Solution**: Verify camera initialization in Serial Monitor

### Stream Access Issues

**Problem**: Cannot connect to stream URL
- **Solution**: Verify IP address from Serial Monitor
- **Check**: PC and ESP32-S3 are on same WiFi network
- **Check**: Firewall is not blocking port 80
- **Solution**: Try accessing from different device/network

**Problem**: Stream is slow or laggy
- **Solution**: Check WiFi signal strength
- **Solution**: Reduce camera resolution in code (if needed)
- **Check**: Network bandwidth (other devices using WiFi)
- **Solution**: Move ESP32-S3 closer to router

**Problem**: Stream disconnects frequently
- **Solution**: Check WiFi signal stability
- **Solution**: Reduce stream quality/resolution
- **Check**: Power supply is stable (use external power if needed)

### Upload Issues

**Problem**: Upload fails
- **Solution**: Check USB cable supports data (not just power)
- **Solution**: Press and hold BOOT button during upload
- **Check**: Correct COM port in `platformio.ini`
- **Solution**: Try different USB port/cable

**Problem**: Board not recognized
- **Solution**: Install ESP32-S3 USB drivers (CP210x or CH340)
- **Check**: Board is powered on
- **Solution**: Check Device Manager (Windows) or `lsusb` (Linux)

## Configuration Options

### Camera Resolution

Default resolution is set by the camera module. To change, modify camera configuration in `src/main.cpp`:

```cpp
config.frame_size = FRAMESIZE_VGA;  // Options: QQVGA, QVGA, VGA, etc.
```

### Stream Quality

Adjust JPEG quality (affects bandwidth and quality):

```cpp
config.jpeg_quality = 12;  // 0-63, lower = higher quality
```

### Frame Rate

Frame rate depends on:
- Camera resolution
- WiFi bandwidth
- Processing power

Typical: 10-30 FPS depending on settings.

## Integration with System

The camera module works independently but is part of the complete system:

1. **ESP32-S3 Camera** → WiFi HTTP → **PC (Browser/Viewer)**
2. **PC Hand Tracker** → USB Serial → **ESP32 Sender** → ESP-NOW → **Vehicle**

See [Main README](../README.md) for complete system setup.

## Code Structure

```
ESP_Camera_Module/
├── src/
│   ├── main.cpp           # Camera firmware
│   └── camera_viewer.py   # PC-side viewer
├── boards/
│   └── esp32cam_s3_wroom_n16r8.json  # Board configuration
├── platformio.ini          # Build configuration
└── README.md              # This file
```

## Technical Details

For technical information about the streaming architecture:
- [PC Side Components Technical Details](../docs/PC_SIDE_COMPONENTS.md)

## Performance Tips

- **Reduce Resolution**: Lower resolution = higher FPS and lower bandwidth
- **Optimize JPEG Quality**: Balance between quality and bandwidth
- **Stable Power**: Use external power supply for consistent performance
- **WiFi Signal**: Ensure strong WiFi signal for best performance
- **Network Load**: Reduce other network traffic for smoother streaming

---

**The camera stream is accessible at `http://<ESP32_IP>/stream` once connected to WiFi.**


# ESP-CAM (OV3660 over UDP)

Minimal ESP32-CAM firmware that captures JPEG frames from an OV3660/OV2640 sensor and streams them over UDP using the 8-byte framing expected by the existing Python viewer in pc_side/ESP_Camera_Module/src/camera_viewer.py.

## Quick start
1. Install PlatformIO (or use the VS Code extension).
2. Update WiFi credentials in [src/main.cpp](src/main.cpp) (`WIFI_SSID`, `WIFI_PASSWORD`).
3. Connect the ESP32-CAM (AI Thinker pinout) over USB.
4. Build and flash:
   ```bash
   cd ESP-CAM
   pio run -t upload
   pio device monitor
   ```
5. On your PC, start the UDP viewer (already present in this repo):
   ```bash
   cd pc_side/ESP_Camera_Module/src
   python3 camera_viewer.py
   ```
   The viewer broadcasts discovery on port 5001; the ESP replies and streams to the viewer on UDP port 5000.

## Hardware (AI Thinker ESP32-CAM pin map)

| Camera | GPIO |
| --- | --- |
| PWDN | 32 |
| RESET | -1 |
| XCLK | 0 |
| SIOD | 26 |
| SIOC | 27 |
| D7 | 35 |
| D6 | 34 |
| D5 | 39 |
| D4 | 36 |
| D3 | 21 |
| D2 | 19 |
| D1 | 18 |
| D0 | 5 |
| VSYNC | 25 |
| HREF | 23 |
| PCLK | 22 |

- XCLK set to 20 MHz, PSRAM used when available (double frame buffers).
- Default resolution: VGA, JPEG quality 12; adjust in [src/main.cpp](src/main.cpp).

## Network protocol
- Discovery: viewer sends `DISCOVER_CAMERA_VIEWER` on UDP 5001 (broadcast). Device replies `ESP32-CAM:<ip>` and learns viewer IP from `VIEWER_IP:<ip>`.
- Streaming: UDP 5000, packets with 8-byte little-endian header: `frame_id (u32)`, `fragment_id (u16)`, `total_fragments (u16)`. Fragment id `0xFFFF` is a start-of-frame marker.
- Payload size per packet: 1024 bytes of JPEG data by default.

## Notes
- Targeted at OV3660 (e.g., ESP32-CAM modules shipped with that sensor); works with OV2640 too. Sensor tuning flips OV3660 vertically and tweaks brightness/saturation.
- If discovery fails, the firmware broadcasts frames until it learns a viewer IP.
- Ports: discovery 5001 UDP, stream 5000 UDP. Ensure firewall permits inbound UDP on the viewer machine.

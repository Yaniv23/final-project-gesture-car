# 5. Technology Summary

## 5.1 Technical Stack

| Component | Technology | Version |
|-----------|-------------|---------|
| **PC** | Python | 3.10+ |
| **PC - Vision** | OpenCV | 4.5+ |
| **PC - Gestures** | MediaPipe | 0.10.9 |
| **PC - Serial** | PySerial | 3.5+ |
| **PC - UDP** | Python socket | Standard |
| **ESP32** | ESP-IDF / Arduino | Latest |
| **RTOS** | FreeRTOS | (Included) |
| **Wireless** | ESP-NOW | 2.4GHz |
| **Wireless** | WiFi UDP | 2.4GHz |
| **Build System** | PlatformIO | Latest |

## 5.2 Technical Characteristics

- **Command latency**: < 20ms (PC → Motors)
- **Motor control frequency**: 100Hz (10ms)
- **Sensor frequency**: 20Hz (50ms)
- **Command protocol**: Binary (1 byte/command)
- **Camera protocol**: UDP (fragmented JPEG)
- **Vehicle communication**: ESP-NOW (without WiFi AP)
- **Camera communication**: WiFi UDP (port 5000)
- **Architecture**: Multi-task (FreeRTOS)

---

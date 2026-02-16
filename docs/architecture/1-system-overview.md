# 1. System Overview

## 1.1 Overview

The system is composed of **4 main components** that communicate via different protocols:

- **PC (Python)**: Gesture recognition and video display
- **ESP32 Sender**: Communication bridge USB Serial → ESP-NOW
- **ESP32-S3 Camera**: WiFi video streaming module
- **ESP32 Vehicle Controller**: Main vehicle controller with FreeRTOS

## 1.2 Global Architecture Diagram

```mermaid
graph TB
    subgraph PC["🖥️ PC (Python)"]
        HandTracker["Hand Tracker<br/>(MediaPipe)"]
        CameraViewer["Camera Viewer<br/>(OpenCV)"]
    end
    
    subgraph ESP32Sender["📡 ESP32 Sender"]
        Sender["ESP32 Sender<br/>(Bridge)"]
    end
    
    subgraph ESP32Camera["📹 ESP32-S3 Camera"]
        Camera["ESP32-S3 Camera<br/>(WiFi Stream)"]
    end
    
    subgraph Vehicle["🚗 ESP32 Vehicle Controller"]
        Controller["Vehicle Controller<br/>(FreeRTOS)"]
        Motors["4x Motors<br/>(Mecanum)"]
        Sensors["Sensors<br/>(Ultrasonic + Servo)"]
    end
    
    USBProtocol["USB Serial<br/>115200 baud"]
    WiFiProtocol["WiFi UDP<br/>JPEG Fragmented"]
    ESPNOWProtocol["ESP-NOW<br/>2.4GHz Wireless"]
    
    HandTracker -->|"Gesture<br/>commands"| USBProtocol
    USBProtocol -->|"Serialized<br/>commands"| Sender
    CameraViewer <-->|"Video stream"| WiFiProtocol
    WiFiProtocol <-->|"MJPEG"| Camera
    Sender -->|"Binary<br/>commands"| ESPNOWProtocol
    ESPNOWProtocol -->|"Motor<br/>commands"| Controller
    Controller -->|"PWM control"| Motors
    Controller -->|"Read/Control"| Sensors
    
    classDef pcStyle fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
    classDef esp32Style fill:#00C853,stroke:#007E33,stroke-width:3px,color:#fff
    classDef cameraStyle fill:#FF6F00,stroke:#E65100,stroke-width:3px,color:#fff
    classDef vehicleStyle fill:#9C27B0,stroke:#6A1B9A,stroke-width:3px,color:#fff
    classDef hardwareStyle fill:#F44336,stroke:#C62828,stroke-width:2px,color:#fff
    classDef commStyle fill:#FFD700,stroke:#B8860B,stroke-width:3px,color:#000
    
    class HandTracker,CameraViewer pcStyle
    class Sender esp32Style
    class Camera cameraStyle
    class Controller vehicleStyle
    class Motors,Sensors hardwareStyle
    class USBProtocol,UDPProtocol,ESPNOWProtocol commStyle
```

## 1.3 Architectural Principles

- **Separation of concerns**: Each component has a single, well-defined role
- **Asynchronous communication**: ESP-NOW for minimal latency
- **Real-time**: FreeRTOS for motor control (< 20ms)
- **Modularity**: Task-based architecture

---

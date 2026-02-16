# 4. Communication Flow

## 4.1 Global Communication Protocol

```mermaid
sequenceDiagram
    participant User as 👤 User
    participant PC as 🖥️ PC (Hand Tracker)
    participant Sender as 📡 ESP32 Sender
    participant Vehicle as 🚗 Vehicle Controller
    participant Motors as ⚙️ Motors
    
    User->>PC: Makes gesture (hand)
    PC->>PC: MediaPipe detection
    PC->>PC: Gesture analysis
    PC->>Sender: USB Serial (Hex command)
    Sender->>Sender: Protocol conversion
    Sender->>Vehicle: ESP-NOW (Binary command)
    Vehicle->>Vehicle: Command validation
    Vehicle->>Vehicle: Enqueue to queue
    Vehicle->>Motors: Motion execution
    Motors-->>User: Vehicle moves
```

## 4.2 Binary Command Protocol

The system uses a **simple binary protocol** with single-byte commands:

```
┌─────────────────────────────────────────┐
│  Command Format (1 byte)                │
├─────────────────────────────────────────┤
│  Byte 0: Command Code (0x00 - 0xFF)   │
│                                          │
│  Motion Commands:                        │
│  0x00: STOP                             │
│  0x01: FORWARD                           │
│  0x02: BACKWARD                          │
│  0x03: SIDEWAY_LEFT                      │
│  0x04: SIDEWAY_RIGHT                     │
│  0x05: ROTATE_CW                         │
│  0x06: ROTATE_CCW                        │
│  0x07-0x0A: DIAGONAL_*                   │
│  0x0B-0x0C: PIVOT_*                      │
│                                          │
│  Mode Commands:                          │
│  0x20: MODE_MANUAL                       │
│  0x21: MODE_AUTONOMOUS                   │
│  0x22: MODE_TOGGLE                       │
│                                          │
│  System Commands:                        │
│  0xF0: HANDSHAKE_INIT                    │
│  0xF1: HANDSHAKE_ACK                     │
│  0xF2: HEARTBEAT                         │
└─────────────────────────────────────────┘
```

## 4.3 UDP Camera Protocol (ESP32-S3 → PC)

The camera system uses **UDP** for video streaming with fragmented JPEG frames:

### UDP Packet Format

```
┌─────────────────────────────────────────┐
│  Packet Header (8 bytes)                │
├─────────────────────────────────────────┤
│  frame_id:      uint32_t (4 bytes)      │
│  fragment_id:   uint16_t (2 bytes)      │
│  total_fragments: uint16_t (2 bytes)    │
├─────────────────────────────────────────┤
│  Fragment Data (max 1392 bytes)         │
│  JPEG frame data (fragmented)          │
└─────────────────────────────────────────┘
```

### Characteristics

- **UDP Port**: 5000 (stream), 5001 (discovery)
- **Max packet size**: 1400 bytes (UDP safe size)
- **Data per packet**: 1392 bytes (1400 - 8 header)
- **Fragmentation**: JPEG frames fragmented if > 1392 bytes
- **Reconstruction**: PC reconstructs frames from fragments
- **Discovery**: UDP broadcast on port 5001 for auto-discovery

### UDP Camera Flow

```mermaid
sequenceDiagram
    participant PC as PC (Camera Viewer)
    participant ESP32 as ESP32-S3 Camera
    participant Camera as Camera Hardware
    
    Note over PC,ESP32: Discovery Phase (Port 5001)
    PC->>ESP32: UDP Broadcast "DISCOVER_CAMERA_VIEWER"
    ESP32->>PC: UDP "CAMERA_IP: <IP>"
    PC->>ESP32: UDP "VIEWER_IP: <PC_IP>"
    
    Note over PC,ESP32: Streaming Phase (Port 5000)
    loop Frame Capture & Send
        Camera->>ESP32: Capture JPEG frame
        ESP32->>ESP32: Fragment frame (if > 1392 bytes)
        loop For each fragment
            ESP32->>PC: UDP Packet (Header + Fragment)
            Note right of ESP32: frame_id, fragment_id,<br/>total_fragments
        end
        PC->>PC: Reconstruct frame from fragments
        PC->>PC: Validate JPEG (0xFFD8...0xFFD9)
        PC->>PC: Decode & Display (OpenCV)
    end
```

## 4.4 Detailed Communication Diagram

```mermaid
graph TB
    subgraph PC2Sender["PC → ESP32 Sender"]
        PCApp["Hand Tracker<br/>(Python)"]
        USBPort["USB Serial Port<br/>115200 baud"]
        SenderApp["ESP32 Sender<br/>(Arduino/ESP32)"]
        
        PCApp -->|"String/Hex Command"| USBPort
        USBPort -->|"Serial Data"| SenderApp
    end
    
    subgraph Sender2Vehicle["ESP32 Sender → Vehicle"]
        SenderESP["ESP32 Sender"]
        ESPNowLink["ESP-NOW Link<br/>2.4GHz"]
        VehicleESP["Vehicle Controller<br/>(ESP32)"]
        
        SenderESP -->|"Binary Command<br/>(1 byte)"| ESPNowLink
        ESPNowLink -->|"Wireless Packet"| VehicleESP
    end
    
    subgraph Vehicle2Motors["Vehicle → Motors"]
        VehicleController["Vehicle Controller"]
        CommandQueue["Command Queue<br/>(FreeRTOS)"]
        MotorTask["Motor Control Task"]
        MotorDrivers["Motor Drivers<br/>(TB6612)"]
        MotorsHW["4x DC Motors"]
        
        VehicleController -->|"Enqueue"| CommandQueue
        CommandQueue -->|"Dequeue"| MotorTask
        MotorTask -->|"PWM Signals"| MotorDrivers
        MotorDrivers -->|"Power"| MotorsHW
    end
    
    subgraph Camera2PC["ESP32-S3 Camera → PC"]
        CameraESP["ESP32-S3 Camera"]
        UDPStream["UDP Stream<br/>Port 5000<br/>Fragmented JPEG"]
        PCViewer["Camera Viewer<br/>(Python)"]
        
        CameraESP -->|"UDP Packets<br/>(Fragmented)"| UDPStream
        UDPStream -->|"Reconstruct"| PCViewer
    end
    
    PC2Sender --> Sender2Vehicle
    Sender2Vehicle --> Vehicle2Motors
    
    classDef pc fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
    classDef sender fill:#00C853,stroke:#007E33,stroke-width:2px,color:#fff
    classDef vehicle fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    classDef hardware fill:#F44336,stroke:#C62828,stroke-width:2px,color:#fff
    classDef comm fill:#FFD700,stroke:#B8860B,stroke-width:2px,color:#000
    
    class PCApp,PCViewer pc
    class SenderApp,SenderESP sender
    class VehicleESP,VehicleController,MotorTask vehicle
    class MotorDrivers,MotorsHW,CameraESP hardware
    class USBPort,ESPNowLink,CommandQueue,UDPStream comm
```

---

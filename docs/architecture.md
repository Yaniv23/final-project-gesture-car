# Project Architecture - Gesture-Controlled Car

**Version:** 2.0  
**Date:** 2025-01-27  
**Description:** Structured architecture document with visual diagrams

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Vehicle Architecture (ESP32 Controller)](#2-vehicle-architecture-esp32-controller)
3. [PC Architecture (Hand Tracking & Camera)](#3-pc-architecture-hand-tracking--camera)
4. [Communication Flow](#4-communication-flow)

---

## 1. System Overview

### 1.1 Overview

The system is composed of **4 main components** that communicate via different protocols:

- **PC (Python)** : Gesture recognition and video display
- **ESP32 Sender** : USB Serial → ESP-NOW communication bridge
- **ESP32-S3 Camera** : WiFi video streaming module (firmware based on forked [esp32-mjpeg-multiclient-espcam-drivers](https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers) — MJPEG over HTTP, multiclient)
- **ESP32 Vehicle Controller** : Main vehicle controller with FreeRTOS

### 1.2 Global Architecture Diagram

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
    WiFiProtocol["WiFi HTTP<br/>MJPEG"]
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
    class USBProtocol,WiFiProtocol,ESPNOWProtocol commStyle
```

### 1.3 Architectural Principles

- **Separation of concerns** : Each component has a single role
- **Asynchronous communication** : ESP-NOW for minimal latency
- **Real-time** : FreeRTOS for motor control (< 20ms)
- **Modularity** : Task-based architecture

---

## 2. Vehicle Architecture (ESP32 Controller)

### 2.1 Controller Overview

The vehicle controller uses **FreeRTOS** to manage several concurrent tasks with different priorities. It supports **2 driving modes**: **MANUAL** and **AUTONOMOUS**.

### 2.2 Vehicle Architecture Diagram - Global View

```mermaid
graph TB
    subgraph VehicleController["🚗 ESP32 Vehicle Controller"]
        subgraph FreeRTOS["FreeRTOS Scheduler"]
            subgraph HighPriority["High Priority"]
                MotorControl["Motor Control<br/>Priority 4<br/>10ms"]
            end
            
            subgraph MediumPriority["Medium Priority"]
                SensorFusion["Sensors<br/>Priority 3<br/>50ms"]
                Autonomous["Autonomous<br/>Priority 3<br/>50ms"]
                Communication["Communication<br/>Priority 2<br/>100ms"]
            end
        end
        
        subgraph SharedResources["Shared Resources"]
            CommandQueue["Command Queue<br/>(FreeRTOS Queue)"]
            ModeManager["Mode Manager<br/>(Singleton)"]
            MotorDriver["Motor Driver<br/>(4 Motors)"]
        end
        
        subgraph Hardware["Hardware"]
            Motors["4x DC Motors<br/>(Mecanum Wheels)"]
            Servo["Servo Motor<br/>(Scanning)"]
            Ultrasonic["Ultrasonic Sensor<br/>(HC-SR04)"]
        end
    end
    
    ESPNOW["ESP-NOW<br/>Wireless"]
    
    ESPNOW -->|"Commands"| Communication
    Communication -->|"Send"| CommandQueue
    CommandQueue -->|"Read"| MotorControl
    CommandQueue -->|"Read"| Autonomous
    ModeManager -->|"Mode state"| MotorControl
    ModeManager -->|"Mode state"| Autonomous
    MotorControl -->|"PWM"| MotorDriver
    Autonomous -->|"Commands"| CommandQueue
    SensorFusion -->|"Read"| Ultrasonic
    SensorFusion -->|"Control"| Servo
    MotorDriver -->|"Signals"| Motors
    
    classDef highPriority fill:#F44336,stroke:#C62828,stroke-width:3px,color:#fff
    classDef mediumPriority fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef shared fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    classDef hardware fill:#607D8B,stroke:#37474F,stroke-width:2px,color:#fff
    classDef comm fill:#FFD700,stroke:#B8860B,stroke-width:2px,color:#000
    
    class MotorControl highPriority
    class SensorFusion,Autonomous,Communication mediumPriority
    class CommandQueue,ModeManager,MotorDriver shared
    class Motors,Servo,Ultrasonic hardware
    class ESPNOW comm
```

### 2.3 Task Architecture (Details)

```mermaid
graph LR
    subgraph Tasks["FreeRTOS Tasks"]
        T1["1. Motor Control<br/>10ms | Priority 4<br/>━━━━━━━━━━━━━━━━<br/>• Command dequeue<br/>• Mode check<br/>• PWM output<br/>• Mecanum kinematics"]
        
        T2["2. Sensors<br/>50ms | Priority 3<br/>━━━━━━━━━━━━━━━━<br/>• Ultrasonic reading<br/>• Servo control<br/>• Distance filtering<br/>• SensorState update"]
        
        T3["3. Autonomous<br/>50ms | Priority 3<br/>━━━━━━━━━━━━━━━━<br/>• Navigation logic<br/>• Obstacle avoidance<br/>• 3-direction scan<br/>• Stuck recovery"]
        
        T4["4. Communication<br/>100ms | Priority 2<br/>━━━━━━━━━━━━━━━━<br/>• ESP-NOW reception<br/>• Protocol parsing<br/>• Queue management<br/>• Handshake handling"]
    end
    
    classDef task1 fill:#F44336,stroke:#C62828,stroke-width:3px,color:#fff
    classDef task2 fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef task3 fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef task4 fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    
    class T1 task1
    class T2 task2
    class T3 task3
    class T4 task4
```

### 2.4 Driving Modes

The system supports **2 driving modes** managed by `ModeManager`:

#### Mode MANUAL
- Control via hand gestures (PC → ESP32 Sender → Vehicle)
- Commands received via ESP-NOW
- Real-time responsiveness (< 20ms)

#### Mode AUTONOMOUS
- Autonomous navigation with obstacle avoidance
- Uses ultrasonic sensor + servo for 3-direction scanning (10°, 90°, 180°)
- Stuck detection and recovery (up to 3 attempts)
- Distance from `SensorState` (updated by Sensors task); scan uses 20 samples per direction
- See [Section 8](#8-autonomous-mode---current-implementation-task_autonomouscpp) and [obstacle-detection-flow.md](architecture/obstacle-detection-flow.md) for the actual flow and constants.

### 2.5 Mode Diagram

```mermaid
stateDiagram-v2
    [*] --> MANUAL: Start
    
    MANUAL --> AUTONOMOUS: CMD_MODE_AUTONOMOUS<br/>or CMD_MODE_TOGGLE
    AUTONOMOUS --> MANUAL: CMD_MODE_MANUAL<br/>or CMD_MODE_TOGGLE
    
    state MANUAL {
        [*] --> WaitingCommand
        WaitingCommand --> ProcessingCommand: ESP-NOW command
        ProcessingCommand --> ExecutingMotion: Validation
        ExecutingMotion --> WaitingCommand: Execution complete
        ExecutingMotion --> EmergencyStop: Obstacle detected
        EmergencyStop --> WaitingCommand: Obstacle cleared
    }
    
    state AUTONOMOUS {
        [*] --> Forward
        Forward --> Scan: Obstacle detected
        Scan --> Decision: Scan 3 directions
        Decision --> Action: Direction choice
        Action --> Forward: Motion executed
        Action --> BackingUp: All directions blocked
        BackingUp --> Scan: Backup complete
        Forward --> StuckPivoting: Stuck detected
        StuckPivoting --> Scan: Pivot complete
    }
```

### 2.6 Data Flow - MANUAL Mode

```mermaid
sequenceDiagram
    participant PC as PC (Hand Tracker)
    participant Sender as ESP32 Sender
    participant Vehicle as Vehicle Controller
    participant Comm as Communication Task
    participant Queue as Command Queue
    participant Motor as Motor Control Task
    participant Motors as Motors
    
    PC->>Sender: USB Serial (Command)
    Sender->>Vehicle: ESP-NOW (Binary command)
    Vehicle->>Comm: ESP-NOW reception
    Comm->>Comm: Protocol validation
    Comm->>Queue: Send command
    Queue->>Motor: Read command
    Motor->>Motor: Mode check (MANUAL)
    Motor->>Motor: Execute motion
    Motor->>Motors: PWM control
    Motors-->>PC: Motion complete
```

### 2.7 Data Flow - AUTONOMOUS Mode

```mermaid
sequenceDiagram
    participant Auto as Autonomous Task
    participant Sensor as Sensors Task
    participant SensorState as SensorState
    participant Servo as Servo Motor
    participant Queue as Command Queue
    participant Motor as Motor Control Task
    participant Motors as Motors
    
    loop Navigation Loop (50ms)
        Auto->>SensorState: getSensorState (front/rear)
        SensorState-->>Auto: front_distance, rear_distance
        
        alt Obstacle detected (< 20cm)
            Auto->>Servo: Scan 3 directions (10°, 90°, 180°)
            Servo-->>Auto: Servo positions
            Auto->>Auto: 20 samples per direction
            Auto->>Auto: Pick best direction
            Auto->>Queue: Motion command
            Queue->>Motor: Read command
            Motor->>Motor: Mode check (AUTONOMOUS)
            Motor->>Motors: Execute motion
        else No obstacle
            Auto->>Queue: CMD_FORWARD
            Queue->>Motor: Read command
            Motor->>Motors: Continue forward
        end
    end
```

### 2.8 Software Layer Structure

```mermaid
graph TB
    subgraph ApplicationLayer["Application Layer (Tasks)"]
        MotorTask["task_motor_control"]
        SensorTask["task_sensors"]
        AutoTask["task_autonomous"]
        CommTask["task_communication"]
    end
    
    subgraph ControlLayer["Control Layer"]
        ModeManager["ModeManager<br/>(Singleton)"]
        MotionControl["MotionControl<br/>(Mecanum Kinematics)"]
    end
    
    subgraph DriverLayer["Driver Layer"]
        MotorDriver["MotorDriver<br/>(4 Motors)"]
        ServoDriver["ServoDriver"]
        UltrasonicDriver["UltrasonicDriver"]
    end
    
    subgraph CommunicationLayer["Communication Layer"]
        ESPNowHandler["ESPNowHandler"]
        CommandProtocol["CommandProtocol<br/>(Binary)"]
    end
    
    subgraph SharedLayer["Shared Resources"]
        Queues["FreeRTOS Queues"]
        Semaphores["FreeRTOS Semaphores"]
        Types["Shared Types"]
    end
    
    ApplicationLayer --> ControlLayer
    ApplicationLayer --> DriverLayer
    ApplicationLayer --> CommunicationLayer
    ApplicationLayer --> SharedLayer
    ControlLayer --> DriverLayer
    CommunicationLayer --> SharedLayer
    
    classDef app fill:#2196F3,stroke:#1565C0,stroke-width:2px,color:#fff
    classDef control fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    classDef driver fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    classDef comm fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef shared fill:#607D8B,stroke:#37474F,stroke-width:2px,color:#fff
    
    class MotorTask,SensorTask,AutoTask,CommTask app
    class ModeManager,MotionControl control
    class MotorDriver,ServoDriver,UltrasonicDriver driver
    class ESPNowHandler,CommandProtocol comm
    class Queues,Semaphores,Types shared
```

---

## 3. PC Architecture (Hand Tracking & Camera)

### 3.1 PC Overview

The PC side consists of **2 main Python applications** that run independently:

- **Hand Tracker** : Gesture recognition with MediaPipe (`pc_side/Hand_Tracking/Hand_Tracker.py`, `constant.py`)
- **Camera Viewer** : MJPEG stream display from ESP32-CAM (`pc_side/ESP-CAM/mjpeg_viewer.py`). The vehicle camera runs firmware based on the forked repo [esp32-mjpeg-multiclient-espcam-drivers](https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers) (HTTP MJPEG, supports multiple viewers).

### 3.2 PC Architecture Diagram

```mermaid
graph TB
    subgraph PC["🖥️ PC (Python Applications)"]
        subgraph HandTrackingApp["Hand Tracker Application"]
            HandTracker["Hand_Tracker.py<br/>━━━━━━━━━━━━━━━━<br/>• Webcam capture<br/>• MediaPipe processing<br/>• Gesture recognition<br/>• Command generation"]
            
            subgraph HandTrackingModules["Modules"]
                MediaPipeModule["MediaPipe<br/>Hand Detection"]
                GestureLogic["Gesture Logic<br/>• Direction detection<br/>• Stability filtering<br/>• Mode switching"]
                SerialComm["Serial Communication<br/>• Port management<br/>• Command sending<br/>• Error handling"]
            end
            
            Config["constant.py<br/>━━━━━━━━━━━━━━━━<br/>• COM_PORT<br/>• BAUD_RATE<br/>• Thresholds"]
        end
        
        subgraph CameraApp["Camera Viewer Application"]
            CameraViewer["mjpeg_viewer.py<br/>━━━━━━━━━━━━━━━━<br/>• HTTP client<br/>• MJPEG stream /mjpeg/1<br/>• Multipart decode<br/>• OpenCV display"]
        end
        
        subgraph Dependencies["Dependencies"]
            OpenCV["OpenCV<br/>(cv2)"]
            MediaPipeLib["MediaPipe<br/>(hands)"]
            PySerial["PySerial<br/>(serial)"]
            NumPy["NumPy<br/>(numpy)"]
        end
    end
    
    subgraph Hardware["Hardware"]
        Webcam["USB Webcam"]
        ESP32Sender["ESP32 Sender<br/>(USB Serial)"]
        ESP32Camera["ESP32-CAM<br/>(MJPEG server, WiFi)"]
    end
    
    Webcam -->|"Video frames"| HandTracker
    HandTracker --> MediaPipeModule
    MediaPipeModule --> GestureLogic
    GestureLogic --> SerialComm
    SerialComm -->|"USB Serial"| ESP32Sender
    Config --> HandTracker
    
    CameraViewer -->|"HTTP GET<br/>/mjpeg/1"| ESP32Camera
    ESP32Camera -->|"HTTP MJPEG<br/>multipart stream"| CameraViewer
    
    HandTracker -.-> OpenCV
    HandTracker -.-> MediaPipeLib
    HandTracker -.-> PySerial
    CameraViewer -.-> OpenCV
    CameraViewer -.-> NumPy
    
    classDef app fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
    classDef module fill:#00C853,stroke:#007E33,stroke-width:2px,color:#fff
    classDef config fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef dep fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    classDef hw fill:#F44336,stroke:#C62828,stroke-width:2px,color:#fff
    
    class HandTracker,CameraViewer app
    class MediaPipeModule,GestureLogic,SerialComm module
    class Config config
    class OpenCV,MediaPipeLib,PySerial,NumPy dep
    class Webcam,ESP32Sender,ESP32Camera hw
```

### 3.3 Processing Flow - Hand Tracker

```mermaid
flowchart TD
    Start([Start]) --> Init[Initialization<br/>• Webcam<br/>• MediaPipe<br/>• Serial Port]
    Init --> Capture[Capture Frame<br/>Webcam]
    Capture --> Process[MediaPipe Processing<br/>Hand Detection]
    Process --> HandDetected{Hand<br/>detected?}
    
    HandDetected -->|No| TimeoutCheck{Timeout<br/>> 1s?}
    TimeoutCheck -->|Yes| StopCmd[Send STOP]
    TimeoutCheck -->|No| Capture
    
    HandDetected -->|Yes| Analyze[Gesture Analysis<br/>• Finger position<br/>• Hand orientation<br/>• 3D position]
    Analyze --> GestureType{Gesture<br/>type?}
    
    GestureType -->|Fist| Stop[STOP command]
    GestureType -->|Index Up| Forward[FORWARD command]
    GestureType -->|Index Down| Backward[BACKWARD command]
    GestureType -->|Hand Position| Direction[Direction calculation<br/>Angle + Position]
    GestureType -->|3 Fingers| ModeToggle[Toggle Mode<br/>MANUAL/AUTONOMOUS]
    
    Direction --> SidewayLeft[SIDEWAY_LEFT]
    Direction --> SidewayRight[SIDEWAY_RIGHT]
    Direction --> Diagonal[DIAGONAL_*]
    Direction --> Rotate[ROTATE_*]
    
    Stop --> Stability[Stability check<br/>Frame counter]
    Forward --> Stability
    Backward --> Stability
    SidewayLeft --> Stability
    SidewayRight --> Stability
    Diagonal --> Stability
    Rotate --> Stability
    ModeToggle --> Stability
    
    Stability --> Stable{Stable<br/>≥ 3 frames?}
    Stable -->|No| Capture
    Stable -->|Yes| Send[Send Command<br/>USB Serial]
    Send --> Capture
    
    classDef process fill:#2196F3,stroke:#1565C0,stroke-width:2px,color:#fff
    classDef decision fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef action fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    
    class Init,Capture,Process,Analyze,Stability process
    class HandDetected,TimeoutCheck,GestureType,Stable decision
    class Stop,Forward,Backward,SidewayLeft,SidewayRight,Diagonal,Rotate,ModeToggle,Send action
```

### 3.4 Gesture Commands

| Gesture | Command | Code Hex | Description |
|---------|---------|----------|-------------|
| 👊 **Fist** | `STOP` | `0x00` | Full stop |
| 👆 **Index Up** | `FORWARD` | `0x01` | Move forward |
| 👇 **Index Down** | `BACKWARD` | `0x02` | Move backward |
| ✋ **Hand Position** | `SIDEWAY_LEFT/RIGHT` | `0x03/0x04` | Sideways movement |
| 🔄 **Circle** | `ROTATE_CW/CCW` | `0x05/0x06` | Rotation |
| 📍 **Hand Position** | `DIAGONAL_*` | `0x07-0x0A` | Diagonal movements |
| ✌️ **3 Fingers** | `MODE_TOGGLE` | `0x22` | Toggle mode |

### 3.5 PC Module Architecture

```mermaid
graph LR
    subgraph HandTrackerApp["Hand Tracker Application"]
        Main["Hand_Tracker.py<br/>(Main Loop)"]
        
        subgraph Processing["Processing Pipeline"]
            Capture["Frame Capture<br/>(OpenCV)"]
            Detection["Hand Detection<br/>(MediaPipe)"]
            Analysis["Gesture Analysis<br/>(Custom Logic)"]
            Filtering["Stability Filtering<br/>(Frame Counter)"]
        end
        
        subgraph Communication["Communication"]
            SerialMgr["Serial Manager<br/>(Connection)"]
            CommandSender["Command Sender<br/>(Protocol)"]
        end
        
        ConfigFile["constant.py<br/>(Configuration)"]
    end
    
    subgraph CameraApp["Camera Viewer Application"]
        Viewer["mjpeg_viewer.py<br/>(Main Loop)"]
        HTTPClient["HTTP Client<br/>(GET /mjpeg/1)"]
        MJPEGParser["MJPEG Stream Parser<br/>(multipart decode)"]
        Display["OpenCV Display<br/>(Window)"]
    end
    
    Main --> Capture
    Capture --> Detection
    Detection --> Analysis
    Analysis --> Filtering
    Filtering --> SerialMgr
    SerialMgr --> CommandSender
    ConfigFile --> Main
    ConfigFile --> SerialMgr
    
    Viewer --> HTTPClient
    HTTPClient --> MJPEGParser
    MJPEGParser --> Display
    
    classDef main fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
    classDef process fill:#00C853,stroke:#007E33,stroke-width:2px,color:#fff
    classDef comm fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef config fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    
    class Main,Viewer main
    class Capture,Detection,Analysis,Filtering,HTTPClient,MJPEGParser,Display process
    class SerialMgr,CommandSender comm
    class ConfigFile config
```

### 3.6 Camera Firmware Source

Vehicle camera streaming uses firmware based on the forked repository [arkhipenko/esp32-mjpeg-multiclient-espcam-drivers](https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers) (BSD-3-Clause). The ESP32-CAM runs an MJPEG multiclient server; the PC connects via HTTP and displays the stream with `pc_side/ESP-CAM/mjpeg_viewer.py`.

```mermaid
graph LR
    Viewer["PC<br/>mjpeg_viewer.py"]
    Firmware["ESP32-CAM<br/>MJPEG server<br/>forked repo"]
    Viewer -->|"HTTP GET<br/>/mjpeg/1"| Firmware
    Firmware -->|"multipart stream"| Viewer
```

---

## 4. Communication Flow

### 4.1 Global Communication Protocol

```mermaid
sequenceDiagram
    participant User as User
    participant PC as PC (Hand Tracker)
    participant Sender as ESP32 Sender
    participant Vehicle as Vehicle Controller
    participant Motors as Motors
    
    User->>PC: Makes gesture (hand)
    PC->>PC: MediaPipe detection
    PC->>PC: Gesture analysis
    PC->>Sender: USB Serial (Hex command)
    Sender->>Sender: Protocol conversion
    Sender->>Vehicle: ESP-NOW (Binary command)
    Vehicle->>Vehicle: Command validation
    Vehicle->>Vehicle: Enqueue
    Vehicle->>Motors: Execute motion
    Motors-->>User: Vehicle moves
```

### 4.2 Binary Command Protocol

The system uses a **simple binary protocol** with single-byte commands:

```
┌─────────────────────────────────────────┐
│  Command Format (1 byte)                 │
├─────────────────────────────────────────┤
│  Byte 0: Command code (0x00 - 0xFF)     │
│                                          │
│  Motion commands:                        │
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
│  Mode commands:                          │
│  0x20: MODE_MANUAL                       │
│  0x21: MODE_AUTONOMOUS                   │
│  0x22: MODE_TOGGLE                       │
│                                          │
│  System commands:                        │
│  0xF0: HANDSHAKE_INIT                    │
│  0xF1: HANDSHAKE_ACK                     │
│  0xF2: HEARTBEAT                         │
└─────────────────────────────────────────┘
```

### 4.3 HTTP MJPEG Camera Protocol (ESP32-CAM → PC)

The camera system uses **HTTP MJPEG** for video streaming. The vehicle camera runs firmware based on the forked repository [esp32-mjpeg-multiclient-espcam-drivers](https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers) (MJPEG multiclient server, up to 10 clients).

#### Protocol

- **Request** : HTTP GET to `http://<camera_ip>/mjpeg/1`
- **Response** : `multipart/x-mixed-replace` stream with boundary (e.g. `+++===123454321===+++`)
- **Firmware** : [arkhipenko/esp32-mjpeg-multiclient-espcam-drivers](https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers) (BSD-3-Clause)

#### HTTP MJPEG Flow

```mermaid
sequenceDiagram
    participant PC as PC (MJPEG Viewer)
    participant ESP32 as ESP32-CAM (MJPEG server)
    participant Camera as Camera Hardware
    
    PC->>ESP32: HTTP GET /mjpeg/1
    ESP32->>PC: 200 OK multipart/x-mixed-replace
    
    loop Stream frames
        Camera->>ESP32: Capture JPEG frame
        ESP32->>ESP32: Encode as MJPEG part
        ESP32->>PC: Multipart chunk (boundary + JPEG)
        PC->>PC: Parse multipart stream
        PC->>PC: Decode JPEG & Display (OpenCV)
    end
```

### 4.4 Detailed Communication Diagram

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
    
    subgraph Camera2PC["ESP32-CAM → PC"]
        CameraESP["ESP32-CAM<br/>(MJPEG server)"]
        HTTPStream["HTTP MJPEG<br/>/mjpeg/1"]
        PCViewer["MJPEG Viewer<br/>(mjpeg_viewer.py)"]
        
        CameraESP -->|"multipart stream"| HTTPStream
        HTTPStream -->|"Parse multipart<br/>Decode"| PCViewer
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
    class USBPort,ESPNowLink,CommandQueue,HTTPStream comm
```

---

## 5. Technology Summary

### 5.1 Technology Stack

| Component | Technology | Version |
|-----------|-------------|---------|
| **PC** | Python | 3.10+ |
| **PC - Vision** | OpenCV | 4.5+ |
| **PC - Gestures** | MediaPipe | 0.10.9 |
| **PC - Serial** | PySerial | 3.5+ |
| **PC - Camera** | HTTP (requests) | MJPEG viewer |
| **ESP32** | ESP-IDF / Arduino | Latest |
| **RTOS** | FreeRTOS | (Included) |
| **Wireless** | ESP-NOW | 2.4GHz |
| **Camera stream** | HTTP MJPEG | esp32-mjpeg-multiclient-espcam-drivers |
| **Build System** | PlatformIO | Latest |

### 5.2 Technical Characteristics

- **Command latency** : < 20ms (PC → Motors)
- **Motor control frequency** : 100 Hz (10 ms)
- **Sensor frequency** : 20 Hz (50 ms)
- **Command protocol** : Binary (1 byte per command)
- **Camera protocol** : HTTP MJPEG
- **Vehicle communication** : ESP-NOW (no WiFi AP)
- **Camera communication** : WiFi HTTP (multipart stream, `/mjpeg/1`)
- **Architecture** : Multi-task (FreeRTOS)

---

## 6. Architecture Key Points

### 6.1 Strengths

- **Clear separation of concerns**  
- **Modular and extensible architecture**  
- **Real-time guarantee (FreeRTOS)**  
- **Efficient binary protocol**  
- **Integrated safety system**  
- **Multi-mode support (MANUAL/AUTONOMOUS)**

### 6.2 Points of Attention

- **ESP-NOW has no delivery guarantee** (no ACK currently)  
- **Limited ESP32 resources** (RAM/Flash)  
- **Variable network latency** (WiFi interference possible)  
- **Configuration spread across** (constant.py, config.h); autonomous timings/thresholds are hardcoded in `task_autonomous.cpp`, not in config.h

---

---

## 7. Autonomous Mode - Conceptual Simplified Design (Not Implemented)

This section describes a **conceptual** simplified 3-state FSM design. The **current** implementation is in [Section 8](#8-autonomous-mode---current-implementation-task_autonomouscpp) and in `Vehicule/src/tasks/task_autonomous.cpp`. The codebase does **not** contain separate `obstacle_detection/` or `navigation/` modules; autonomous logic lives in `task_autonomous.cpp` with hardcoded timings and thresholds (not in `config.h`).

### 7.1 Overview (Conceptual)

The conceptual design uses a simplified state machine (FSM) with 3 states for obstacle-avoidance navigation:
- **3 states** instead of 7
- **1 recovery strategy** instead of 5
- **Stuck detection** based on timeout instead of complex methods

### 7.2 Conceptual FSM Diagram (3 States)

```mermaid
stateDiagram-v2
    [*] --> FORWARD: Start
    
    state FORWARD {
        [*] --> MeasureDistance
        MeasureDistance --> AdvanceNormal: distance >= 15cm
        MeasureDistance --> TransitionScan: distance < 15cm
        AdvanceNormal --> MeasureDistance: CMD_FORWARD
        TransitionScan --> [*]: CMD_STOP
    }
    
    FORWARD --> SCAN: distance < 15cm
    FORWARD --> SCAN: timeout 5s no change
    
    state SCAN {
        [*] --> ScanLeft: 45 deg
        ScanLeft --> ScanCentre: Wait 200ms
        ScanCentre --> ScanRight: 90 to 135 deg
        ScanRight --> ScanComplete: Validation
        ScanComplete --> [*]
    }
    
    SCAN --> ACTION: Scan complete + decision
    
    state ACTION {
        [*] --> ExecuteMotion
        ExecuteMotion --> RotateLeft: ROTATE_CCW
        ExecuteMotion --> RotateRight: ROTATE_CW
        ExecuteMotion --> AdvanceShort: FORWARD 200ms
        ExecuteMotion --> UTurn: BACKWARD + ROTATE random
        RotateLeft --> [*]
        RotateRight --> [*]
        AdvanceShort --> [*]
        UTurn --> [*]
    }
    
    ACTION --> FORWARD: Motion complete
    
    note right of FORWARD
        Two ways to enter SCAN:
        1. distance < 15cm (obstacle detected)
        2. Timeout 5s no distance change (stuck)
        
        Simple stuck detection:
        • Timeout: no change >= 3cm for 5s
        • Solution: backup 400ms then scan
    end note
    
    note right of ACTION
        Possible actions:
        • LEFT: CCW rotation 500ms
        • RIGHT: CW rotation 500ms
        • FORWARD: Advance 200ms
        • U-TURN: Backup 400ms + random rotation 500ms
    end note
```

### 7.2.1 Conceptual Flow - Overview

```mermaid
flowchart TD
    Start([Start]) --> Forward[FORWARD<br/>Measure distance]
    
    Forward -->|distance >= 15cm| ForwardOK[Advance normal<br/>CMD_FORWARD]
    Forward -->|distance < 15cm| Scan[SCAN<br/>3 directions<br/>45 deg 90 deg 135 deg]
    Forward -->|Timeout 5s<br/>no change| Backup[Backup 400ms]
    
    ForwardOK --> Forward
    Backup --> Scan
    
    Scan -->|Scan complete| Decision[Pick best direction<br/>Left > Right > Forward > U-Turn]
    
    Decision -->|Left > 30cm| ActionLeft[ACTION<br/>ROTATE_CCW 500ms]
    Decision -->|Right > 30cm| ActionRight[ACTION<br/>ROTATE_CW 500ms]
    Decision -->|Forward > 30cm| ActionForward[ACTION<br/>FORWARD 200ms]
    Decision -->|All < 30cm| ActionUTurn[ACTION<br/>BACKWARD 400ms<br/>+ ROTATE random 500ms]
    
    ActionLeft --> Forward
    ActionRight --> Forward
    ActionForward --> Forward
    ActionUTurn --> Forward
    
    style Forward fill:#4CAF50,stroke:#2E7D32,stroke-width:3px,color:#fff
    style Scan fill:#2196F3,stroke:#1565C0,stroke-width:2px,color:#fff
    style Decision fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    style ActionLeft fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    style ActionRight fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    style ActionForward fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    style ActionUTurn fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    style Backup fill:#F44336,stroke:#C62828,stroke-width:2px,color:#fff
```

### 7.2.2 Conceptual Comparison (Before / After)

| Aspect | Before | After (conceptual) |
|--------|--------|---------------------|
| **States** | 7 (FORWARD, SCAN, DECISION, ACTION, BACKING_UP, STUCK_PIVOTING, STOPPED) | 3 (FORWARD, SCAN, ACTION) |
| **Recovery strategies** | 5 (BACKUP_TURN, PIVOT_360, BACKUP_LONG, RANDOM_TURN, WALL_FOLLOW) | 1 (Backup + random rotation) |
| **Modules** | 6 (conceptual) | 3 (conceptual; not present in repo) |
| **Stuck detection** | Variance, oscillation, position tracking | Simple timeout (5s no change) |

*Note: The current codebase implements autonomous logic in a single task (`task_autonomous.cpp`) with hardcoded values; it does not use separate `obstacle_detection/` or `navigation/` modules or AUTO_* defines in config.h. See Section 8 and [obstacle-detection-flow.md](architecture/obstacle-detection-flow.md).*

### 7.3 Conceptual Processing Pipeline

```
┌──────────────┐
│ Ultrasonic   │
│ Raw Reading  │
└──────┬───────┘
       ↓
┌──────────────┐
│ SensorFilter │ ← Median filter (buffer=3)
│ (median)     │
└──────┬───────┘
       ↓
┌──────────────┐
│ Position     │
│ Tracker      │ → Motion detection
└──────┬───────┘
       ↓
┌──────────────┐
│ Stuck        │
│ Detector     │ → Variance + Oscillations
└──────┬───────┘
       ↓
┌──────────────┐
│ Navigation   │
│ FSM          │ → Decision + Action
└──────────────┘
```

*The above pipeline is conceptual. The actual implementation uses `SensorState` (updated by `task_sensors`) and inline scan/decision logic in `task_autonomous.cpp`.*

### 7.4 Conceptual Modules (Not in Repo)

The following modules are **not** present in the repository. Autonomous logic is implemented directly in `task_autonomous.cpp` (see Section 8).

#### SensorFilter (conceptual)
- **Role** : Remove ultrasonic sensor spikes
- **Algorithm** : Median filter (circular buffer n=3)

#### ObstacleScanner (conceptual)
- **Role** : Scan environment (3 directions)
- **Angles** : Conceptual 45° / 90° / 135°; actual code uses 10° / 90° / 180°

#### NavigationStateMachine (conceptual)
- **Role** : Orchestrate navigation
- **States** : 3 states (FORWARD, SCAN, ACTION)

#### DirectionDecider (conceptual)
- **Role** : Pick best direction
- **Criterion** : Max distance among free directions

### 7.5 Configuration (Actual vs Conceptual)

**Actual:** [Vehicule/src/config.h](Vehicule/src/config.h) contains task periods and hardware pins but **no** AUTO_* autonomous parameters. Obstacle threshold (20 cm), rear threshold (15 cm), stuck threshold (40 cm), backup (800 ms), turn (1200 ms), recovery (800 ms), and scan angles (10°, 90°, 180°) are **hardcoded** in [task_autonomous.cpp](Vehicule/src/tasks/task_autonomous.cpp).

### 7.6 Conceptual Performance

- **Scan time** : 600–800 ms (3 directions)
- **Decision time** : < 10 ms
- **Total reaction** : < 1000 ms (detection → action)
- **Cycle period** : 50 ms (20 Hz)

### 7.7 Conceptual Stuck Detection

Conceptual simplified stuck detection (not all implemented as described):

#### Two Ways to Enter SCAN

**1. Distance < 15 cm (Obstacle detected)**  
   - Vehicle measures front distance; if below threshold → stop and transition to SCAN.

**2. Timeout with no change (Stuck detected)**  
   - Condition: No distance change ≥ 3 cm for 5 s.  
   - Solution: Backup 400 ms then transition to SCAN.

#### Conceptual Block Recovery Flow

```mermaid
sequenceDiagram
    participant FSM as NavigationStateMachine
    participant SC as ObstacleScanner
    
    Note over FSM: FORWARD state
    FSM->>SC: getFilteredDistance()
    SC-->>FSM: distance
    
    FSM->>FSM: Check timeout<br/>(5s no change >= 3cm)
    
    alt Stuck detected
        FSM->>FSM: CMD_BACKWARD (400ms)
        FSM->>FSM: Transition to SCAN
    else Obstacle detected
        FSM->>FSM: CMD_STOP
        FSM->>FSM: Transition to SCAN
    end
    
    FSM->>SC: scanDirection(45, 90, 135)
    SC-->>FSM: Scan results
    FSM->>FSM: Decide direction -> ACTION -> FORWARD
```

#### Advantages of Simplification (conceptual)

1. **Fewer false positives** : Simple timeout instead of multiple complex methods  
2. **Easier to debug** : Clear, direct logic  
3. **Fewer resources** : No circular buffers or complex history  
4. **Fast recovery** : Backup then scan to find a new path

---

## 8. Autonomous Mode - Current Implementation (`task_autonomous.cpp`)

See also: [obstacle-detection-flow.md](architecture/obstacle-detection-flow.md) for a concise flow and constants.

### 8.1 Overview

Autonomous mode is implemented directly in the FreeRTOS task `task_autonomous` (file `Vehicule/src/tasks/task_autonomous.cpp`). The logic follows a simple loop:

- **Advance in short steps** in a straight line.
- **Read front distance** from `SensorState` (updated by the Sensors task).
- **Trigger avoidance** when an obstacle is detected below a fixed threshold (< 20 cm).
- **Scan 3 directions** (10°, 90°, 180° — right, center, left) with the servo; 20 samples per direction.
- **Pick the clearest direction** and execute the corresponding rotation (1200 ms turn, 800 ms recovery rotate).
- **Apply "stuck" recovery** (up to 3 attempts, 800 ms each) if max distance ≤ 40 cm.

This implementation is intentionally **blocking and local** to the autonomous task for simplicity. Autonomous timings and thresholds are **hardcoded** in `task_autonomous.cpp`; [config.h](Vehicule/src/config.h) does not define AUTO_* parameters.

### 8.2 Flow Diagram - `task_autonomous`

```mermaid
flowchart TD
    Start([Task start]) --> WaitSetup[Wait setupComplete]
    WaitSetup --> Loop{Mode AUTONOMOUS ?}
    
    Loop -->|No| Idle[Short sleep<br/>(20 ms)]
    Idle --> Loop
    
    Loop -->|Yes| ForwardStep[Send CMD_FORWARD<br/>+ delay 60 ms]
    ForwardStep --> Measure[Read front distance<br/>from SensorState]
    
    Measure -->|distance >= 20cm<br/>or invalid| Loop
    Measure -->|distance < 20cm| Obstacle[Obstacle detected<br/>CMD_STOP]
    
    Obstacle --> Backup[CMD_BACKWARD<br/>+ check rear via SensorState]
    Backup --> Scan[Scan 3 directions<br/>10 deg / 90 deg / 180 deg<br/>20 samples per direction]
    
    Scan --> Decide[Pick best direction<br/>max valid distance]
    
    Decide -->|max_distance > 40cm| Execute[Rotate CW/CCW<br/>1200 ms toward best direction]
    Decide -->|max_distance <= 40cm| Recovery[Recovery stuck<br/>up to 3 attempts, 800 ms each + rescan]
    
    Recovery -->|Path found| Execute
    Recovery -->|Still stuck| Stuck[CMD_STOP<br/>Stay in place]
    
    Execute --> Loop
    Stuck --> Loop
    
    style WaitSetup fill:#ECEFF1,stroke:#607D8B,stroke-width:2px,color:#000
    style Loop fill:#4CAF50,stroke:#2E7D32,stroke-width:3px,color:#fff
    style ForwardStep fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    style Measure fill:#2196F3,stroke:#1565C0,stroke-width:2px,color:#fff
    style Obstacle fill:#F44336,stroke:#C62828,stroke-width:2px,color:#fff
    style Backup fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    style Scan fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    style Decide fill:#FFC107,stroke:#FFA000,stroke-width:2px,color:#000
    style Recovery fill:#FF5722,stroke:#E64A19,stroke-width:2px,color:#fff
    style Execute fill:#8BC34A,stroke:#558B2F,stroke-width:2px,color:#fff
    style Stuck fill:#795548,stroke:#4E342E,stroke-width:2px,color:#fff
```

### 8.3 Components Used

- **Sensors**  
  - **Front distance** : Read via `getSensorState(&sensor_state)` in the main loop (`sensor_state.front_distance`). The Sensors task updates `SensorState` from the ultrasonic driver.  
  - **Rear distance** : Same `SensorState` (`sensor_state.rear_distance`); used during backup to stop if rear < 15 cm.  
  - **Scan** : Inside `scanThreeDirections(servo)`, raw readings use `readFrontSensorRaw(&raw)` (20 samples per direction).

- **Actuators**  
  - `ServoDriver servo` : Moves the ultrasonic sensor to 3 fixed angles for scan: 10° (right), 90° (center), 180° (left).  
  - `task_motor_control` (via `xCommandQueue`) : Executes `CMD_FORWARD`, `CMD_BACKWARD`, `CMD_ROTATE_CW`, `CMD_ROTATE_CCW`, `CMD_STOP`.

- **Infrastructure**  
  - `ModeManager` : Enables autonomous mode (`isAutonomousMode()`); logic is skipped when not in AUTONOMOUS.  
  - `xCommandQueue` : Shared motor command queue.  
  - `SensorState` : Shared state updated by `task_sensors`; consumed by `task_autonomous`.

### 8.4 Three-Direction Scan Algorithm

The internal function `scanThreeDirections(servo)` performs a scan in three positions:

- **Scan angles** : `{10°, 90°, 180°}` (right, center, left). Indices 0 = right, 1 = center, 2 = left.
- **Per position** :
  - Move servo to target angle (non-blocking),
  - Stabilization delay: `SENSOR_READ_INTERVAL_MS` (60 ms from [config.h](Vehicule/src/config.h)),
  - **20 samples** per direction with `SENSOR_READ_INTERVAL_MS` between reads; each sample via `readFrontSensorRaw(&raw)`.
- **Filtering** :
  - Keep only distances `0 < d < 400 cm`,
  - **Average** of valid readings per direction,
  - If no valid reading: direction stays `-1.0f` (invalid).
- At end of scan, servo is **returned to 90°** for the next cycle.

### 8.5 Best Direction Selection

The function `pickBestDirection(distances, max_distance)`:

- Iterates over the 3 directions and **ignores** invalid values (`<= 0` or `>= 400`).  
- Selects the direction with **maximum distance** and returns its index:
  - `0` : right (10°),  
  - `1` : center (90°),  
  - `2` : left (180°).
- If no valid direction is found:
  - **Defaults to center** (index 1).

Rotation is then applied as:

- `best_dir == 0` → **CW** (`CMD_ROTATE_CW`), 1200 ms.  
- `best_dir == 2` → **CCW** (`CMD_ROTATE_CCW`), 1200 ms.  
- `best_dir == 1` → no rotation; loop continues forward.

### 8.6 "Stuck" Recovery Strategy

After the scan, if max distance remains low:

- **Stuck condition** : `max_distance <= 40 cm`.  
- The algorithm enters a **recovery loop (up to 3 attempts)**:
  - Find the direction with the largest valid distance,
  - Choose motion:
    - Index 0 (right) → `CMD_ROTATE_CW`,  
    - Index 2 (left) → `CMD_ROTATE_CCW`,  
    - Index 1 (center) → `CMD_FORWARD`,  
    - Otherwise → reuse last rotation (`current_rotate_cmd`),
  - Send the command for **800 ms** (`rotate_recovery`),
  - Stop, then **full rescan**.
- If after 3 attempts max distance is **still ≤ 40 cm**:
  - System is **"stuck"**; sends `CMD_STOP` and continues the main loop.

### 8.7 Front / Rear Safety

- **Front** :
  - Obstacle threshold: **20 cm**.  
  - While `distance >= 20 cm` (or invalid), the vehicle **keeps advancing** in short steps (`CMD_FORWARD` + 60 ms delay).  
  - Below threshold: **immediate stop** (`CMD_STOP`), then backup + scan.

- **Rear** :
  - During backup, the task reads **rear distance** from `getSensorState()` every 100 ms.  
  - If `0 < rear_distance < 15 cm`, it stops backup, logs `[AUTO] Rear obstacle detected during backup` and sends `CMD_STOP`.

Backup duration is **800 ms** total (or until rear obstacle). This provides **minimal but robust** safety while keeping the autonomous code compact.

---

**Document created 2025-01-27**  
**Version 2.1 - Structured Architecture with Mermaid Diagrams + Autonomous Mode**

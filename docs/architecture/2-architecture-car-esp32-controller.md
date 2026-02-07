# 2. Architecture Car (ESP32 Controller)

## 2.1 Controller Overview

The car controller uses **FreeRTOS** to manage multiple concurrent tasks with different priorities. It supports **2 driving modes**: **MANUAL** and **AUTONOMOUS**.

## 2.2 Car Architecture Diagram - Global View

```mermaid
graph TB
    subgraph VehicleController["🚗 ESP32 Vehicle Controller"]
        subgraph FreeRTOS["FreeRTOS Scheduler"]
            subgraph HighPriority["🔴 High Priority"]
                MotorControl["Motor Control<br/>Priority 4<br/>10ms"]
            end
            
            subgraph MediumPriority["🟡 Medium Priority"]
                SensorFusion["Sensor Fusion<br/>Priority 3<br/>50ms"]
                Autonomous["Autonomous<br/>Priority 3<br/>50ms"]
                Communication["Communication<br/>Priority 2<br/>100ms"]
            end
            
            subgraph LowPriority["🟢 Low Priority"]
                Telemetry["Telemetry<br/>Priority 1<br/>100ms"]
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
    
    ESPNOW -->|"Commandes"| Communication
    Communication -->|"Send"| CommandQueue
    CommandQueue -->|"Read"| MotorControl
    CommandQueue -->|"Read"| Autonomous
    ModeManager -->|"Mode state"| MotorControl
    ModeManager -->|"Mode state"| Autonomous
    MotorControl -->|"PWM"| MotorDriver
    Autonomous -->|"Commandes"| CommandQueue
    SensorFusion -->|"Read"| Ultrasonic
    SensorFusion -->|"Control"| Servo
    MotorDriver -->|"Signals"| Motors
    Telemetry -->|"Status"| ESPNOW
    
    classDef highPriority fill:#F44336,stroke:#C62828,stroke-width:3px,color:#fff
    classDef mediumPriority fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef lowPriority fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    classDef shared fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    classDef hardware fill:#607D8B,stroke:#37474F,stroke-width:2px,color:#fff
    classDef comm fill:#FFD700,stroke:#B8860B,stroke-width:2px,color:#000
    
    class MotorControl highPriority
    class SensorFusion,Autonomous,Communication mediumPriority
    class Telemetry lowPriority
    class CommandQueue,ModeManager,MotorDriver shared
    class Motors,Servo,Ultrasonic hardware
    class ESPNOW comm
```

## 2.3 Task Architecture (Details)

```mermaid
graph LR
    subgraph Tasks["FreeRTOS Tasks"]
        T1["1. Safety Monitor<br/>⏱️ 50ms | 🔴 Priority 5<br/>━━━━━━━━━━━━━━━━<br/>• Watchdog monitoring<br/>• Emergency stop<br/>• Obstacle detection<br/>• Timeout monitoring"]
        
        T2["2. Motor Control<br/>⏱️ 10ms | 🔴 Priority 4<br/>━━━━━━━━━━━━━━━━<br/>• Command processing<br/>• Mode switching<br/>• Motion execution<br/>• PWM control"]
        
        T3["3. Sensor Fusion<br/>⏱️ 50ms | 🟡 Priority 3<br/>━━━━━━━━━━━━━━━━<br/>• Ultrasonic reading<br/>• Servo control<br/>• Distance filtering<br/>• Sensor data fusion"]
        
        T4["4. Autonomous<br/>⏱️ 50ms | 🟡 Priority 3<br/>━━━━━━━━━━━━━━━━<br/>• Navigation logic<br/>• Obstacle avoidance<br/>• Path planning<br/>• Stuck detection"]
        
        T5["5. Communication<br/>⏱️ 100ms | 🟡 Priority 2<br/>━━━━━━━━━━━━━━━━<br/>• ESP-NOW reception<br/>• Protocol parsing<br/>• Queue management<br/>• Handshake handling"]
        
        T6["6. Telemetry<br/>⏱️ 100ms | 🟢 Priority 1<br/>━━━━━━━━━━━━━━━━<br/>• Status reporting<br/>• Debug info<br/>• Performance metrics"]
    end
    
    classDef task1 fill:#F44336,stroke:#C62828,stroke-width:3px,color:#fff
    classDef task2 fill:#F44336,stroke:#C62828,stroke-width:3px,color:#fff
    classDef task3 fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef task4 fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef task5 fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef task6 fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    
    class T1 task1
    class T2 task2
    class T3 task3
    class T4 task4
    class T5 task5
    class T6 task6
```

## 2.4 Driving Modes

The system supports **2 driving modes** managed by the `ModeManager`:

### Mode MANUAL
- Control via hand gestures (PC → ESP32 Sender → Vehicle)
- Commands received via ESP-NOW
- Real-time responsiveness (< 20ms)

### Mode AUTONOMOUS
- Autonomous navigation with obstacle avoidance
- Uses ultrasonic sensor + servo for scanning
- Stuck detection
- Navigation algorithm with 3-direction scan

## 2.5 Mode Diagram

```mermaid
stateDiagram-v2
    [*] --> MANUAL: Startup
    
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
        Decision --> Action: Choose direction
        Action --> Forward: Motion executed
        Action --> BackingUp: All directions blocked
        BackingUp --> Scan: Backup complete
        Forward --> StuckPivoting: Stuck detected
        StuckPivoting --> Scan: Pivot complete
    }
```

## 2.6 Data Flow - MANUAL Mode

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
    Motor->>Motor: Motion execution
    Motor->>Motors: PWM control
    Motors-->>PC: Motion completed
```

## 2.7 Data Flow - AUTONOMOUS Mode

```mermaid
sequenceDiagram
    participant Auto as Autonomous Task
    participant Sensor as Sensor Fusion Task
    participant Ultrasonic as Ultrasonic Sensor
    participant Servo as Servo Motor
    participant Queue as Command Queue
    participant Motor as Motor Control Task
    participant Motors as Motors
    
    loop Navigation Loop (50ms)
        Auto->>Sensor: Request distance reading
        Sensor->>Ultrasonic: Read distance
        Ultrasonic-->>Sensor: Distance (cm)
        Sensor-->>Auto: Filtered distance
        
        alt Obstacle detected (< 18cm)
            Auto->>Servo: Scan 3 directions (45°, 90°, 135°)
            Servo-->>Auto: Servo positions
            Auto->>Ultrasonic: Multiple readings
            Ultrasonic-->>Auto: Distances (L, C, R)
            Auto->>Auto: Direction decision
            Auto->>Queue: Motion command
            Queue->>Motor: Read command
            Motor->>Motor: Mode check (AUTONOMOUS)
            Motor->>Motors: Motion execution
        else No obstacle
            Auto->>Queue: FORWARD command
            Queue->>Motor: Read command
            Motor->>Motors: Continuous forward
        end
    end
```

## 2.8 Software Layer Structure

```mermaid
graph TB
    subgraph ApplicationLayer["Application Layer (Tasks)"]
        MotorTask["task_motor_control"]
        SensorTask["task_sensor_fusion"]
        AutoTask["task_autonomous"]
        CommTask["task_communication"]
        TelemetryTask["task_telemetry"]
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
    
    class SafetyTask,MotorTask,SensorTask,AutoTask,CommTask,TelemetryTask app
    class ModeManager,MotionControl control
    class MotorDriver,ServoDriver,UltrasonicDriver driver
    class ESPNowHandler,CommandProtocol comm
    class Queues,Semaphores,Types shared
```

---

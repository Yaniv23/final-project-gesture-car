# 2. Architecture Véhicule (ESP32 Controller)

## 2.1 Vue d'Ensemble du Contrôleur

Le contrôleur véhicule utilise **FreeRTOS** pour gérer plusieurs tâches concurrentes avec des priorités différentes. Il supporte **2 modes de conduite** : **MANUAL** et **AUTONOMOUS**.

## 2.2 Schéma Architecture Véhicule - Vue Globale

```mermaid
graph TB
    subgraph VehicleController["🚗 ESP32 Vehicle Controller"]
        subgraph FreeRTOS["FreeRTOS Scheduler"]
            subgraph HighPriority["🔴 Priorité Haute"]
                MotorControl["Motor Control<br/>Priority 4<br/>10ms"]
            end
            
            subgraph MediumPriority["🟡 Priorité Moyenne"]
                SensorFusion["Sensor Fusion<br/>Priority 3<br/>50ms"]
                Autonomous["Autonomous<br/>Priority 3<br/>50ms"]
                Communication["Communication<br/>Priority 2<br/>100ms"]
            end
            
            subgraph LowPriority["🟢 Priorité Basse"]
                Telemetry["Telemetry<br/>Priority 1<br/>100ms"]
            end
        end
        
        subgraph SharedResources["Ressources Partagées"]
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
    Communication -->|"Envoie"| CommandQueue
    CommandQueue -->|"Lit"| MotorControl
    CommandQueue -->|"Lit"| Autonomous
    ModeManager -->|"État mode"| MotorControl
    ModeManager -->|"État mode"| Autonomous
    MotorControl -->|"PWM"| MotorDriver
    Autonomous -->|"Commandes"| CommandQueue
    SensorFusion -->|"Lecture"| Ultrasonic
    SensorFusion -->|"Contrôle"| Servo
    MotorDriver -->|"Signaux"| Motors
    Telemetry -->|"Statut"| ESPNOW
    
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

## 2.3 Architecture des Tasks (Détails)

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

## 2.4 Modes de Conduite

Le système supporte **2 modes de conduite** gérés par le `ModeManager` :

### Mode MANUAL
- Contrôle via gestes de la main (PC → ESP32 Sender → Vehicle)
- Commandes reçues via ESP-NOW
- Réactivité temps réel (< 20ms)

### Mode AUTONOMOUS
- Navigation autonome avec évitement d'obstacles
- Utilise capteur ultrasonique + servo pour scanning
- Détection de blocage (stuck detection)
- Algorithme de navigation avec scan 3 directions

## 2.5 Schéma des Modes

```mermaid
stateDiagram-v2
    [*] --> MANUAL: Démarrage
    
    MANUAL --> AUTONOMOUS: CMD_MODE_AUTONOMOUS<br/>ou CMD_MODE_TOGGLE
    AUTONOMOUS --> MANUAL: CMD_MODE_MANUAL<br/>ou CMD_MODE_TOGGLE
    
    state MANUAL {
        [*] --> WaitingCommand
        WaitingCommand --> ProcessingCommand: Commande ESP-NOW
        ProcessingCommand --> ExecutingMotion: Validation
        ExecutingMotion --> WaitingCommand: Fin exécution
        ExecutingMotion --> EmergencyStop: Obstacle détecté
        EmergencyStop --> WaitingCommand: Obstacle évité
    }
    
    state AUTONOMOUS {
        [*] --> Forward
        Forward --> Scan: Obstacle détecté
        Scan --> Decision: Scan 3 directions
        Decision --> Action: Choix direction
        Action --> Forward: Mouvement exécuté
        Action --> BackingUp: Toutes directions bloquées
        BackingUp --> Scan: Recul terminé
        Forward --> StuckPivoting: Blocage détecté
        StuckPivoting --> Scan: Pivot terminé
    }
```

## 2.6 Flux de Données - Mode MANUAL

```mermaid
sequenceDiagram
    participant PC as PC (Hand Tracker)
    participant Sender as ESP32 Sender
    participant Vehicle as Vehicle Controller
    participant Comm as Communication Task
    participant Queue as Command Queue
    participant Motor as Motor Control Task
    participant Motors as Motors
    
    PC->>Sender: USB Serial (Commande)
    Sender->>Vehicle: ESP-NOW (Commande binaire)
    Vehicle->>Comm: Réception ESP-NOW
    Comm->>Comm: Validation protocole
    Comm->>Queue: Envoie commande
    Queue->>Motor: Lecture commande
    Motor->>Motor: Vérification mode (MANUAL)
    Motor->>Motor: Exécution mouvement
    Motor->>Motors: Contrôle PWM
    Motors-->>PC: Mouvement effectué
```

## 2.7 Flux de Données - Mode AUTONOMOUS

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
        Auto->>Sensor: Demande lecture distance
        Sensor->>Ultrasonic: Lecture distance
        Ultrasonic-->>Sensor: Distance (cm)
        Sensor-->>Auto: Distance filtrée
        
        alt Obstacle détecté (< 18cm)
            Auto->>Servo: Scan 3 directions (45°, 90°, 135°)
            Servo-->>Auto: Positions servo
            Auto->>Ultrasonic: Mesures multiples
            Ultrasonic-->>Auto: Distances (L, C, R)
            Auto->>Auto: Décision direction
            Auto->>Queue: Commande mouvement
            Queue->>Motor: Lecture commande
            Motor->>Motor: Vérification mode (AUTONOMOUS)
            Motor->>Motors: Exécution mouvement
        else Pas d'obstacle
            Auto->>Queue: Commande FORWARD
            Queue->>Motor: Lecture commande
            Motor->>Motors: Avancement continu
        end
    end
```

## 2.8 Structure des Couches Logiciel

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

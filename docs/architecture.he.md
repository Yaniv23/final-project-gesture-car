# ארכיטקטורת הפרויקט - מכונית מבוקרת תנועות

**גרסה:** 2.0  
**תאריך:** 2025-01-27  
**תיאור:** מסמך ארכיטקטורה מובנה עם דיאגרמות ויזואליות

---

## תוכן עניינים

1. [תצוגה כללית של המערכת](#1-תצוגה-כללית-של-המערכת)
2. [ארכיטקטורת הרכב (ESP32 Controller)](#2-ארכיטקטורת-הרכב-esp32-controller)
3. [ארכיטקטורת PC (Hand Tracking & Camera)](#3-ארכיטקטורת-pc-hand-tracking--camera)
4. [זרימת תקשורת](#4-זרימת-תקשורת)

---

## 1. תצוגה כללית של המערכת

### 1.1 סקירה כללית

המערכת מורכבת מ-**4 רכיבים עיקריים** המתקשרים באמצעות פרוטוקולים שונים:

- **PC (Python)** : זיהוי תנועות והצגת וידאו
- **ESP32 Sender** : גשר תקשורת USB Serial → ESP-NOW
- **ESP32-S3 Camera** : מודול הזרמת וידאו WiFi
- **ESP32 Vehicle Controller** : בקר ראשי של הרכב עם FreeRTOS

### 1.2 דיאגרמת ארכיטקטורה כללית

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
    
    HandTracker -->|"Commandes<br/>gestuelles"| USBProtocol
    USBProtocol -->|"Commandes<br/>sérialisées"| Sender
    CameraViewer <-->|"Stream vidéo"| WiFiProtocol
    WiFiProtocol <-->|"MJPEG"| Camera
    Sender -->|"Commandes<br/>binaires"| ESPNOWProtocol
    ESPNOWProtocol -->|"Commandes<br/>moteur"| Controller
    Controller -->|"Contrôle PWM"| Motors
    Controller -->|"Lecture/Contrôle"| Sensors
    
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

### 1.3 עקרונות ארכיטקטוניים

- **הפרדת אחריות** : לכל רכיב תפקיד ייחודי
- **תקשורת אסינכרונית** : ESP-NOW לזמן תגובה מינימלי
- **זמן אמת** : FreeRTOS לבקרת מנוע (< 20ms)
- **מודולריות** : ארכיטקטורה מבוססת משימות (tasks)

---

## 2. ארכיטקטורת הרכב (ESP32 Controller)

### 2.1 סקירת הבקר

בקר הרכב משתמש ב-**FreeRTOS** לניהול מספר משימות מקבילות עם עדיפויות שונות. הוא תומך ב-**2 מצבי נהיגה** : **MANUAL** ו-**AUTONOMOUS**.

### 2.2 דיאגרמת ארכיטקטורת הרכב - תצוגה כללית

```mermaid
graph TB
    subgraph VehicleController["🚗 ESP32 Vehicle Controller"]
        subgraph FreeRTOS["FreeRTOS Scheduler"]
            subgraph HighPriority["🔴 Priorité Haute"]
                SafetyMonitor["Safety Monitor<br/>Priority 5<br/>50ms"]
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
    SafetyMonitor -->|"Arrêt d'urgence"| MotorDriver
    MotorDriver -->|"Signaux"| Motors
    Telemetry -->|"Statut"| ESPNOW
    
    classDef highPriority fill:#F44336,stroke:#C62828,stroke-width:3px,color:#fff
    classDef mediumPriority fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef lowPriority fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    classDef shared fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    classDef hardware fill:#607D8B,stroke:#37474F,stroke-width:2px,color:#fff
    classDef comm fill:#FFD700,stroke:#B8860B,stroke-width:2px,color:#000
    
    class SafetyMonitor,MotorControl highPriority
    class SensorFusion,Autonomous,Communication mediumPriority
    class Telemetry lowPriority
    class CommandQueue,ModeManager,MotorDriver shared
    class Motors,Servo,Ultrasonic hardware
    class ESPNOW comm
```

### 2.3 ארכיטקטורת המשימות (פרטים)

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

### 2.4 מצבי נהיגה

המערכת תומכת ב-**2 מצבי נהיגה** המנוהלים על ידי `ModeManager`:

#### מצב MANUAL
- בקרה באמצעות תנועות יד (PC → ESP32 Sender → Vehicle)
- פקודות מתקבלות דרך ESP-NOW
- תגובתיות זמן אמת (< 20ms)

#### מצב AUTONOMOUS
- ניווט אוטונומי עם הימנעות ממכשולים
- משתמש בחיישן אולטרה-סאונד + סרבו לסריקה
- זיהוי תקיעות (stuck detection)
- אלגוריתם ניווט עם סריקת 3 כיוונים

### 2.5 דיאגרמת המצבים

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

### 2.6 זרימת נתונים - מצב MANUAL

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

### 2.7 זרימת נתונים - מצב AUTONOMOUS

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

### 2.8 מבנה שכבות התוכנה

```mermaid
graph TB
    subgraph ApplicationLayer["Application Layer (Tasks)"]
        SafetyTask["task_safety_monitor"]
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

## 3. ארכיטקטורת PC (Hand Tracking & Camera)

### 3.1 סקירת PC

צד ה-PC מורכב מ-**2 אפליקציות Python עיקריות** הפועלות באופן עצמאי:

- **Hand Tracker** : זיהוי תנועות עם MediaPipe
- **Camera Viewer** : הצגת הזרמת וידאו מ-ESP32-S3

### 3.2 דיאגרמת ארכיטקטורת PC

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
            CameraViewer["camera_viewer.py<br/>━━━━━━━━━━━━━━━━<br/>• UDP socket receiver<br/>• Frame reconstruction<br/>• JPEG decoding<br/>• OpenCV display"]
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
        ESP32Camera["ESP32-S3 Camera<br/>(WiFi)"]
    end
    
    Webcam -->|"Video frames"| HandTracker
    HandTracker --> MediaPipeModule
    MediaPipeModule --> GestureLogic
    GestureLogic --> SerialComm
    SerialComm -->|"USB Serial"| ESP32Sender
    Config --> HandTracker
    
    CameraViewer -->|"UDP Discovery<br/>(Port 5001)"| ESP32Camera
    ESP32Camera -->|"UDP Stream<br/>(Port 5000)<br/>Fragmented JPEG"| CameraViewer
    
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

### 3.3 זרימת עיבוד - Hand Tracker

```mermaid
flowchart TD
    Start([Démarrage]) --> Init[Initialisation<br/>• Webcam<br/>• MediaPipe<br/>• Serial Port]
    Init --> Capture[Capture Frame<br/>Webcam]
    Capture --> Process[MediaPipe Processing<br/>Hand Detection]
    Process --> HandDetected{Main<br/>détectée?}
    
    HandDetected -->|Non| TimeoutCheck{Timeout<br/>> 1s?}
    TimeoutCheck -->|Oui| StopCmd[Envoyer STOP]
    TimeoutCheck -->|Non| Capture
    
    HandDetected -->|Oui| Analyze[Analyse Gesture<br/>• Position doigts<br/>• Orientation main<br/>• Position 3D]
    Analyze --> GestureType{Type de<br/>gesture?}
    
    GestureType -->|Fist| Stop[Commande STOP]
    GestureType -->|Index Up| Forward[Commande FORWARD]
    GestureType -->|Index Down| Backward[Commande BACKWARD]
    GestureType -->|Hand Position| Direction[Calcul Direction<br/>Angle + Position]
    GestureType -->|3 Fingers| ModeToggle[Toggle Mode<br/>MANUAL/AUTONOMOUS]
    
    Direction --> SidewayLeft[SIDEWAY_LEFT]
    Direction --> SidewayRight[SIDEWAY_RIGHT]
    Direction --> Diagonal[DIAGONAL_*]
    Direction --> Rotate[ROTATE_*]
    
    Stop --> Stability[Vérification Stabilité<br/>Compteur de frames]
    Forward --> Stability
    Backward --> Stability
    SidewayLeft --> Stability
    SidewayRight --> Stability
    Diagonal --> Stability
    Rotate --> Stability
    ModeToggle --> Stability
    
    Stability --> Stable{Stable<br/>≥ 3 frames?}
    Stable -->|Non| Capture
    Stable -->|Oui| Send[Envoyer Commande<br/>USB Serial]
    Send --> Capture
    
    classDef process fill:#2196F3,stroke:#1565C0,stroke-width:2px,color:#fff
    classDef decision fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef action fill:#4CAF50,stroke:#2E7D32,stroke-width:2px,color:#fff
    
    class Init,Capture,Process,Analyze,Stability process
    class HandDetected,TimeoutCheck,GestureType,Stable decision
    class Stop,Forward,Backward,SidewayLeft,SidewayRight,Diagonal,Rotate,ModeToggle,Send action
```

### 3.4 פקודות תנועות

| תנועה | פקודה | קוד Hex | תיאור |
|---------|----------|----------|-------------|
| 👊 **אגרוף** | `STOP` | `0x00` | עצירה מלאה |
| 👆 **אצבע למעלה** | `FORWARD` | `0x01` | התקדמות |
| 👇 **אצבע למטה** | `BACKWARD` | `0x02` | נסיעה לאחור |
| ✋ **מיקום יד** | `SIDEWAY_LEFT/RIGHT` | `0x03/0x04` | תנועה צדדית |
| 🔄 **עיגול** | `ROTATE_CW/CCW` | `0x05/0x06` | סיבוב |
| 📍 **מיקום יד** | `DIAGONAL_*` | `0x07-0x0A` | תנועות אלכסוניות |
| ✌️ **3 אצבעות** | `MODE_TOGGLE` | `0x22` | החלפת מצב |

### 3.5 ארכיטקטורת מודולי PC

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
        Viewer["camera_viewer.py<br/>(Main Loop)"]
        HTTPClient["HTTP Client<br/>(Requests)"]
        MJPEGDecoder["MJPEG Decoder<br/>(Stream)"]
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
    
    Viewer --> UDPReceiver
    UDPReceiver --> FrameReconstructor
    FrameReconstructor --> JPEGDecoder
    JPEGDecoder --> Display
    
    classDef main fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
    classDef process fill:#00C853,stroke:#007E33,stroke-width:2px,color:#fff
    classDef comm fill:#FF9800,stroke:#E65100,stroke-width:2px,color:#fff
    classDef config fill:#9C27B0,stroke:#6A1B9A,stroke-width:2px,color:#fff
    
    class Main,Viewer main
    class Capture,Detection,Analysis,Filtering,UDPReceiver,FrameReconstructor,JPEGDecoder,Display process
    class SerialMgr,CommandSender comm
    class ConfigFile config
```

---

## 4. זרימת תקשורת

### 4.1 פרוטוקול תקשורת כללי

```mermaid
sequenceDiagram
    participant User as 👤 Utilisateur
    participant PC as 🖥️ PC (Hand Tracker)
    participant Sender as 📡 ESP32 Sender
    participant Vehicle as 🚗 Vehicle Controller
    participant Motors as ⚙️ Motors
    
    User->>PC: Fait un geste (main)
    PC->>PC: MediaPipe détection
    PC->>PC: Analyse gesture
    PC->>Sender: USB Serial (Commande hex)
    Sender->>Sender: Conversion protocole
    Sender->>Vehicle: ESP-NOW (Commande binaire)
    Vehicle->>Vehicle: Validation commande
    Vehicle->>Vehicle: Envoie dans queue
    Vehicle->>Motors: Exécution mouvement
    Motors-->>User: Véhicule bouge
```

### 4.2 פרוטוקול פקודות בינארי

המערכת משתמשת ב-**פרוטוקול בינארי פשוט** עם פקודות של בית אחד:

```
┌─────────────────────────────────────────┐
│  Format de Commande (1 byte)            │
├─────────────────────────────────────────┤
│  Byte 0: Code Commande (0x00 - 0xFF)   │
│                                          │
│  Commandes Mouvement:                   │
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
│  Commandes Mode:                         │
│  0x20: MODE_MANUAL                       │
│  0x21: MODE_AUTONOMOUS                   │
│  0x22: MODE_TOGGLE                       │
│                                          │
│  Commandes Système:                      │
│  0xF0: HANDSHAKE_INIT                    │
│  0xF1: HANDSHAKE_ACK                     │
│  0xF2: HEARTBEAT                         │
└─────────────────────────────────────────┘
```

### 4.3 פרוטוקול UDP מצלמה (ESP32-S3 → PC)

מערכת המצלמה משתמשת ב-**UDP** להזרמת וידאו עם פיצול פריימים JPEG:

#### פורמט חבילת UDP

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

#### מאפיינים

- **פורט UDP** : 5000 (stream), 5001 (discovery)
- **גודל מקסימלי של חבילה** : 1400 bytes (גודל בטוח ל-UDP)
- **גודל נתונים לכל חבילה** : 1392 bytes (1400 - 8 header)
- **פיצול** : פריימים JPEG מפוצלים אם > 1392 bytes
- **הרכבה מחדש** : PC מרכיב מחדש פריימים מהחלקים
- **גילוי** : שידור UDP על פורט 5001 לגילוי אוטומטי

#### זרימת UDP מצלמה

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

### 4.4 דיאגרמת תקשורת מפורטת

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
        MotorDrivers["Motor Drivers<br/>(L298N)"]
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
    PC2Sender --> Camera2PC
    
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

## 5. סיכום טכנולוגיות

### 5.1 מחסנית טכנולוגית

| רכיב | טכנולוגיה | גרסה |
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

### 5.2 מאפיינים טכניים

- **זמן תגובת פקודה** : < 20ms (PC → Motors)
- **תדירות בקרת מנוע** : 100Hz (10ms)
- **תדירות חיישנים** : 20Hz (50ms)
- **פרוטוקול פקודות** : בינארי (1 byte/פקודה)
- **פרוטוקול מצלמה** : UDP (fragmented JPEG)
- **תקשורת רכב** : ESP-NOW (ללא WiFi AP)
- **תקשורת מצלמה** : WiFi UDP (פורט 5000)
- **ארכיטקטורה** : רב-משימתי (FreeRTOS)

---

## 6. נקודות מפתח בארכיטקטורה

### 6.1 נקודות חוזק

✅ **הפרדה ברורה של אחריות**  
✅ **ארכיטקטורה מודולרית וניתנת להרחבה**  
✅ **זמן אמת מובטח (FreeRTOS)**  
✅ **פרוטוקול בינארי יעיל**  
✅ **מערכת אבטחה משולבת**  
✅ **תמיכה במצבים מרובים (MANUAL/AUTONOMOUS)**

### 6.2 נקודות תשומת לב

⚠️ **ESP-NOW ללא ערבות מסירה** (אין ACK כרגע)  
⚠️ **משאבים מוגבלים של ESP32** (RAM/Flash)  
⚠️ **זמן תגובת רשת משתנה** (הפרעות WiFi אפשריות)  
⚠️ **תצורה מפוזרת** (constant.py, config.h)

---

**מסמך נוצר ב-2025-01-27**  
**גרסה 2.0 - ארכיטקטורה מובנית עם דיאגרמות Mermaid**

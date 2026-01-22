# 3. Architecture PC (Hand Tracking & Camera)

## 3.1 Vue d'Ensemble PC

Le côté PC est composé de **2 applications Python principales** qui fonctionnent indépendamment :

- **Hand Tracker** : Reconnaissance de gestes avec MediaPipe
- **Camera Viewer** : Visualisation du stream vidéo depuis ESP32-S3

## 3.2 Schéma Architecture PC

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

## 3.3 Flux de Traitement - Hand Tracker

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

## 3.4 Commandes Gestuelles

| Gesture | Commande | Code Hex | Description |
|---------|----------|----------|-------------|
| 👊 **Fist** | `STOP` | `0x00` | Arrêt complet |
| 👆 **Index Up** | `FORWARD` | `0x01` | Avancer |
| 👇 **Index Down** | `BACKWARD` | `0x02` | Reculer |
| ✋ **Hand Position** | `SIDEWAY_LEFT/RIGHT` | `0x03/0x04` | Déplacement latéral |
| 🔄 **Circle** | `ROTATE_CW/CCW` | `0x05/0x06` | Rotation |
| 📍 **Hand Position** | `DIAGONAL_*` | `0x07-0x0A` | Mouvements diagonaux |
| ✌️ **3 Fingers** | `MODE_TOGGLE` | `0x22` | Basculer mode |

## 3.5 Architecture des Modules PC

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

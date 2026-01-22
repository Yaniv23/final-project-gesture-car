# 1. Vue Globale du Système

## 1.1 Vue d'Ensemble

Le système est composé de **4 composants principaux** qui communiquent via différents protocoles :

- **PC (Python)** : Reconnaissance de gestes et visualisation vidéo
- **ESP32 Sender** : Pont de communication USB Serial → ESP-NOW
- **ESP32-S3 Camera** : Module de streaming vidéo WiFi
- **ESP32 Vehicle Controller** : Contrôleur principal du véhicule avec FreeRTOS

## 1.2 Schéma Architecture Globale

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

## 1.3 Principes Architecturaux

- **Séparation des responsabilités** : Chaque composant a un rôle unique
- **Communication asynchrone** : ESP-NOW pour la latence minimale
- **Temps réel** : FreeRTOS pour le contrôle moteur (< 20ms)
- **Modularité** : Architecture basée sur des tâches (tasks)

---

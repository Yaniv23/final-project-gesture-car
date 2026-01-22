# 5. Résumé des Technologies

## 5.1 Stack Technique

| Composant | Technologie | Version |
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

## 5.2 Caractéristiques Techniques

- **Latence commande** : < 20ms (PC → Motors)
- **Fréquence contrôle moteur** : 100Hz (10ms)
- **Fréquence capteurs** : 20Hz (50ms)
- **Protocole commandes** : Binaire (1 byte/commande)
- **Protocole caméra** : UDP (fragmented JPEG)
- **Communication véhicule** : ESP-NOW (sans WiFi AP)
- **Communication caméra** : WiFi UDP (port 5000)
- **Architecture** : Multi-tâches (FreeRTOS)

---

# Guide de Lancement du Stream UDP

Ce guide explique comment lancer le stream vidéo UDP entre l'ESP32-S3 et votre PC.

## 📋 Prérequis

1. **Python 3** installé avec les dépendances :
   ```bash
   pip install opencv-python numpy
   ```

2. **ESP32-S3** avec le code uploadé et connecté au WiFi

3. **Même réseau WiFi** : PC et ESP32 doivent être sur le même réseau

## 🔧 Configuration Initiale

### Étape 1: Trouver l'IP de votre PC

Sur Linux :
```bash
hostname -I
# ou
ip addr show wlo1  # pour WiFi
# ou
ip addr show eth0  # pour Ethernet
```

Sur Windows :
```cmd
ipconfig
# Cherchez "IPv4 Address" sous votre interface WiFi
```

Sur macOS :
```bash
ifconfig | grep "inet "
```

### Étape 2: Configurer l'IP dans le code ESP32

Éditez `src/main.cpp` et modifiez la ligne 17 :

```cpp
const char* TARGET_IP = "172.20.10.7";  // Remplacez par l'IP de votre PC
```

### Étape 3: Vérifier les paramètres WiFi

Dans `src/main.cpp`, vérifiez que les identifiants WiFi sont corrects :

```cpp
const char *WIFI_SSID     = "iPhone de Yaniv";
const char *WIFI_PASSWORD = "12345678";
```

## 🚀 Lancement du Stream

### Méthode 1: Script automatique (Recommandé)

```bash
cd ESP_Camera_Module
./start_stream.sh
```

### Méthode 2: Commande Python directe

```bash
cd ESP_Camera_Module/src
python3 camera_viewer.py
```

Ou avec un port personnalisé :
```bash
python3 camera_viewer.py --port 5000
```

### Méthode 3: Depuis la racine du projet

```bash
cd ESP_Camera_Module/src
python3 camera_viewer.py
```

## 📤 Upload du code sur l'ESP32

Si vous n'avez pas encore uploadé le code :

```bash
cd ESP_Camera_Module

# Uploader le code
pio run -t upload

# Voir les logs (pour vérifier l'IP et la connexion WiFi)
pio device monitor
```

## ✅ Vérification

1. **Vérifier la connexion WiFi** :
   - Le Serial Monitor devrait afficher : `WiFi connected`
   - Notez l'IP de l'ESP32 affichée

2. **Vérifier l'envoi UDP** :
   - Le Serial Monitor devrait afficher : `UDP streaming tasks started`
   - Pas d'erreurs "UDP send failed"

3. **Vérifier la réception** :
   - Le viewer Python devrait afficher : `[INFO] First frame received!`
   - Une fenêtre OpenCV devrait s'ouvrir avec le stream

## 🐛 Dépannage

### Problème: "Still waiting for ESP32 camera stream..."

**Solutions** :
1. Vérifiez que l'IP dans `main.cpp` correspond à l'IP de votre PC
2. Vérifiez que le PC et l'ESP32 sont sur le même réseau WiFi
3. Vérifiez le firewall (port 5000 UDP doit être ouvert)
4. Vérifiez les logs Serial Monitor de l'ESP32

### Problème: "UDP send failed" dans Serial Monitor

**Solutions** :
1. Vérifiez que l'IP cible est correcte
2. Vérifiez que le PC écoute bien (le viewer Python est lancé)
3. Vérifiez la connexion WiFi de l'ESP32

### Problème: Frames corrompues ou noires

**Solutions** :
1. Vérifiez que la caméra est bien connectée
2. Vérifiez les logs Serial Monitor pour les erreurs de capture
3. Vérifiez l'éclairage (la caméra a besoin de lumière)

### Problème: Stream lent ou saccadé

**Solutions** :
1. Vérifiez la qualité du signal WiFi
2. Réduisez la résolution dans `main.cpp` (FRAMESIZE_QQVGA au lieu de FRAMESIZE_QVGA)
3. Augmentez la compression JPEG (augmentez `jpeg_quality` vers 20-30)

## 📝 Notes

- **Port UDP** : 5000 (configurable dans les deux fichiers)
- **Format** : JPEG sur UDP
- **Résolution par défaut** : QVGA (320x240)
- **Qualité JPEG** : 15 (0 = meilleure qualité, 63 = pire qualité)

## 🎮 Contrôles

- **'q'** : Quitter le viewer
- **Fermer la fenêtre** : Quitte également le viewer

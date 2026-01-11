#!/bin/bash
# Script pour lancer le viewer de stream UDP de la caméra ESP32

# Obtenir le répertoire du script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo "ESP32 Camera UDP Stream Viewer"
echo "=========================================="
echo ""

# Vérifier que Python est installé
if ! command -v python3 &> /dev/null; then
    echo "[ERREUR] Python3 n'est pas installé"
    exit 1
fi

# Vérifier que OpenCV est installé
python3 -c "import cv2" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "[ERREUR] OpenCV n'est pas installé"
    echo "Installez-le avec: pip install opencv-python numpy"
    exit 1
fi

# Lancer le viewer
echo "[INFO] Démarrage du viewer UDP sur le port 5000..."
echo "[INFO] Assurez-vous que l'ESP32 est connecté et envoie des données"
echo "[INFO] Appuyez sur 'q' dans la fenêtre pour quitter"
echo ""

cd "$SCRIPT_DIR/src"
python3 camera_viewer.py "$@"

Import("env")

"""
Script automatique pour inclure les fichiers sources de Vehicule dans test_motor_espnow
Ce script scanne automatiquement les répertoires et ajoute tous les fichiers .cpp nécessaires
sans avoir besoin de les lister manuellement.

IMPORTANT: Ce script est le SEUL mécanisme pour ajouter des fichiers depuis Vehicule/src.
Les symlinks dans src/ ont été supprimés pour éviter la double compilation.
"""

import os
import glob

# Configuration: fichiers à EXCLURE (car non utilisés dans ce test)
EXCLUDED_FILES = [
    "servo_driver.cpp",        # Nécessite ESP32Servo library
    "ultrasonic_driver.cpp",  # Non utilisé dans ce test
    "task_sensor_fusion.cpp", # Utilise les capteurs ultrasoniques
    "task_telemetry.cpp",     # Optionnel pour ce test
    "main.cpp",               # On utilise main.cpp local
]

# Répertoires à inclure depuis Vehicule/src
INCLUDED_DIRS = [
    "control",
    "drivers", 
    "communication",
    "shared",
    "safety",
]

project_dir = env.get("PROJECT_DIR")
vehicule_src_dir = os.path.join(project_dir, "..", "Vehicule", "src")

# Vérifier que le répertoire Vehicule/src existe
if not os.path.exists(vehicule_src_dir):
    print(f"[ERROR] Répertoire Vehicule/src non trouvé: {vehicule_src_dir}")
    print("[ERROR] Assurez-vous que le projet Vehicule existe.")
    exit(1)

# S'assurer que les fichiers externes ont accès aux headers du framework Arduino
# Les fichiers compilés via BuildSources doivent hériter des mêmes includes
# que les fichiers dans src/. PlatformIO devrait le faire automatiquement,
# mais on peut s'assurer que les libraries Arduino sont accessibles
framework_path = env.get("FRAMEWORK_DIR")
if framework_path:
    # Ajouter le chemin vers les libraries Arduino pour les fichiers externes
    wifi_lib_path = os.path.join(framework_path, "libraries", "WiFi", "src")
    if os.path.exists(wifi_lib_path):
        env.Append(CPPPATH=[wifi_lib_path])
        print(f"[INFO] Chemin WiFi library ajouté: {wifi_lib_path}")

print("=" * 60)
print("Inclusion automatique des fichiers depuis Vehicule/src")
print("Répertoire source:", vehicule_src_dir)
print("=" * 60)

# Compteur de fichiers ajoutés et exclus
files_added = 0
files_excluded = 0

# Parcourir chaque répertoire et ajouter les fichiers .cpp
for dir_name in INCLUDED_DIRS:
    dir_path = os.path.join(vehicule_src_dir, dir_name)
    
    if not os.path.exists(dir_path):
        print(f"[WARNING] Répertoire non trouvé: {dir_path}")
        continue
    
    # Trouver tous les fichiers .cpp dans ce répertoire
    cpp_files = glob.glob(os.path.join(dir_path, "*.cpp"))
    
    if not cpp_files:
        print(f"[INFO] Aucun fichier .cpp trouvé dans {dir_name}/")
        continue
    
    # Construire le filtre src_filter pour exclure les fichiers indésirables
    # Format: +<*.cpp> -<fichier1.cpp> -<fichier2.cpp> ...
    src_filter_parts = ["+<*.cpp>"]
    for excluded_file in EXCLUDED_FILES:
        src_filter_parts.append(f"-<{excluded_file}>")
    src_filter = " ".join(src_filter_parts)
    
    # Lister les fichiers qui seront inclus/exclus pour le log
    for cpp_file in sorted(cpp_files):
        filename = os.path.basename(cpp_file)
        if filename in EXCLUDED_FILES:
            print(f"[SKIP] Exclusion: {dir_name}/{filename}")
            files_excluded += 1
        elif os.path.isfile(cpp_file):
            print(f"[ADD] {dir_name}/{filename}")
            files_added += 1
    
    # Ajouter le répertoire avec BuildSources et le filtre
    build_dir = os.path.join("$BUILD_DIR", f"vehicule_{dir_name}")
    env.BuildSources(build_dir, dir_path, src_filter)

print("=" * 60)
print(f"Résumé:")
print(f"  - Fichiers ajoutés: {files_added}")
print(f"  - Fichiers exclus: {files_excluded}")
print("=" * 60)


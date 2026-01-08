Import("env")

"""
Script automatique pour inclure les fichiers sources de Vehicule dans test_motor_espnow
Ce script scanne automatiquement les répertoires et ajoute tous les fichiers .cpp nécessaires
sans avoir besoin de les lister manuellement.
"""

import os
import glob

# Configuration: fichiers à EXCLURE (car non utilisés dans ce test)
EXCLUDED_FILES = [
    "servo_driver.cpp",
    "ultrasonic_driver.cpp",
    "task_sensor_fusion.cpp",  # Utilise les capteurs ultrasoniques
    "task_telemetry.cpp",      # Optionnel pour ce test
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

print("=" * 60)
print("Inclusion automatique des fichiers depuis Vehicule/src")
print("Répertoire source:", vehicule_src_dir)
print("=" * 60)

# Compteur de fichiers ajoutés
files_added = 0

# Parcourir chaque répertoire et ajouter les fichiers .cpp
for dir_name in INCLUDED_DIRS:
    dir_path = os.path.join(vehicule_src_dir, dir_name)
    
    if not os.path.exists(dir_path):
        print(f"[WARNING] Répertoire non trouvé: {dir_path}")
        continue
    
    # Trouver tous les fichiers .cpp dans ce répertoire
    cpp_files = glob.glob(os.path.join(dir_path, "*.cpp"))
    
    for cpp_file in cpp_files:
        filename = os.path.basename(cpp_file)
        
        # Vérifier si le fichier doit être exclu
        if filename in EXCLUDED_FILES:
            print(f"[SKIP] Exclusion: {filename}")
            continue
        
        # Ajouter le fichier au build
        build_dir = os.path.join("$BUILD_DIR", f"vehicule_{dir_name}")
        env.BuildSources(build_dir, dir_path, filename)
        print(f"[ADD] {dir_name}/{filename}")
        files_added += 1

print("=" * 60)
print(f"Total: {files_added} fichier(s) .cpp ajouté(s) au build")
print("=" * 60)


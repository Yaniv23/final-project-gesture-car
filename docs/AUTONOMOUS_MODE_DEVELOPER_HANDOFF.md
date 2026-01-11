# Handoff Développeur - Mode Autonome

**Date:** 2025-01-27  
**De:** Winston (Architect)  
**Pour:** Développeur  
**Document de référence:** `docs/AUTONOMOUS_MODE_ARCHITECTURE.md`

---

## Résumé Exécutif

Implémentation d'un **mode de conduite autonome** pour la voiture contrôlée par gestes. Version simplifiée utilisant uniquement le capteur ultrasonique avant existant (pas de capteur arrière, pas de MPU pour la v1.0).

---

## Objectifs

- ✅ **Mode Manuel:** Conservé tel quel (aucune modification)
- 🆕 **Mode Autonome:** Navigation automatique avec évitement d'obstacles
- 🔄 **Basculement:** Via commandes ESP-NOW (`CMD_MODE_MANUAL` / `CMD_MODE_AUTONOMOUS`)

---

## Fichiers à Créer/Modifier

### Nouveaux Fichiers

1. **`src/control/mode_manager.h`** - Header du gestionnaire de mode
2. **`src/control/mode_manager.cpp`** - Implémentation du gestionnaire de mode
3. **`src/tasks/task_autonomous.cpp`** - Tâche autonome (pas de .h nécessaire, déclaration dans main.cpp)

### Fichiers à Modifier

1. **`src/communication/command_protocol.h`**
   - Ajouter: `CMD_MODE_MANUAL = 0x20`
   - Ajouter: `CMD_MODE_AUTONOMOUS = 0x21`
   - Ajouter: `CMD_MODE_TOGGLE = 0x22`

2. **`src/config.h`**
   - Ajouter paramètres mode autonome (voir section Configuration)

3. **`src/tasks/task_motor_control.cpp`**
   - Gérer commandes de mode
   - Vérifier mode avant exécution commandes manuelles

4. **`src/main.cpp`**
   - Créer tâche autonome
   - Inclure mode_manager.h

---

## Implémentation - Étapes Détaillées

### Étape 1: ModeManager (Fondation)

**Fichier:** `src/control/mode_manager.h`

```cpp
#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum DrivingMode {
    MODE_MANUAL = 0,
    MODE_AUTONOMOUS = 1
};

class ModeManager {
public:
    static ModeManager& getInstance();
    
    DrivingMode getCurrentMode() const;
    bool setMode(DrivingMode mode);
    bool isManualMode() const;
    bool isAutonomousMode() const;
    
private:
    ModeManager();
    ~ModeManager() = default;
    ModeManager(const ModeManager&) = delete;
    ModeManager& operator=(const ModeManager&) = delete;
    
    DrivingMode current_mode_;
    mutable SemaphoreHandle_t mode_mutex_;
    static ModeManager* instance_;
};

#endif // MODE_MANAGER_H
```

**Fichier:** `src/control/mode_manager.cpp`

```cpp
#include "mode_manager.h"

ModeManager* ModeManager::instance_ = nullptr;

ModeManager::ModeManager() 
    : current_mode_(MODE_MANUAL) {
    mode_mutex_ = xSemaphoreCreateMutex();
    if (mode_mutex_ == NULL) {
        // Error handling
    }
}

ModeManager& ModeManager::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new ModeManager();
    }
    return *instance_;
}

DrivingMode ModeManager::getCurrentMode() const {
    if (xSemaphoreTake(mode_mutex_, portMAX_DELAY) == pdTRUE) {
        DrivingMode mode = current_mode_;
        xSemaphoreGive(mode_mutex_);
        return mode;
    }
    return MODE_MANUAL; // Default fallback
}

bool ModeManager::setMode(DrivingMode mode) {
    if (xSemaphoreTake(mode_mutex_, portMAX_DELAY) == pdTRUE) {
        current_mode_ = mode;
        xSemaphoreGive(mode_mutex_);
        return true;
    }
    return false;
}

bool ModeManager::isManualMode() const {
    return getCurrentMode() == MODE_MANUAL;
}

bool ModeManager::isAutonomousMode() const {
    return getCurrentMode() == MODE_AUTONOMOUS;
}
```

### Étape 2: Commandes de Mode

**Fichier:** `src/communication/command_protocol.h`

Ajouter après les commandes existantes (après `CMD_PIVOT_RIGHT = 0x0C`):

```cpp
    // Mode control commands (0x20-0x22)
    CMD_MODE_MANUAL = 0x20,        // Basculer en mode manuel
    CMD_MODE_AUTONOMOUS = 0x21,    // Basculer en mode autonome
    CMD_MODE_TOGGLE = 0x22,        // Basculer entre modes
```

**Important:** Mettre à jour `isValidCommand()` pour exclure ces commandes (elles sont gérées séparément).

### Étape 3: Configuration

**Fichier:** `src/config.h`

Ajouter à la fin:

```cpp
// ============================================================================
// Autonomous Mode Settings
// ============================================================================
#define AUTONOMOUS_TASK_PRIORITY     3
#define AUTONOMOUS_TASK_PERIOD_MS    50   // 20 Hz
#define AUTONOMOUS_TASK_STACK_SIZE   4096

// Navigation parameters
#define OBSTACLE_DISTANCE_THRESHOLD_CM  20  // Distance minimale avant obstacle
#define SAFE_DISTANCE_CM                30  // Distance de sécurité
#define TURN_DURATION_MS                1000 // Durée de rotation (1 seconde)
#define AUTONOMOUS_FORWARD_SPEED       200  // Vitesse avant en mode autonome
#define AUTONOMOUS_TURN_SPEED          150  // Vitesse de rotation
```

### Étape 4: Tâche Autonome

**Fichier:** `src/tasks/task_autonomous.cpp`

Voir le code complet dans `docs/AUTONOMOUS_MODE_ARCHITECTURE.md` section "6. Tâche Autonome".

**Points clés:**
- Utilise le driver `Ultrasonic` existant
- Lit uniquement le capteur avant
- Algorithme simple: avancer → obstacle → tourner → continuer
- Envoie commandes à `xCommandQueue` (même queue que mode manuel)

### Étape 5: Modification task_motor_control

**Fichier:** `src/tasks/task_motor_control.cpp`

Ajouter en début de fonction (après initialisation):

```cpp
#include "../control/mode_manager.h"

// Dans task_motor_control():
ModeManager& mode_mgr = ModeManager::getInstance();
```

Dans la boucle principale, avant le switch des commandes:

```cpp
// Gérer les commandes de mode
if (cmd_byte == CMD_MODE_MANUAL) {
    mode_mgr.setMode(MODE_MANUAL);
    Serial.println("[MOTOR] Mode: MANUAL");
    motion_stop();  // Arrêter les moteurs lors du changement de mode
    continue;
} else if (cmd_byte == CMD_MODE_AUTONOMOUS) {
    mode_mgr.setMode(MODE_AUTONOMOUS);
    Serial.println("[MOTOR] Mode: AUTONOMOUS");
    motion_stop();  // Arrêter les moteurs lors du changement de mode
    continue;
} else if (cmd_byte == CMD_MODE_TOGGLE) {
    DrivingMode new_mode = mode_mgr.isManualMode() ? MODE_AUTONOMOUS : MODE_MANUAL;
    mode_mgr.setMode(new_mode);
    Serial.print("[MOTOR] Mode: ");
    Serial.println(new_mode == MODE_MANUAL ? "MANUAL" : "AUTONOMOUS");
    motion_stop();
    continue;
}

// Les commandes de mouvement (CMD_STOP à CMD_PIVOT_RIGHT) fonctionnent
// dans les deux modes (manuel = depuis ESP-NOW, autonome = depuis algorithme)
```

### Étape 6: Intégration dans main.cpp

**Fichier:** `src/main.cpp`

1. Ajouter include:
```cpp
#include "control/mode_manager.h"
```

2. Ajouter déclaration de fonction (avec les autres):
```cpp
void task_autonomous(void *pvParameters);
```

3. Créer la tâche dans `setup()` (après les autres tâches):
```cpp
// Task: Autonomous (Priority 3)
xTaskCreate(
    task_autonomous,
    "Autonomous",
    AUTONOMOUS_TASK_STACK_SIZE,
    NULL,
    AUTONOMOUS_TASK_PRIORITY,
    NULL
);
Serial.println("[SETUP] Created task: Autonomous (Priority 3)");
```

---

## Tests à Effectuer

### Tests Unitaires

1. **ModeManager:**
   - Basculement MODE_MANUAL → MODE_AUTONOMOUS
   - Basculement MODE_AUTONOMOUS → MODE_MANUAL
   - Thread safety (appels concurrents)

2. **Commandes de mode:**
   - `CMD_MODE_MANUAL` reçue → mode basculé
   - `CMD_MODE_AUTONOMOUS` reçue → mode basculé
   - Moteurs arrêtés lors du changement

### Tests d'Intégration

1. **Basculement de mode:**
   - Manuel → Autonome (via ESP-NOW)
   - Autonome → Manuel (via ESP-NOW)
   - Pendant mouvement (sécurité)

2. **Navigation autonome:**
   - Avance en espace libre
   - Détecte obstacle et tourne
   - Continue après évitement

3. **Robustesse:**
   - Capteur défaillant (retour -1)
   - Changement de mode rapide
   - Queue pleine

---

## Points d'Attention

### ⚠️ Important

1. **Isolation du code manuel:** Ne pas modifier le comportement du mode manuel existant
2. **Thread safety:** ModeManager utilise un mutex pour accès concurrent
3. **Sécurité:** Toujours arrêter les moteurs lors d'un changement de mode
4. **Queue partagée:** Mode manuel et autonome utilisent la même `xCommandQueue`

### 🔍 Détails Techniques

1. **Priorité tâche:** 3 (même niveau que sensor_fusion)
2. **Période:** 50ms (20 Hz) - suffisant pour navigation simple
3. **Stack size:** 4096 bytes (monitorer avec FreeRTOS tools)
4. **Capteur:** Réutilise driver existant, pas de modification nécessaire

### 📝 Notes

- Le capteur ultrasonique est déjà initialisé dans `task_sensor_fusion`
- En mode autonome, `task_sensor_fusion` peut être désactivée (économie d'énergie)
- L'algorithme est simple pour v1.0, peut être amélioré plus tard

---

## Ordre d'Implémentation Recommandé

1. ✅ **ModeManager** (1-2h) - Fondation, tests unitaires
2. ✅ **Commandes de mode** (30min) - Communication
3. ✅ **Modification task_motor_control** (1h) - Intégration
4. ✅ **Tâche autonome** (3-4h) - Fonctionnalité principale
5. ✅ **Intégration main.cpp** (30min) - Finalisation
6. ✅ **Tests** (2-3h) - Validation complète

**Total estimé:** ~8-11 heures

---

## Questions / Support

Si vous avez des questions pendant l'implémentation:
- Consulter `docs/AUTONOMOUS_MODE_ARCHITECTURE.md` pour détails complets
- Vérifier `docs/VEHICLE_CONTROLLER.md` pour architecture FreeRTOS existante
- Tester chaque composant indépendamment avant intégration

---

## Validation Finale

Avant de considérer l'implémentation terminée:

- [ ] ModeManager fonctionne (tests unitaires passés)
- [ ] Basculement de mode fonctionne via ESP-NOW
- [ ] Mode autonome navigue et évite obstacles
- [ ] Mode manuel fonctionne toujours (régression)
- [ ] Pas de fuites mémoire (FreeRTOS stack monitoring)
- [ ] Code documenté et commenté

---

**Bon développement ! 🚗**

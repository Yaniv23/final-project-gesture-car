# Tâche: Amélioration de l'Algorithme d'Évitement d'Obstacles - Mode Autonome

**Version:** 1.0  
**Date:** 2025-01-27  
**Priorité:** Haute  
**Complexité:** Moyenne  
**Durée estimée:** 3-4 jours

---

## Contexte et Objectif

L'algorithme d'évitement d'obstacles actuel utilise un **scan continu** (sweep) du servo de 0° à 60° pendant le mouvement. Cette approche fonctionne mais n'est pas optimale selon les meilleures pratiques pour les robots d'évitement d'obstacles.

**Objectif:** Implémenter une stratégie d'évitement d'obstacles optimisée basée sur un **scan déclenché** (3 directions: gauche, avant, droite) uniquement lorsqu'un obstacle est détecté, avec améliorations supplémentaires (filtrage, détection de blocage améliorée).

---

## Analyse de l'État Actuel

### Architecture Actuelle

- **Fichier:** `Vehicule/src/tasks/task_autonomous.cpp`
- **Période de tâche:** 50ms (20 Hz)
- **Priorité:** 3 (même niveau que Sensor Fusion)
- **Stratégie actuelle:** Scan continu du servo de 0° à 60° pendant le mouvement
- **Machine à états:** 5 états (FORWARD, BACKING_UP, TURNING, STUCK_PIVOTING, STOPPED)

### Problèmes Identifiés

1. **Scan continu inefficace:** Le servo balaie continuellement même quand aucun obstacle n'est présent
2. **Pas de filtrage des mesures:** Les mesures ultrasoniques peuvent être bruitées
3. **Détection de blocage basique:** La détection actuelle peut être améliorée
4. **Pas de stratégie de scan optimisée:** Le scan devrait être déclenché uniquement en cas d'obstacle

---

## Stratégie Recommandée (Basée sur Recherche)

### Principe Fondamental

**Scan déclenché au lieu de scan continu:**
- Le robot avance en mesurant uniquement la distance **devant** (servo fixe au centre)
- Si un obstacle est détecté à moins d'un seuil (ex. 18-20 cm):
  - Arrêter les moteurs
  - Scanner **3 directions** avec le servo: **Gauche (45°) → Avant (90°) → Droite (135°)**
  - Choisir la direction avec le **plus d'espace libre**
  - Tourner dans cette direction puis repartir

### Avantages de cette Approche

1. **Efficacité:** Scan uniquement quand nécessaire (réduit usure servo, économise énergie)
2. **Fiabilité:** Moins d'interférences entre mesures
3. **Simplicité:** Algorithme plus facile à comprendre et débugger
4. **Performance:** Décision plus rapide (3 mesures ciblées vs scan complet)

---

## Architecture FreeRTOS - Adaptation

### Contraintes à Respecter

✅ **À MAINTENIR:**
- Période de tâche: 50ms (20 Hz)
- Priorité: 3 (ne pas modifier)
- Structure FreeRTOS existante
- Utilisation des queues FreeRTOS pour les commandes
- Compatibilité avec `ModeManager` et autres composants

✅ **À MODIFIER:**
- Stratégie de scan (continu → déclenché)
- Machine à états (ajouter état SCAN)
- Logique de décision (basée sur 3 directions au lieu de distance map)

### Nouvelle Machine à États

```
┌─────────────┐
│   FORWARD   │ ← Avance, mesure distance devant (servo fixe 90°)
└──────┬──────┘
       │ Obstacle détecté (< CRITICAL_DISTANCE)
       ▼
┌─────────────┐
│    SCAN     │ ← Arrêt, scan 3 directions (45°, 90°, 135°)
└──────┬──────┘
       │ Scan complet
       ▼
┌─────────────┐
│  DECISION   │ ← Choisir meilleure direction
└──────┬──────┘
       │ Direction choisie
       ▼
┌─────────────┐
│   ACTION    │ ← Tourner ou reculer selon décision
└──────┬──────┘
       │ Action terminée
       ▼
┌─────────────┐
│   FORWARD   │ ← Retour à l'avance
└─────────────┘

États spéciaux:
- BACKING_UP: Recul sécurisé (existant, à garder)
- STUCK_PIVOTING: Pivot sur place si bloqué (existant, améliorer)
```

---

## Spécifications Techniques

### 1. États de Navigation

```cpp
enum NavigationState {
    STATE_FORWARD,          // Avance avec mesure devant uniquement
    STATE_SCAN,             // NOUVEAU: Scan 3 directions déclenché
    STATE_DECISION,          // NOUVEAU: Décision basée sur scan
    STATE_ACTION,           // NOUVEAU: Exécution mouvement choisi
    STATE_BACKING_UP,       // Existant: Recul sécurisé
    STATE_STUCK_PIVOTING,   // Existant: Pivot si bloqué (améliorer)
    STATE_STOPPED           // Existant: Arrêt sécurité
};
```

### 2. Structure de Données pour Scan 3 Directions

```cpp
struct ScanResult {
    float distance_left;    // Distance à gauche (45°)
    float distance_front;   // Distance devant (90°)
    float distance_right;   // Distance à droite (135°)
    unsigned long timestamp_ms;
    bool is_valid;
};

static ScanResult last_scan;
```

### 3. Paramètres de Configuration (à ajouter dans `config.h`)

```cpp
// Scan déclenché - Angles de scan
#define SCAN_LEFT_ANGLE        45   // Angle gauche pour scan
#define SCAN_CENTER_ANGLE      90   // Angle centre (avant)
#define SCAN_RIGHT_ANGLE       135  // Angle droite pour scan

// Distances seuils
#define CRITICAL_DISTANCE_CM   18   // Distance critique pour déclencher scan
#define MIN_FREE_SPACE_CM      25   // Espace minimum pour choisir une direction

// Timing servo
#define SERVO_STABILIZATION_MS 200  // Délai stabilisation servo avant mesure (200-250ms recommandé)

// Filtrage mesures
#define FILTER_SAMPLES         5    // Nombre d'échantillons pour moyenne
#define FILTER_DELAY_MS        20   // Délai entre échantillons

// Détection blocage améliorée
#define STUCK_BACKUP_COUNT     3    // Nombre de tentatives avant backup long
#define STUCK_BACKUP_DURATION_MS 1000  // Durée backup long si bloqué
```

### 4. Fonctions Clés à Implémenter

#### 4.1 Scan 3 Directions avec Filtrage

```cpp
/**
 * @brief Effectue un scan des 3 directions (gauche, avant, droite)
 * @param ultrasonic_sensor Référence au capteur ultrasonique
 * @param servo Référence au servo driver
 * @param result Structure ScanResult à remplir
 * @return true si scan réussi, false sinon
 */
static bool scan3Directions(Ultrasonic& ultrasonic_sensor, 
                             ServoDriver& servo, 
                             ScanResult& result) {
    // 1. Gauche (45°)
    servo.setAngle(SCAN_LEFT_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
    result.distance_left = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
    
    // 2. Avant (90°)
    servo.setAngle(SCAN_CENTER_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
    result.distance_front = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
    
    // 3. Droite (135°)
    servo.setAngle(SCAN_RIGHT_ANGLE);
    vTaskDelay(pdMS_TO_TICKS(SERVO_STABILIZATION_MS));
    result.distance_right = filteredDistance(ultrasonic_sensor, FILTER_SAMPLES);
    
    // Recentrer servo
    servo.setAngle(SCAN_CENTER_ANGLE);
    
    result.timestamp_ms = millis();
    result.is_valid = (result.distance_left > 0 && 
                       result.distance_front > 0 && 
                       result.distance_right > 0);
    
    return result.is_valid;
}
```

#### 4.2 Filtrage des Mesures (Moyenne)

```cpp
/**
 * @brief Mesure distance filtrée (moyenne de plusieurs échantillons)
 * @param ultrasonic_sensor Référence au capteur
 * @param samples Nombre d'échantillons pour la moyenne
 * @return Distance filtrée en cm, ou -1.0 si invalide
 */
static float filteredDistance(Ultrasonic& ultrasonic_sensor, int samples) {
    float sum = 0.0f;
    int valid_count = 0;
    
    for (int i = 0; i < samples; i++) {
        float dist = ultrasonic_sensor.readDistanceCM();
        // Filtrer valeurs invalides (négatives ou > 400cm)
        if (dist >= 0.0f && dist <= 400.0f) {
            sum += dist;
            valid_count++;
        }
        if (i < samples - 1) {
            vTaskDelay(pdMS_TO_TICKS(FILTER_DELAY_MS));
        }
    }
    
    if (valid_count == 0) {
        return -1.0f;
    }
    
    return sum / valid_count;
}
```

#### 4.3 Décision de Direction

```cpp
/**
 * @brief Choisit la meilleure direction basée sur scan
 * @param scan Résultat du scan 3 directions
 * @return Direction choisie: 0=GAUCHE, 1=DROITE, 2=AVANT, 3=DEMI_TOUR
 */
static int decideDirection(const ScanResult& scan) {
    // Priorité: Gauche > Droite > Avant > Demi-tour
    
    if (scan.distance_left > MIN_FREE_SPACE_CM && 
        scan.distance_left > scan.distance_right) {
        return 0; // GAUCHE
    }
    
    if (scan.distance_right > MIN_FREE_SPACE_CM && 
        scan.distance_right >= scan.distance_left) {
        return 1; // DROITE
    }
    
    if (scan.distance_front > MIN_FREE_SPACE_CM) {
        return 2; // AVANT
    }
    
    // Tout bloqué - demi-tour
    return 3; // DEMI_TOUR
}
```

#### 4.4 Exécution du Mouvement

```cpp
/**
 * @brief Exécute le mouvement choisi
 * @param direction Direction choisie (0-3)
 * @param turn_duration_ms Durée de rotation en ms
 */
static void executeMovement(int direction, uint32_t turn_duration_ms) {
    uint8_t command;
    
    switch (direction) {
        case 0: // GAUCHE
            command = CMD_ROTATE_CCW;
            break;
        case 1: // DROITE
            command = CMD_ROTATE_CW;
            break;
        case 2: // AVANT
            command = CMD_FORWARD;
            break;
        case 3: // DEMI_TOUR
            // Reculer puis tourner
            command = CMD_BACKWARD;
            xQueueSend(xCommandQueue, &command, 0);
            vTaskDelay(pdMS_TO_TICKS(turn_duration_ms / 2));
            command = CMD_ROTATE_CW; // Tourner à droite
            break;
    }
    
    xQueueSend(xCommandQueue, &command, 0);
}
```

### 5. Détection de Blocage Améliorée

```cpp
/**
 * @brief Détecte si le robot est coincé (amélioration)
 * @param consecutive_stuck_count Compteur de tentatives échouées
 * @return true si bloqué, false sinon
 */
static bool checkIfStuckImproved(int& consecutive_stuck_count) {
    // Si toutes les directions sont bloquées
    if (last_scan.is_valid) {
        if (last_scan.distance_left < MIN_FREE_SPACE_CM &&
            last_scan.distance_front < MIN_FREE_SPACE_CM &&
            last_scan.distance_right < MIN_FREE_SPACE_CM) {
            consecutive_stuck_count++;
            
            if (consecutive_stuck_count >= STUCK_BACKUP_COUNT) {
                return true;
            }
        } else {
            consecutive_stuck_count = 0; // Reset si chemin trouvé
        }
    }
    
    return false;
}
```

---

## Plan d'Implémentation

### Phase 1: Préparation (Jour 1)

1. **Ajouter nouvelles constantes dans `config.h`**
   - Angles de scan (45°, 90°, 135°)
   - Distances seuils (CRITICAL_DISTANCE_CM, MIN_FREE_SPACE_CM)
   - Paramètres de filtrage
   - Paramètres de détection de blocage améliorée

2. **Créer structure `ScanResult`**
   - Ajouter dans `task_autonomous.cpp`
   - Variable statique `last_scan`

### Phase 2: Implémentation Core (Jour 2)

3. **Implémenter fonction `filteredDistance()`**
   - Moyenne de N échantillons
   - Filtrage valeurs invalides
   - Tests unitaires si possible

4. **Implémenter fonction `scan3Directions()`**
   - Positionnement servo aux 3 angles
   - Mesures filtrées à chaque angle
   - Recentrage servo après scan

5. **Implémenter fonction `decideDirection()`**
   - Logique de priorité (Gauche > Droite > Avant > Demi-tour)
   - Vérification MIN_FREE_SPACE_CM

### Phase 3: Machine à États (Jour 3)

6. **Modifier machine à états**
   - Ajouter états: STATE_SCAN, STATE_DECISION, STATE_ACTION
   - Modifier STATE_FORWARD pour mesure devant uniquement (servo fixe 90°)
   - Implémenter transitions entre états

7. **Implémenter fonction `executeMovement()`**
   - Gestion des 4 directions
   - Gestion demi-tour (recul + rotation)

8. **Intégrer détection de blocage améliorée**
   - Utiliser `checkIfStuckImproved()`
   - Backup long si bloqué plusieurs fois

### Phase 4: Tests et Optimisation (Jour 4)

9. **Tests fonctionnels**
   - Test scan 3 directions
   - Test décision direction
   - Test détection blocage
   - Test filtrage mesures

10. **Optimisation et ajustement**
    - Ajuster paramètres (distances, délais)
    - Vérifier performance (latence décision)
    - Vérifier consommation mémoire

---

## Points d'Attention

### ⚠️ Compatibilité FreeRTOS

- **Ne pas bloquer la tâche:** Utiliser `vTaskDelay()` au lieu de `delay()`
- **Gestion des queues:** Vérifier `pdTRUE` après `xQueueSend()`
- **Timing:** Le scan 3 directions prend ~600-750ms (3 × 200ms stabilisation + mesures)
  - Pendant ce temps, la tâche doit continuer à s'exécuter (pas de blocage)

### ⚠️ Gestion du Servo

- **Servo fixe en FORWARD:** Servo à 90° (centre) pendant l'avance
- **Stabilisation:** Toujours attendre SERVO_STABILIZATION_MS avant mesure
- **Recentrage:** Remettre servo à 90° après scan

### ⚠️ Filtrage des Mesures

- **Valeurs invalides:** Filtrer distances < 0 ou > 400cm
- **Timeout:** Gérer timeout du capteur ultrasonique
- **Performance:** FILTER_SAMPLES=5 ajoute ~100ms par mesure (5 × 20ms)

### ⚠️ Détection de Blocage

- **Compteur:** Réinitialiser compteur si chemin trouvé
- **Backup long:** Si bloqué 3 fois, faire backup long
- **Pivot:** Pivot sur place si tout bloqué (existant, améliorer)

---

## Tests de Validation

### Test 1: Scan 3 Directions
- **Objectif:** Vérifier que le scan mesure correctement les 3 directions
- **Méthode:** Placer obstacles à différentes positions, vérifier mesures
- **Critère:** Mesures cohérentes avec positions obstacles

### Test 2: Décision de Direction
- **Objectif:** Vérifier que la meilleure direction est choisie
- **Méthode:** Scénarios avec obstacles à gauche/droite/devant
- **Critère:** Direction choisie correspond à plus grand espace libre

### Test 3: Filtrage
- **Objectif:** Vérifier réduction du bruit
- **Méthode:** Comparer mesures filtrées vs non-filtrées
- **Critère:** Variance réduite avec filtrage

### Test 4: Détection de Blocage
- **Objectif:** Vérifier détection et récupération
- **Méthode:** Coincer robot dans coin, vérifier backup long
- **Critère:** Robot détecte blocage et exécute backup long après 3 tentatives

### Test 5: Performance
- **Objectif:** Vérifier latence de décision
- **Méthode:** Mesurer temps entre détection obstacle et décision
- **Critère:** < 1 seconde (scan + décision)

---

## Checklist d'Implémentation

### Code
- [ ] Ajouter constantes dans `config.h`
- [ ] Créer structure `ScanResult`
- [ ] Implémenter `filteredDistance()`
- [ ] Implémenter `scan3Directions()`
- [ ] Implémenter `decideDirection()`
- [ ] Implémenter `executeMovement()`
- [ ] Implémenter `checkIfStuckImproved()`
- [ ] Modifier machine à états (ajouter SCAN, DECISION, ACTION)
- [ ] Modifier STATE_FORWARD (servo fixe 90°, mesure devant uniquement)
- [ ] Supprimer code scan continu (sweep) si non utilisé

### Tests
- [ ] Test scan 3 directions
- [ ] Test décision direction
- [ ] Test filtrage
- [ ] Test détection blocage
- [ ] Test performance (latence)

### Documentation
- [ ] Commenter fonctions clés (Doxygen)
- [ ] Documenter paramètres configurables
- [ ] Mettre à jour README si nécessaire

---

## Références

- **Architecture actuelle:** `docs/architecture.md`
- **Code actuel:** `Vehicule/src/tasks/task_autonomous.cpp`
- **Configuration:** `Vehicule/src/config.h`
- **Stratégie recommandée:** `docs/Prepare moi un mardown avec la meilleur stategie d.md`
- **Documentation FreeRTOS:** `docs/VEHICLE_CONTROLLER.md`

---

## Notes pour l'Agent Dev

### Fichiers à Modifier

1. **`Vehicule/src/config.h`**
   - Ajouter nouvelles constantes de configuration

2. **`Vehicule/src/tasks/task_autonomous.cpp`**
   - Modifier machine à états
   - Implémenter nouvelles fonctions
   - Adapter logique de navigation

### Fichiers à Consulter (Référence)

- `Vehicule/src/drivers/servo_driver.h` - Interface servo
- `Vehicule/src/drivers/ultrasonic_driver.h` - Interface capteur
- `Vehicule/src/shared/queues.h` - Queues FreeRTOS
- `Vehicule/src/control/mode_manager.h` - Gestionnaire de mode

### Points Critiques

1. **Ne pas casser fonctionnalité existante:** Tester que le mode autonome fonctionne toujours
2. **Respecter timing FreeRTOS:** Utiliser `vTaskDelay()` et `vTaskDelayUntil()`
3. **Gestion mémoire:** Vérifier stack size suffisant (actuellement 4096 bytes)
4. **Compatibilité:** Maintenir compatibilité avec autres tâches FreeRTOS

---

## Métriques de Succès

- ✅ **Latence décision:** < 1 seconde (détection → action)
- ✅ **Fiabilité:** > 95% de décisions correctes
- ✅ **Performance:** Pas de dégradation vs version actuelle
- ✅ **Robustesse:** Gestion correcte des cas limites (blocage, mesures invalides)

---

**Document créé pour amélioration de l'algorithme d'évitement d'obstacles - Mode Autonome**

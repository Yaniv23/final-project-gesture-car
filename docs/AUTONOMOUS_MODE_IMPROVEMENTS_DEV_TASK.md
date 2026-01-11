# Tâche Dev: Améliorations Mode Autonome - Conduite Fluide et Récupération

**Status:** Ready for Development  
**Priorité:** Haute  
**Complexité:** Moyenne  
**Durée estimée:** 5 jours  
**Fichier source:** `Vehicule/src/tasks/task_autonomous.cpp`  
**Documentation référence:** `docs/AUTONOMOUS_MODE_ADVANCED_IMPROVEMENTS.md`

---

## Story

Améliorer l'algorithme d'évitement d'obstacles du mode autonome pour obtenir une conduite plus fluide, une meilleure détection d'obstacles, et une récupération efficace des situations bloquées. 

**État actuel:** L'implémentation utilise un scan déclenché à 3 directions (45°, 90°, 135°) avec filtrage des mesures. La machine à états fonctionne mais présente des limitations:
- **Tournant fixe:** Durée de rotation fixe (`TURN_DURATION_MS`) - peut être insuffisant pour obstacles larges ou excessif pour petits obstacles
- **Détection de blocage limitée:** Basée uniquement sur distances mesurées, pas sur mouvement réel du robot
- **Récupération basique:** Stratégies limitées (backup + pivot) - peut créer des boucles infinies

**Objectif:** Implémenter 3 améliorations prioritaires pour navigation plus intelligente et robuste.

---

## Acceptance Criteria

1. ✅ **Tournant adaptatif:** Le robot tourne jusqu'à ce que le chemin soit clair (au lieu d'une durée fixe)
2. ✅ **Détection de blocage améliorée:** Détection basée sur mouvement réel via historique de positions
3. ✅ **Stratégies de récupération multiples:** 5 stratégies différentes avec variation pour éviter boucles
4. ✅ **Tests:** Tous les tests passent, pas de régression sur fonctionnalité existante
5. ✅ **Performance:** Latence décision < 800ms (amélioration depuis < 1000ms actuel)
6. ✅ **Fiabilité:** > 90% de récupération réussie des situations bloquées

---

## Tasks

### Task 1: Configuration et Structures de Données
**Durée estimée:** 2 heures  
**Priorité:** CRITICAL (base pour toutes les autres tâches)

#### Subtasks

- [ ] **1.1** Ajouter nouvelles constantes dans `Vehicule/src/config.h`:
  
  Ajouter dans la section "Autonomous Mode Settings" (après ligne ~145):
  
  ```cpp
  // Tournant adaptatif
  #define ADAPTIVE_TURN_MAX_DURATION_MS  2000  // Durée max rotation (safety)
  #define ADAPTIVE_TURN_CHECK_INTERVAL_MS 50   // Intervalle vérification pendant rotation
  
  // Détection de blocage améliorée
  #define POSITION_VARIANCE_THRESHOLD     5.0f  // Variance min pour considérer mouvement (cm²)
  #define STUCK_MOVEMENT_CHECK_MS        2000   // Vérifier mouvement après X ms
  
  // Stratégies de récupération
  #define RECOVERY_MAX_ATTEMPTS           5     // Nombre max de tentatives avant état spécial
  ```

- [ ] **1.2** Ajouter structures de données dans `task_autonomous.cpp`:
  
  Ajouter après la structure `StuckDetection` (après ligne ~49):
  
  ```cpp
  // Historique de positions pour détection mouvement réel
  struct PositionHistory {
      float distances[5];           // Historique des 5 dernières distances
      unsigned long timestamps[5];  // Timestamps correspondants
      int index;                    // Index circulaire (0-4)
      bool is_moving;               // Flag: robot bouge-t-il réellement?
  };
  
  static PositionHistory position_history;
  
  // Mémoire de récupération
  struct RecoveryMemory {
      int attempt_count;                    // Nombre de tentatives de récupération
      RecoveryStrategy last_strategy;       // Dernière stratégie utilisée
      unsigned long last_recovery_time;     // Timestamp dernière récupération
  };
  
  static RecoveryMemory recovery_memory;
  ```

- [ ] **1.3** Ajouter enum pour stratégies de récupération:
  
  Ajouter après l'enum `NavigationState` (après ligne ~60):
  
  ```cpp
  // Stratégies de récupération
  enum RecoveryStrategy {
      RECOVERY_BACKUP_TURN,      // Reculer puis tourner aléatoirement
      RECOVERY_PIVOT_360,        // Rotation complète 360°
      RECOVERY_BACKUP_LONG,      // Recul long
      RECOVERY_RANDOM_TURN,      // Tourner aléatoirement avec angle variable
      RECOVERY_WALL_FOLLOW       // Essayer suivi de mur (simplifié: scan seulement)
  };
  ```

- [ ] **1.4** Initialiser structures dans `task_autonomous()`:
  
  Dans la fonction `task_autonomous()`, après `resetStuckMemory()` (ligne ~330), ajouter:
  
  ```cpp
  // Initialiser historique de positions
  for (int i = 0; i < 5; i++) {
      position_history.distances[i] = 0.0f;
      position_history.timestamps[i] = 0;
  }
  position_history.index = 0;
  position_history.is_moving = false;
  
  // Initialiser mémoire de récupération
  recovery_memory.attempt_count = 0;
  recovery_memory.last_strategy = RECOVERY_BACKUP_TURN;
  recovery_memory.last_recovery_time = 0;
  ```

**Validation:**
- [ ] Code compile sans erreurs
- [ ] Constantes accessibles dans `task_autonomous.cpp`
- [ ] Structures initialisées correctement

---

### Task 2: Tournant Adaptatif
**Durée estimée:** 4 heures  
**Priorité:** HIGH (amélioration immédiate visible)

#### Subtasks

- [ ] **2.1** Implémenter fonction `adaptiveTurn()`:
  
  Ajouter avant la fonction `task_autonomous()` (après ligne ~302):
  
  ```cpp
  /**
   * @brief Tourne jusqu'à ce que le chemin soit clair (tournant adaptatif)
   * @param turn_direction Direction de rotation (CMD_ROTATE_CW ou CMD_ROTATE_CCW)
   * @param max_turn_duration_ms Durée maximum de rotation (safety timeout)
   * @return true si chemin clair trouvé, false si timeout
   * @details Mesure distance devant pendant la rotation et arrête dès que chemin clair
   */
  static bool adaptiveTurn(uint8_t turn_direction, uint32_t max_turn_duration_ms) {
      unsigned long turn_start_ms = millis();
      uint8_t command = turn_direction;
      
      // Envoyer commande de rotation
      if (xQueueSend(xCommandQueue, &command, 0) != pdTRUE) {
          Serial.println("[AUTO] Error: Failed to send turn command");
          return false;
      }
      
      // Vérifier périodiquement si chemin clair
      while (millis() - turn_start_ms < max_turn_duration_ms) {
          vTaskDelay(pdMS_TO_TICKS(ADAPTIVE_TURN_CHECK_INTERVAL_MS));
          
          // Mesurer distance devant pendant rotation (servo au centre)
          servo.setAngle(SCAN_CENTER_ANGLE);
          vTaskDelay(pdMS_TO_TICKS(50)); // Stabilisation rapide (réduit de 200ms pour rapidité)
          
          // Mesure rapide (3 échantillons au lieu de 5 pour réduire latence)
          float distance = filteredDistance(ultrasonic_sensor, 3);
          
          // Si chemin clair trouvé, arrêter rotation immédiatement
          if (distance >= MIN_FREE_SPACE_CM && distance > 0) {
              uint8_t stop_cmd = CMD_STOP;
              xQueueSend(xCommandQueue, &stop_cmd, 0);
              Serial.print("[AUTO] Adaptive turn: path clear at ");
              Serial.print(distance);
              Serial.println(" cm");
              return true;
          }
      }
      
      // Timeout - arrêter rotation (safety)
      uint8_t stop_cmd = CMD_STOP;
      xQueueSend(xCommandQueue, &stop_cmd, 0);
      Serial.println("[AUTO] Adaptive turn: timeout reached");
      return false;
  }
  ```

- [ ] **2.2** Modifier état `STATE_ACTION` pour utiliser tournant adaptatif:
  
  Dans le case `STATE_ACTION` (ligne ~528), modifier la logique pour directions GAUCHE/DROITE:
  
  **AVANT (lignes ~576-587):**
  ```cpp
  } else if (action_direction == 0 || action_direction == 1) {
      // Turning left or right
      if (now - action_start_ms < TURN_DURATION_MS) {
          command = executeMovement(action_direction, TURN_DURATION_MS);
      } else {
          // Turn complete
          command = CMD_STOP;
          action_initiated = false;
          action_direction = -1;
          nav_state = STATE_FORWARD;
          Serial.println("[AUTO] Turn complete");
      }
  }
  ```
  
  **APRÈS:**
  ```cpp
  } else if (action_direction == 0 || action_direction == 1) {
      // Turning left or right - utiliser tournant adaptatif
      static bool adaptive_turn_initiated = false;
      
      if (!adaptive_turn_initiated) {
          // Démarrer tournant adaptatif
          uint8_t turn_cmd = (action_direction == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
          bool path_clear = adaptiveTurn(turn_cmd, ADAPTIVE_TURN_MAX_DURATION_MS);
          
          if (path_clear) {
              // Chemin clair trouvé - transition vers FORWARD
              command = CMD_STOP;
              adaptive_turn_initiated = false;
              action_initiated = false;
              action_direction = -1;
              nav_state = STATE_FORWARD;
              Serial.println("[AUTO] Adaptive turn complete - path clear");
          } else {
              // Timeout - continuer en FORWARD quand même
              command = CMD_STOP;
              adaptive_turn_initiated = false;
              action_initiated = false;
              action_direction = -1;
              nav_state = STATE_FORWARD;
              Serial.println("[AUTO] Adaptive turn timeout - continuing forward");
          }
      }
  }
  ```

- [ ] **2.3** Tests fonctionnels:
  - Test avec obstacle large (vérifier rotation continue jusqu'à chemin clair)
  - Test avec obstacle petit (vérifier rotation s'arrête rapidement)
  - Test timeout (vérifier arrêt après max_duration si pas de chemin clair)

**Validation:**
- [ ] Tournant adaptatif fonctionne (arrête quand chemin clair trouvé)
- [ ] Pas de régression sur autres fonctionnalités (AVANT, DEMI_TOUR)
- [ ] Tests passent
- [ ] Code compile sans erreurs

---

### Task 3: Détection de Blocage Améliorée
**Durée estimée:** 4 heures  
**Priorité:** HIGH (améliore robustesse)

#### Subtasks

- [ ] **3.1** Implémenter fonction `updatePositionHistory()`:
  
  Ajouter avant `task_autonomous()` (après `adaptiveTurn()`):
  
  ```cpp
  /**
   * @brief Met à jour l'historique de positions pour détecter mouvement réel
   * @param current_distance Distance actuelle mesurée
   * @details Calcule variance des 5 dernières distances pour déterminer si robot bouge
   */
  static void updatePositionHistory(float current_distance) {
      unsigned long now = millis();
      
      // Ajouter nouvelle mesure (FIFO circulaire)
      position_history.index = (position_history.index + 1) % 5);
      position_history.distances[position_history.index] = current_distance;
      position_history.timestamps[position_history.index] = now;
      
      // Calculer moyenne des distances
      float mean = 0.0f;
      int valid_count = 0;
      for (int i = 0; i < 5; i++) {
          if (position_history.distances[i] > 0) {
              mean += position_history.distances[i];
              valid_count++;
          }
      }
      
      if (valid_count == 0) {
          position_history.is_moving = false;
          return;
      }
      
      mean /= valid_count;
      
      // Calculer variance
      float variance = 0.0f;
      for (int i = 0; i < 5; i++) {
          if (position_history.distances[i] > 0) {
              float diff = position_history.distances[i] - mean;
              variance += diff * diff;
          }
      }
      variance /= valid_count;
      
      // Si variance faible = pas de mouvement (robot probablement bloqué)
      // Si variance élevée = mouvement détecté (robot avance)
      position_history.is_moving = (variance > POSITION_VARIANCE_THRESHOLD);
  }
  ```

- [ ] **3.2** Implémenter fonction `checkIfStuckAdvanced()`:
  
  Remplacer la fonction `checkIfStuckImproved()` existante (ligne ~228):
  
  ```cpp
  /**
   * @brief Détecte si le robot est coincé (version améliorée)
   * @return true si bloqué, false sinon
   * @details Combine vérification distances ET mouvement réel
   */
  static bool checkIfStuckAdvanced() {
      if (!STUCK_DETECTION_ENABLED) {
          return false;
      }
      
      // Vérifier 1: Toutes directions bloquées (logique existante améliorée)
      if (last_scan.is_valid) {
          if (last_scan.distance_left < MIN_FREE_SPACE_CM &&
              last_scan.distance_front < MIN_FREE_SPACE_CM &&
              last_scan.distance_right < MIN_FREE_SPACE_CM) {
              stuck_memory.consecutive_stuck_count++;
              
              if (stuck_memory.consecutive_stuck_count >= STUCK_BACKUP_COUNT) {
                  return true;
              }
          } else {
              // Au moins une direction libre - reset compteur
              stuck_memory.consecutive_stuck_count = 0;
          }
      }
      
      // Vérifier 2: Pas de mouvement réel malgré commande FORWARD
      static uint8_t last_sent_command = CMD_STOP;
      static unsigned long last_command_time = 0;
      static unsigned long last_forward_command_time = 0;
      
      // Détecter si commande FORWARD envoyée
      // Note: On doit tracker la dernière commande envoyée
      // Pour simplifier, on vérifie dans STATE_FORWARD
      
      // Si commande FORWARD active depuis X ms et pas de mouvement détecté
      unsigned long now = millis();
      if (last_sent_command == CMD_FORWARD) {
          if (now - last_forward_command_time > STUCK_MOVEMENT_CHECK_MS) {
              if (!position_history.is_moving) {
                  stuck_memory.consecutive_failed_attempts++;
                  Serial.print("[AUTO] No movement detected - attempts: ");
                  Serial.println(stuck_memory.consecutive_failed_attempts);
                  
                  if (stuck_memory.consecutive_failed_attempts >= STUCK_THRESHOLD_ATTEMPTS) {
                      Serial.println("[AUTO] Stuck detected: no movement despite FORWARD command");
                      return true;
                  }
              } else {
                  // Mouvement détecté - reset compteur
                  stuck_memory.consecutive_failed_attempts = 0;
              }
          }
      } else {
          // Pas de commande FORWARD - reset
          last_forward_command_time = now;
          stuck_memory.consecutive_failed_attempts = 0;
      }
      
      // Combiner les deux vérifications
      return (stuck_memory.consecutive_stuck_count >= STUCK_BACKUP_COUNT);
  }
  ```

- [ ] **3.3** Intégrer dans machine à états:
  
  Dans `STATE_FORWARD` (ligne ~380), ajouter:
  
  **Après la mesure de distance (ligne ~391):**
  ```cpp
  // Mettre à jour historique de positions pour détection mouvement
  if (distance >= 0.0f && distance <= 400.0f) {
      updatePositionHistory(distance);
  }
  ```
  
  **Remplacer l'appel à `checkIfStuckImproved()` (ligne ~408):**
  ```cpp
  // Check if stuck using improved detection
  if (checkIfStuckAdvanced()) {  // Remplacer checkIfStuckImproved()
      // Vehicle is stuck - enter stuck pivoting state
      nav_state = STATE_STUCK_PIVOTING;
      command = CMD_STOP;
      Serial.println("[AUTO] Stuck detected - entering pivot mode");
      break;
  }
  ```
  
  **Ajouter tracking de commande FORWARD:**
  ```cpp
  // Track last sent command for stuck detection
  static uint8_t last_sent_command = CMD_STOP;
  static unsigned long last_forward_command_time = 0;
  
  if (command == CMD_FORWARD) {
      last_sent_command = CMD_FORWARD;
      last_forward_command_time = millis();
  } else {
      last_sent_command = command;
  }
  ```

**Validation:**
- [ ] Détection de blocage fonctionne (détecte mouvement réel)
- [ ] Pas de faux positifs (robot qui bouge normalement)
- [ ] Tests passent
- [ ] Code compile sans erreurs

---

### Task 4: Stratégies de Récupération Multiples
**Durée estimée:** 6 heures  
**Priorité:** HIGH (complète amélioration)

#### Subtasks

- [ ] **4.1** Implémenter fonction `executeRecoveryStrategy()`:
  
  Ajouter avant `task_autonomous()`:
  
  ```cpp
  /**
   * @brief Exécute une stratégie de récupération
   * @param strategy Stratégie à exécuter
   * @details Implémente 5 stratégies différentes pour éviter boucles
   */
  static void executeRecoveryStrategy(RecoveryStrategy strategy) {
      uint8_t cmd;
      
      Serial.print("[AUTO] Executing recovery strategy: ");
      
      switch (strategy) {
          case RECOVERY_BACKUP_TURN: {
              Serial.println("BACKUP_TURN");
              // Reculer puis tourner aléatoirement
              cmd = CMD_BACKWARD;
              xQueueSend(xCommandQueue, &cmd, 0);
              vTaskDelay(pdMS_TO_TICKS(BACKUP_TIME_MS));
              
              // Tourner aléatoirement gauche ou droite
              cmd = (random(0, 2) == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
              xQueueSend(xCommandQueue, &cmd, 0);
              vTaskDelay(pdMS_TO_TICKS(TURN_DURATION_MS));
              
              cmd = CMD_STOP;
              xQueueSend(xCommandQueue, &cmd, 0);
              break;
          }
          
          case RECOVERY_PIVOT_360: {
              Serial.println("PIVOT_360");
              // Rotation complète 360° pour scanner tout autour
              cmd = CMD_ROTATE_CW;
              xQueueSend(xCommandQueue, &cmd, 0);
              vTaskDelay(pdMS_TO_TICKS(TURN_DURATION_MS * 4)); // 4 × 90° = 360°
              
              cmd = CMD_STOP;
              xQueueSend(xCommandQueue, &cmd, 0);
              break;
          }
          
          case RECOVERY_BACKUP_LONG: {
              Serial.println("BACKUP_LONG");
              // Recul long pour s'éloigner de la zone problématique
              cmd = CMD_BACKWARD;
              xQueueSend(xCommandQueue, &cmd, 0);
              vTaskDelay(pdMS_TO_TICKS(STUCK_BACKUP_DURATION_MS));
              
              cmd = CMD_STOP;
              xQueueSend(xCommandQueue, &cmd, 0);
              break;
          }
          
          case RECOVERY_RANDOM_TURN: {
              Serial.println("RANDOM_TURN");
              // Tourner aléatoirement avec angle variable (90° à 270°)
              cmd = (random(0, 2) == 0) ? CMD_ROTATE_CCW : CMD_ROTATE_CW;
              xQueueSend(xCommandQueue, &cmd, 0);
              
              // Angle aléatoire entre 90° et 270°
              uint32_t random_duration = TURN_DURATION_MS + random(0, TURN_DURATION_MS * 2);
              vTaskDelay(pdMS_TO_TICKS(random_duration));
              
              cmd = CMD_STOP;
              xQueueSend(xCommandQueue, &cmd, 0);
              break;
          }
          
          case RECOVERY_WALL_FOLLOW: {
              Serial.println("WALL_FOLLOW (scan only)");
              // Simplifié: juste scanner pour trouver mur
              // Peut être étendu plus tard avec suivi de mur complet
              scan3Directions(ultrasonic_sensor, servo, last_scan);
              break;
          }
      }
      
      // Mettre à jour mémoire de récupération
      recovery_memory.last_strategy = strategy;
      recovery_memory.last_recovery_time = millis();
  }
  ```

- [ ] **4.2** Implémenter fonction `chooseRecoveryStrategy()`:
  
  ```cpp
  /**
   * @brief Choisit une stratégie de récupération (avec variation)
   * @param attempt_number Numéro de tentative (pour varier stratégies)
   * @return Stratégie choisie
   * @details Varie stratégie selon numéro de tentative pour éviter boucles
   */
  static RecoveryStrategy chooseRecoveryStrategy(int attempt_number) {
      RecoveryStrategy strategies[] = {
          RECOVERY_BACKUP_TURN,
          RECOVERY_PIVOT_360,
          RECOVERY_BACKUP_LONG,
          RECOVERY_RANDOM_TURN,
          RECOVERY_WALL_FOLLOW
      };
      
      // Varier stratégie selon numéro de tentative (modulo pour éviter répétition)
      int index = (attempt_number % 5);
      return strategies[index];
  }
  ```

- [ ] **4.3** Modifier état `STATE_STUCK_PIVOTING`:
  
  Dans le case `STATE_STUCK_PIVOTING` (ligne ~643), remplacer la logique:
  
  **AVANT (lignes ~643-686):**
  ```cpp
  case STATE_STUCK_PIVOTING: {
      // Pivot on place to find a clear path
      // Use triggered scan to find clear direction
      command = CMD_PIVOT_LEFT;  // Default pivot direction
      
      static uint32_t pivot_duration_ms = 0;
      static bool scan_triggered = false;
      pivot_duration_ms += AUTONOMOUS_TASK_PERIOD_MS;
      
      // Trigger scan periodically during pivot
      if (!scan_triggered && pivot_duration_ms >= 500) {
          // Trigger a scan to find clear path
          nav_state = STATE_SCAN;
          scan_triggered = true;
          pivot_duration_ms = 0;
          Serial.println("[AUTO] Triggering scan during pivot");
          break;
      }
      
      // ... reste du code
  }
  ```
  
  **APRÈS:**
  ```cpp
  case STATE_STUCK_PIVOTING: {
      // Pivot on place to find a clear path
      // Utiliser stratégies de récupération multiples
      command = CMD_STOP;
      
      static bool recovery_initiated = false;
      static unsigned long recovery_start_ms = 0;
      unsigned long now = millis();
      
      if (!recovery_initiated) {
          // Choisir et exécuter stratégie de récupération
          RecoveryStrategy strategy = chooseRecoveryStrategy(recovery_memory.attempt_count);
          executeRecoveryStrategy(strategy);
          
          recovery_initiated = true;
          recovery_start_ms = now;
          recovery_memory.attempt_count++;
          
          Serial.print("[AUTO] Recovery attempt #");
          Serial.println(recovery_memory.attempt_count);
      }
      
      // Attendre fin de stratégie (durée variable selon stratégie)
      // Après exécution, scanner pour trouver chemin
      if (now - recovery_start_ms > 2000) { // Attendre 2s pour stratégie complète
          // Scanner pour trouver chemin clair
          nav_state = STATE_SCAN;
          recovery_initiated = false;
          Serial.println("[AUTO] Recovery complete - scanning for clear path");
          break;
      }
      
      // Si trop de tentatives, essayer backup long
      if (recovery_memory.attempt_count >= RECOVERY_MAX_ATTEMPTS) {
          Serial.println("[AUTO] Max recovery attempts reached - trying long backup");
          executeRecoveryStrategy(RECOVERY_BACKUP_LONG);
          recovery_memory.attempt_count = 0; // Reset pour prochaine fois
          nav_state = STATE_FORWARD;
          recovery_initiated = false;
      }
      break;
  }
  ```

- [ ] **4.4** Tests:
  - Test chaque stratégie individuellement
  - Test variation de stratégies (vérifier pas de répétition)
  - Test récupération réussie après stratégie
  - Test max attempts (vérifier backup long après 5 tentatives)

**Validation:**
- [ ] Toutes stratégies fonctionnent
- [ ] Pas de boucle infinie (stratégies varient)
- [ ] Tests passent
- [ ] Code compile sans erreurs

---

### Task 5: Intégration et Tests
**Durée estimée:** 3 heures  
**Priorité:** HIGH (validation finale)

#### Subtasks

- [ ] **5.1** Intégration complète:
  - Vérifier toutes les modifications fonctionnent ensemble
  - Tester transitions entre états
  - Vérifier pas de régression sur fonctionnalité existante
  - Vérifier compatibilité avec autres modes (gesture, etc.)

- [ ] **5.2** Tests fonctionnels:
  - **Test tournant adaptatif:**
    - Obstacle large: vérifier rotation continue jusqu'à chemin clair
    - Obstacle petit: vérifier rotation s'arrête rapidement
    - Timeout: vérifier arrêt après max_duration
  - **Test détection de blocage:**
    - Coincer robot: vérifier détection
    - Robot bouge normalement: vérifier pas de faux positif
    - Toutes directions bloquées: vérifier détection
  - **Test stratégies de récupération:**
    - Tester chaque stratégie individuellement
    - Vérifier variation (pas de boucle)
    - Vérifier récupération réussie
  - **Test performance:**
    - Mesurer latence décision (doit être < 800ms)
    - Vérifier pas de dégradation vs version actuelle

- [ ] **5.3** Tests de régression:
  - Mode autonome fonctionne toujours
  - Pas de crash ou freeze
  - Compatibilité avec autres modes
  - Scan 3 directions fonctionne toujours
  - Décision de direction fonctionne toujours

- [ ] **5.4** Documentation:
  - Commenter nouvelles fonctions (format Doxygen)
  - Documenter paramètres configurables dans `config.h`
  - Mettre à jour commentaires si nécessaire

**Validation:**
- [ ] Tous les tests passent
- [ ] Pas de régression
- [ ] Code documenté
- [ ] Performance acceptable (< 800ms latence)

---

## Dev Notes

### Fichiers à Modifier

1. **`Vehicule/src/config.h`**
   - Ajouter constantes de configuration (Task 1.1)

2. **`Vehicule/src/tasks/task_autonomous.cpp`**
   - Ajouter structures de données (Task 1.2, 1.3)
   - Initialiser structures (Task 1.4)
   - Implémenter `adaptiveTurn()` (Task 2.1)
   - Modifier `STATE_ACTION` (Task 2.2)
   - Implémenter `updatePositionHistory()` (Task 3.1)
   - Implémenter `checkIfStuckAdvanced()` (Task 3.2)
   - Modifier `STATE_FORWARD` (Task 3.3)
   - Implémenter `executeRecoveryStrategy()` (Task 4.1)
   - Implémenter `chooseRecoveryStrategy()` (Task 4.2)
   - Modifier `STATE_STUCK_PIVOTING` (Task 4.3)

### Fichiers à Consulter (Référence)

- `Vehicule/src/drivers/servo_driver.h` - Interface servo (`setAngle()`, `stopSweep()`)
- `Vehicule/src/drivers/ultrasonic_driver.h` - Interface capteur (`readDistanceCM()`)
- `Vehicule/src/shared/queues.h` - Queues FreeRTOS (`xCommandQueue`)
- `Vehicule/src/communication/command_protocol.h` - Commandes disponibles (`CMD_*`)

### Points Critiques

1. **FreeRTOS:** 
   - ✅ Utiliser `vTaskDelay()` et `vTaskDelayUntil()`, **JAMAIS** `delay()`
   - ✅ Respecter période de tâche (50ms) - ne pas bloquer trop longtemps

2. **Queues:**
   - ✅ Toujours vérifier `pdTRUE` après `xQueueSend()`
   - ✅ Gérer cas queue pleine (log warning)

3. **Timing:**
   - ⚠️ Le scan 3 directions prend ~600-750ms - ne pas bloquer la tâche
   - ⚠️ Le tournant adaptatif vérifie toutes les 50ms - acceptable
   - ⚠️ Les stratégies de récupération peuvent prendre 1-4 secondes - acceptable

4. **Mémoire:**
   - ✅ Utiliser structures statiques (pas d'allocation dynamique)
   - ✅ Limiter historique à 5 éléments (économique)
   - ✅ Vérifier stack size suffisant (actuellement 4096 bytes - devrait être OK)

5. **Compatibilité:**
   - ✅ Ne pas casser fonctionnalité existante
   - ✅ Maintenir compatibilité avec autres modes
   - ✅ Tester régression complète

### Ordre d'Implémentation Recommandé

1. **Task 1** (Configuration) - **BASE** pour toutes les autres tâches
2. **Task 2** (Tournant adaptatif) - Amélioration immédiate visible
3. **Task 3** (Détection blocage) - Améliore robustesse
4. **Task 4** (Stratégies récupération) - Complète amélioration
5. **Task 5** (Tests) - Validation finale

### Notes Techniques

- **Randomisation:** Utiliser `random()` d'Arduino pour randomisation stratégies
  - Initialiser avec `randomSeed(analogRead(0))` dans setup si nécessaire
- **Historique circulaire:** Utiliser modulo (`%`) pour index circulaire
- **Variance:** Formule: `variance = Σ(xi - mean)² / n`
- **Timing:** Respecter période de tâche (50ms) - utiliser `vTaskDelay()` pour délais
- **Servo:** Toujours attendre stabilisation (50ms pour mesure rapide, 200ms pour scan complet)

---

## Testing

### Tests Unitaires (Si Possible)

- Test `adaptiveTurn()` avec différents scénarios (obstacle large/petit, timeout)
- Test `updatePositionHistory()` avec données simulées (vérifier calcul variance)
- Test `checkIfStuckAdvanced()` avec différents états (mouvement, bloqué)
- Test `chooseRecoveryStrategy()` (vérifier variation selon attempt_number)

### Tests Fonctionnels

1. **Test Tournant Adaptatif:**
   - Obstacle large (ex. mur): vérifier rotation continue jusqu'à chemin clair
   - Obstacle petit (ex. chaise): vérifier rotation s'arrête rapidement
   - Timeout: vérifier arrêt après max_duration si pas de chemin clair

2. **Test Détection de Blocage:**
   - Coincer robot dans coin: vérifier détection
   - Robot bouge normalement: vérifier pas de faux positif
   - Toutes directions bloquées: vérifier détection rapide

3. **Test Stratégies de Récupération:**
   - Tester chaque stratégie individuellement
   - Vérifier variation (pas de répétition même stratégie)
   - Vérifier récupération réussie après stratégie

4. **Test Performance:**
   - Mesurer latence décision (doit être < 800ms)
   - Vérifier pas de dégradation vs version actuelle
   - Vérifier consommation mémoire (pas de leak)

### Tests de Régression

- Mode autonome fonctionne toujours
- Pas de crash ou freeze
- Compatibilité avec autres modes (gesture, etc.)
- Scan 3 directions fonctionne toujours
- Décision de direction fonctionne toujours

---

## File List

### Fichiers Modifiés
- `Vehicule/src/config.h` - Ajout constantes de configuration
- `Vehicule/src/tasks/task_autonomous.cpp` - Implémentation des améliorations

### Fichiers Créés
- Aucun (modifications dans fichiers existants)

### Fichiers Supprimés
- Aucun

---

## Dev Agent Record

### Agent Model Used
- (À remplir par l'agent dev)

### Debug Log References
- (À remplir par l'agent dev pendant développement)

### Completion Notes
- (À remplir par l'agent dev après complétion)

### Change Log
- (À remplir par l'agent dev avec changements effectués)

---

## Status

**Current Status:** Ready for Development

**Next Steps:**
1. Agent dev lit ce document complet
2. Agent dev commence Task 1 (Configuration) - **CRITICAL**
3. Agent dev suit ordre d'implémentation recommandé
4. Agent dev met à jour checkboxes et sections Dev Agent Record au fur et à mesure
5. Agent dev valide chaque task avant de passer au suivant

---

**Document créé pour amélioration mode autonome - Prêt pour implémentation par agent dev**

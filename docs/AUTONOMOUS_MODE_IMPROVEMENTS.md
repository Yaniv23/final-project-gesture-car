# Améliorations du Mode Autonome

## Problème Identifié

Le mode autonome ne fonctionnait pas car:
1. **Le servo n'était pas utilisé** - Le code utilisait uniquement un capteur ultrasonique fixe devant
2. **Logique de navigation trop simple** - Pas de scan de l'environnement, décisions basées uniquement sur le devant
3. **Gestion d'erreurs insuffisante** - Les erreurs de capteur n'étaient pas bien gérées

## Solution Implémentée

### 1. Utilisation du Servo pour Scanner l'Environnement

Le véhicule scanne maintenant l'environnement dans 3 directions:
- **Gauche** (30°)
- **Centre** (0°)
- **Droite** (30°)

### 2. Algorithme de Navigation Amélioré

#### États de Navigation:
- **STATE_SCANNING**: Scan de l'environnement (gauche → centre → droite)
- **STATE_FORWARD**: Avance si le centre est libre
- **STATE_TURNING**: Tourne vers la meilleure direction si obstacle devant
- **STATE_STOPPED**: Arrêté en cas d'erreur (récupération automatique)

#### Logique de Décision:
1. **Scan complet**: Mesure les distances gauche, centre, droite
2. **Décision intelligente**:
   - Si centre ≥ seuil (20cm) → **AVANCER**
   - Si centre bloqué → **TOURNER** vers la direction avec la plus grande distance libre
   - Si toutes directions bloquées → **TOURNER DROITE** par défaut
3. **Rescan périodique**: Toutes les 2 secondes en mode forward pour détecter les obstacles

### 3. Améliorations Techniques

- **Gestion du timing**: Délai de 150ms pour permettre au servo de se positionner
- **Gestion d'erreurs**: Si le capteur retourne une erreur, traité comme obstacle (sécurité)
- **Logging détaillé**: Messages Serial pour déboguer chaque étape
- **Récupération automatique**: Après une erreur, tentative de récupération après 1 seconde

## Configuration

Les paramètres sont définis dans `config.h`:
- `OBSTACLE_DISTANCE_THRESHOLD_CM = 20` - Distance minimale avant obstacle
- `TURN_DURATION_MS = 1000` - Durée de rotation (1 seconde)
- `AUTONOMOUS_TASK_PERIOD_MS = 50` - Période de la tâche (20 Hz)

## Comment Tester

1. **Compilez et uploadez** le code sur l'ESP32
2. **Activez le mode autonome** via la commande ESP-NOW ou le toggle
3. **Observez les messages Serial**:
   ```
   [AUTO] Entered autonomous mode - Sensors initialized
   [AUTO] Starting environment scan...
   [AUTO] Left distance: XX cm
   [AUTO] Center distance: XX cm
   [AUTO] Right distance: XX cm
   [AUTO] Best direction: CENTER (distance: XX cm)
   [AUTO] Decision: FORWARD
   [MOTOR] FORWARD
   ```
4. **Vérifiez le comportement**:
   - Le véhicule devrait scanner l'environnement
   - Puis avancer si le chemin est libre
   - Tourner si un obstacle est détecté
   - Rescanner périodiquement

## Dépannage

### Le véhicule ne bouge toujours pas:

1. **Vérifiez les messages Serial**:
   - Est-ce que les capteurs s'initialisent correctement?
   - Est-ce que les distances sont mesurées?
   - Quelle décision est prise?

2. **Vérifiez les connexions**:
   - Servo sur pin 4
   - Ultrasonic TRIG sur pin 12, ECHO sur pin 16

3. **Vérifiez le mode**:
   - Le mode autonome est-il bien activé?
   - `[MOTOR] Mode: AUTONOMOUS` devrait apparaître

4. **Vérifiez la queue de commandes**:
   - Si "Command queue full!" apparaît, il y a un problème de communication

### Le véhicule tourne en rond:

- Réduisez `TURN_DURATION_MS` dans `config.h`
- Augmentez `OBSTACLE_DISTANCE_THRESHOLD_CM` pour détecter les obstacles plus tôt

### Le servo ne bouge pas:

- Vérifiez que le servo est bien connecté sur pin 4
- Vérifiez que le LEDC channel 4 n'est pas utilisé ailleurs
- Augmentez `SERVO_MOVE_DELAY_MS` si le servo est lent

## Prochaines Améliorations Possibles

1. **Scan continu**: Utiliser le sweep du servo au lieu de positions fixes
2. **Mémoire des obstacles**: Se souvenir des obstacles récents
3. **Vitesse adaptative**: Ralentir près des obstacles
4. **Détection de mur**: Détecter si on est bloqué de tous les côtés
5. **Navigation vers un objectif**: Ajouter une boussole ou GPS pour navigation ciblée

<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# Prepare moi un mardown avec la meilleur stategie d algo d eviteur d obsacle baser sur toute les recherche que tu as fais precedement

Voici un Markdown prêt à copier-coller, avec une stratégie d’algorithme d’évitement d’obstacles simple, robuste et adaptée à ton matériel (ESP32/Arduino + 1 HC‑SR04 sur servo, + éventuellement un 2ᵉ capteur fixe).

```markdown
# 🚗 Algorithme d’Évitement d’Obstacles – Stratégie Recommandée

Objectif : faire un véhicule DIY (Arduino ou ESP32) qui se déplace **en autonomie** en évitant les obstacles, avec des capteurs ultrasoniques (HC‑SR04) et un servomoteur.

---

## 1. Architecture de l’Algorithme

L’algorithme recommandé est un **scan 3‑directions déclenché uniquement en cas d’obstacle**.  
C’est ce qu’on retrouve dans la plupart des projets d’« obstacle avoiding car » bien faits (Arduino + HC‑SR04 + servo).  

### Boucle générale

1. Avancer tout droit en lisant la distance **devant**.
2. Si un obstacle est détecté à moins d’un certain seuil (ex. 18 cm) :
   - Arrêter les moteurs.
   - Scanner **gauche – avant – droite** avec le servomoteur + HC‑SR04.
   - Choisir la direction avec **le plus d’espace libre**.
   - Tourner dans cette direction puis repartir.
3. Sinon, continuer tout droit.

Ce principe permet:
- De limiter les mesures complexes (on scanne seulement quand c’est utile).
- De réduire les interférences entre capteurs.
- D’avoir un comportement fiable sans algo trop compliqué.

---

## 2. Capteurs et Rôle de Chacun

### 2.1 Capteur ultrasonique HC‑SR04

- Rôle : mesurer la distance jusqu’à l’obstacle devant le robot.
- Plage utile : 5–200 cm.
- Utilisation :
  - 1 capteur monté sur un *servo* pour scanner plusieurs directions.
  - Optionnel : un 2ᵉ capteur fixe à l’arrière pour sécuriser la marche arrière.

### 2.2 Servomoteur (scanner)

- Rôle : orienter le capteur HC‑SR04 vers la **gauche**, **l’avant** ou la **droite**.
- Angles typiques :
  - Gauche : 45°
  - Avant : 90°
  - Droite : 135°
- Délai de stabilisation recommandé : 200–250 ms après chaque changement d’angle avant de mesurer, pour laisser le servo se stabiliser.

---

## 3. États de l’Algorithme (Machine à États Simple)

On peut décrire la logique comme une machine à états simple :

1. **ÉTAT AVANCE**
   - Le robot avance.
   - Il mesure régulièrement la distance devant (ex. toutes les 100 ms).
   - Si `distance_front < CRITICAL_DISTANCE`, passer à **ÉTAT SCAN**.

2. **ÉTAT SCAN**
   - Stopper les moteurs.
   - Scanner trois directions via le servo:
     - Gauche (45°)
     - Avant (90°)
     - Droite (135°)
   - Stocker `dist_left`, `dist_front`, `dist_right`.
   - Passer à **ÉTAT DECISION**.

3. **ÉTAT DECISION**
   - Comparer les distances:
     - Si `dist_left` est la plus grande et > `MIN_FREE_SPACE` → choisir GAUCHE.
     - Sinon si `dist_right` est la plus grande et > `MIN_FREE_SPACE` → choisir DROITE.
     - Sinon si `dist_front` > `MIN_FREE_SPACE` → continuer AVANT.
     - Sinon → on considère que tout est bloqué → **Demi‑tour / marche arrière**.
   - Passer à **ÉTAT ACTION**.

4. **ÉTAT ACTION**
   - Exécuter le mouvement choisi:
     - Tourner à gauche ou droite pendant une durée donnée (ex. 700–900 ms).
     - Ou reculer un peu puis tourner (si tout est bloqué).
   - Revenir ensuite à **ÉTAT AVANCE**.

---

## 4. Pseudo‑Code de l’Algorithme

### 4.1 Boucle principale

```c
loop() {
  distance_front = measureDistanceFront();

  if (distance_front < CRITICAL_DISTANCE) {
    stopMotors();
    scan3Directions();          // Remplit dist_left, dist_front, dist_right
    direction = decideDirection();
    executeMovement(direction); // Tourne ou recule + tourne
  } else {
    moveForward();
  }

  delay(100); // petit délai pour ne pas saturer
}
```


### 4.2 Scan des 3 directions

```c
void scan3Directions() {
  // Gauche
  servo.write(45);
  delay(SERVO_DELAY);
  dist_left = measureDistanceFront();

  // Avant
  servo.write(90);
  delay(SERVO_DELAY);
  dist_front = measureDistanceFront();

  // Droite
  servo.write(135);
  delay(SERVO_DELAY);
  dist_right = measureDistanceFront();

  // Recentrer
  servo.write(90);
}
```


### 4.3 Décision de la direction

```c
int decideDirection() {
  // 0 = GAUCHE, 1 = DROITE, 2 = AVANT, 3 = DEMI_TOUR

  if (dist_left > MIN_FREE_SPACE && dist_left > dist_right) {
    return 0; // GAUCHE
  } else if (dist_right > MIN_FREE_SPACE && dist_right >= dist_left) {
    return 1; // DROITE
  } else if (dist_front > MIN_FREE_SPACE) {
    return 2; // AVANT
  } else {
    return 3; // DEMI_TOUR / MARCHE ARRIÈRE
  }
}
```


### 4.4 Exécution du mouvement

```c
void executeMovement(int direction) {
  switch (direction) {
    case 0: // GAUCHE
      turnLeft();
      break;
    case 1: // DROITE
      turnRight();
      break;
    case 2: // AVANT
      moveForward();
      break;
    case 3: // DEMI_TOUR
      moveBackward();
      delay(TURN_DELAY / 2);
      turnRight();  // ou turnLeft()
      break;
  }
}
```


---

## 5. Paramètres Conseillés (Tuning de Base)

Ces valeurs sont un bon point de départ, à ajuster selon la taille et la vitesse de ton robot:


| Paramètre | Valeur typique | Rôle |
| :-- | :-- | :-- |
| `CRITICAL_DISTANCE` | 15–20 cm | Distance d’arrêt avant obstacle |
| `MIN_FREE_SPACE` | 25–30 cm | Espace minimum pour choisir une voie |
| `SERVO_DELAY` | 200–250 ms | Stabilisation servo avant mesure |
| `TURN_DELAY` | 700–900 ms | Durée de rotation ~90° |
| `MOTOR_SPEED` | 180–220 (0–255) | Vitesse de déplacement |


---

## 6. Améliorations Simples

Une fois la base fonctionnelle, tu peux améliorer :

### 6.1 Filtrage des mesures

Faire la moyenne de plusieurs mesures pour réduire le bruit:

```c
int filteredDistance() {
  int sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += measureDistanceFront();
    delay(20);
  }
  return sum / 5;
}
```

Remplacer `measureDistanceFront()` dans l’algo par `filteredDistance()`.

### 6.2 Détection de blocage (robot coincé)

Si le robot reste trop longtemps à tourner/recule sans trouver d’issue:

- Compter le nombre de fois où l’on tombe dans le cas « tout bloqué ».
- Au‑delà d’un certain nombre (ex. 3), faire un mouvement spécial :
    - Longue marche arrière.
    - Rotation de 180°.

Exemple:

```c
int stuckCount = 0;

if (direction == 3) { // DEMI_TOUR
  stuckCount++;
  if (stuckCount > 3) {
    // mouvement spécial
    moveBackward();
    delay(1000);
    turnRight();
    turnRight(); // ~180°
    stuckCount = 0;
  }
} else {
  stuckCount = 0;
}
```


### 6.3 Utilisation d’un 2ᵉ capteur (arrière)

Si tu montes un deuxième HC‑SR04 à l’arrière:

- L’utiliser avant de reculer:
    - si `distance_back < 15 cm`, **ne pas reculer**, chercher une rotation sur place plutôt.

---

## 7. Résumé de la Stratégie

1. **Simple** : une seule boucle principale, 3 états logiques (AVANCE / SCAN / DÉCISION+ACTION).
2. **Robuste** : seuils ajustables + filtrage possible + gestion de blocage.
3. **Économe** : le scan complet ne se fait **que** quand un obstacle est proche.
4. **Extensible** : facile d’ajouter d’autres capteurs ou l’IMU plus tard.

Cette stratégie est un excellent compromis pour un projet DIY avec Arduino/ESP32, 1–2 capteurs ultrason + servo : elle est simple à coder, facile à débugger, et fonctionne bien dans la majorité des environnements (couloir, salon, maquette de circuit, etc.).

---
```

Si tu veux, je peux ensuite te générer une **version avec du code C/C++ complet** prêt à coller dans l’IDE Arduino (avec `setup()` et `loop()` inclus).```


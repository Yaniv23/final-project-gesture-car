# Checklist de Nettoyage - Suppression des Fonctionnalités Non Utilisées

**Date de création**: 2024  
**Objectif**: Identifier et supprimer de manière sûre les fonctionnalités non utilisées ou non implémentées sans impacter les fonctionnalités actives.

---

## 🔍 Résumé Exécutif

Ce document liste toutes les fonctionnalités, fichiers, et code mort identifiés dans le projet qui peuvent être supprimés en toute sécurité. Chaque élément est classé par priorité et inclut des instructions de vérification avant suppression.

---

## ⚠️ AVANT DE COMMENCER

1. **Créer une branche Git** pour les modifications
2. **Faire un commit** de l'état actuel
3. **Tester le système** avant et après chaque suppression
4. **Vérifier les dépendances** avec `grep` avant suppression

---

## 📋 CATÉGORIE 1: Fichiers/Dossiers Obsolètes (Priorité HAUTE)

### ✅ 1.1 Dossier MPU/ (Non utilisé)

**Emplacement**: `/MPU/`  
**Raison**: Bibliothèque MPU9250 installée mais jamais utilisée dans le code  
**Vérification**:
```bash
# Vérifier qu'aucun fichier ne référence MPU9250
grep -r "MPU9250\|MPU" Vehicule/src/ pc_side/ --exclude-dir=.pio
grep -r "MPU9250\|MPU" test/ --exclude-dir=.pio
```

**Action**:
- [ ] Supprimer le dossier `/MPU/` entier
- [ ] Vérifier qu'aucune dépendance PlatformIO ne référence MPU9250

**Impact**: Aucun (non utilisé)

---

### ✅ 1.2 Dossier Wokwi/ (Simulation non utilisée)

**Emplacement**: `/Wokwi/`  
**Raison**: Fichiers de simulation Wokwi non intégrés dans le workflow  
**Vérification**:
```bash
# Vérifier les références à Wokwi
grep -r "wokwi\|Wokwi" . --exclude-dir=.pio --exclude-dir=.git
```

**Action**:
- [ ] Supprimer `/Wokwi/diagram.json`
- [ ] Supprimer `/Wokwi/l298n.chip.c`
- [ ] Supprimer le dossier `/Wokwi/` si vide

**Impact**: Aucun (non utilisé en production)

---

### ✅ 1.3 Fichier Vehicule_Controller.ino (Ancien code Arduino)

**Emplacement**: `/Vehicule/Vehicule_Controller.ino`  
**Raison**: Ancien fichier Arduino remplacé par `main.cpp` (FreeRTOS)  
**Vérification**:
```bash
# Vérifier si ce fichier est encore référencé
grep -r "Vehicule_Controller\.ino" . --exclude-dir=.pio --exclude-dir=.git
# Résultat attendu: seulement des commentaires dans les fichiers sources
```

**Action**:
- [ ] Vérifier que seuls des commentaires référencent ce fichier
- [ ] Supprimer `/Vehicule/Vehicule_Controller.ino`
- [ ] Optionnel: Conserver comme référence dans `/docs/archive/` si nécessaire

**Impact**: Aucun (code remplacé par main.cpp)

---

## 📋 CATÉGORIE 2: Code Non Implémenté (Priorité MOYENNE)

### ✅ 2.1 Task Telemetry (Vide - Seulement TODOs)

**Emplacement**: `/Vehicule/src/tasks/task_telemetry.cpp`  
**Raison**: Task créée mais complètement vide (seulement des TODOs)  
**Vérification**:
```bash
# Vérifier que task_telemetry n'est utilisée nulle part
grep -r "task_telemetry\|xMotorStatusQueue" Vehicule/src/ --exclude-dir=.pio
```

**Action**:
- [ ] Vérifier que `task_telemetry` n'est appelée que dans `main.cpp` (création)
- [ ] Supprimer la création de task dans `main.cpp` (lignes 214-223)
- [ ] Supprimer `taskHandle_telemetry` (ligne 55)
- [ ] Supprimer le fichier `/Vehicule/src/tasks/task_telemetry.cpp`
- [ ] Supprimer la déclaration forward dans `main.cpp` (ligne 39)

**Impact**: Aucun (fonctionnalité jamais implémentée)

---

### ✅ 2.2 Task Sensor Fusion (Vide - Ne fait rien)

**Emplacement**: `/Vehicule/src/tasks/task_sensor_fusion.cpp`  
**Raison**: Task créée mais ne fait rien (juste des `vTaskDelay`)  
**Vérification**:
```bash
# Vérifier que task_sensor_fusion n'est utilisée que pour création
grep -r "task_sensor_fusion" Vehicule/src/ --exclude-dir=.pio
```

**Action**:
- [ ] Vérifier que `task_sensor_fusion` n'est appelée que dans `main.cpp` (création)
- [ ] Supprimer la création de task dans `main.cpp` (lignes 177-186)
- [ ] Supprimer `taskHandle_sensor` (ligne 53)
- [ ] Supprimer le fichier `/Vehicule/src/tasks/task_sensor_fusion.cpp`
- [ ] Supprimer la déclaration forward dans `main.cpp` (ligne 37)

**Impact**: Aucun (les capteurs sont gérés par `task_autonomous`)

---

### ✅ 2.3 Queue xMotorStatusQueue (Jamais utilisée)

**Emplacement**: `/Vehicule/src/shared/queues.h` et `queues.cpp`  
**Raison**: Queue créée mais jamais utilisée (réservée pour telemetry)  
**Vérification**:
```bash
# Vérifier que xMotorStatusQueue n'est jamais utilisé
grep -r "xMotorStatusQueue" Vehicule/src/ --exclude-dir=.pio
# Résultat attendu: seulement déclaration et création, pas d'utilisation
```

**Action**:
- [ ] Supprimer la déclaration dans `queues.h` (ligne 30)
- [ ] Supprimer la variable globale dans `queues.cpp` (ligne 19)
- [ ] Supprimer la création dans `initSharedQueues()` (lignes 50-56)

**Impact**: Aucun (jamais utilisée)

---

### ✅ 2.4 Queue xMotionCommandQueue (Réservée pour futur)

**Emplacement**: `/Vehicule/src/shared/queues.h` et `queues.cpp`  
**Raison**: Queue créée mais jamais utilisée (réservée pour future implémentation)  
**Vérification**:
```bash
# Vérifier que xMotionCommandQueue n'est jamais utilisé
grep -r "xMotionCommandQueue" Vehicule/src/ --exclude-dir=.pio
# Résultat attendu: seulement déclaration et création, pas d'utilisation
```

**Action**:
- [ ] **DÉCISION**: Si pas prévu dans les 6 prochains mois, supprimer
- [ ] Supprimer la déclaration dans `queues.h` (ligne 26)
- [ ] Supprimer la variable globale dans `queues.cpp` (ligne 16)
- [ ] Supprimer la création dans `initSharedQueues()` (lignes 41-48)

**Impact**: Aucun (jamais utilisée, mais peut être réintroduite si nécessaire)

---

### ✅ 2.5 Fonction test_command_reception() (Déclarée mais jamais implémentée)

**Emplacement**: `/Vehicule/src/main.cpp` (ligne 43)  
**Raison**: Fonction déclarée mais jamais implémentée  
**Vérification**:
```bash
# Vérifier l'implémentation
grep -r "test_command_reception" Vehicule/src/ --exclude-dir=.pio
```

**Action**:
- [ ] Supprimer la déclaration forward dans `main.cpp` (ligne 43)
- [ ] Vérifier qu'elle n'est jamais appelée

**Impact**: Aucun (non implémentée)

---

## 📋 CATÉGORIE 3: Code de Test/Simulation (Priorité BASSE)

### ✅ 3.1 Mode SIMULATION_MODE (Si non utilisé)

**Emplacement**: `/Vehicule/src/config.h` (ligne 91)  
**Raison**: Mode de simulation pour tests sans ESP-NOW  
**Vérification**:
```bash
# Vérifier l'utilisation de SIMULATION_MODE
grep -r "SIMULATION_MODE" Vehicule/src/ --exclude-dir=.pio
```

**Action**:
- [ ] **DÉCISION**: Si jamais utilisé en production, supprimer
- [ ] Supprimer `#define SIMULATION_MODE` dans `config.h`
- [ ] Supprimer tous les `#if SIMULATION_MODE` et `#if !SIMULATION_MODE`
- [ ] Supprimer `task_test_commands` et sa création (lignes 225-236 dans main.cpp)
- [ ] Supprimer `taskHandle_test` (ligne 57)
- [ ] Nettoyer `espnow_handler.cpp` (lignes 125, 203)
- [ ] Nettoyer `task_communication.cpp` (lignes 30, 41)

**Impact**: Moyen (si utilisé pour tests, garder; sinon supprimer)

---

### ✅ 3.2 Fonction test_motor_led_actuation() (Seulement en SIMULATION_MODE)

**Emplacement**: `/Vehicule/src/main.cpp` (lignes 281-327)  
**Raison**: Fonction de test utilisée uniquement en mode simulation  
**Vérification**:
```bash
# Vérifier l'utilisation
grep -r "test_motor_led_actuation" Vehicule/src/ --exclude-dir=.pio
```

**Action**:
- [ ] Si `SIMULATION_MODE` est supprimé, supprimer cette fonction
- [ ] Supprimer la déclaration forward (ligne 44)
- [ ] Supprimer l'implémentation (lignes 281-327)
- [ ] Supprimer `task_test_commands` (lignes 270-274)

**Impact**: Aucun (seulement pour tests)

---

## 📋 CATÉGORIE 4: Documentation Obsolète (Priorité TRÈS BASSE)

### ✅ 4.1 Fichiers de Documentation Non Finalisés

**Emplacement**: `/docs/`  
**Fichiers suspects**:
- `Untitled` (2 lignes seulement)
- `Prepare moi un mardown avec la meilleur stategie d.md` (nom suspect)

**Action**:
- [ ] Vérifier le contenu de `docs/Untitled`
- [ ] Vérifier le contenu de `docs/Prepare moi un mardown avec la meilleur stategie d.md`
- [ ] Supprimer si vide ou non pertinent
- [ ] Renommer si le contenu est utile

**Impact**: Aucun (documentation)

---

## 📋 CATÉGORIE 5: Dossiers Référencés mais Inexistants

### ✅ 5.1 Dossiers Mentionnés dans README mais Inexistants

**Emplacement**: Structure du projet  
**Dossiers**: `Motors/`, `Sensors/`, `Transmission/` (mentionnés dans certains docs mais n'existent pas)

**Action**:
- [ ] Vérifier les références dans la documentation
- [ ] Mettre à jour la documentation si nécessaire
- [ ] Pas d'action de suppression nécessaire (n'existent pas)

**Impact**: Aucun (juste documentation)

---

## 🔧 PROCÉDURE DE SUPPRESSION RECOMMANDÉE

### Étape 1: Préparation
```bash
# Créer une branche
git checkout -b cleanup/unused-features

# Commit l'état actuel
git add .
git commit -m "Checkpoint before cleanup"
```

### Étape 2: Suppression par Catégorie

**Ordre recommandé**:
1. **Catégorie 1** (Fichiers/Dossiers) - Impact minimal
2. **Catégorie 2** (Code non implémenté) - Impact minimal
3. **Catégorie 3** (Code de test) - Vérifier avant suppression
4. **Catégorie 4** (Documentation) - Impact minimal

### Étape 3: Tests après chaque suppression

```bash
# Compiler le projet
cd Vehicule
pio run

# Vérifier qu'il n'y a pas d'erreurs
pio run -t check

# Tester le système si possible
```

### Étape 4: Validation finale

```bash
# Vérifier qu'aucune référence orpheline n'existe
grep -r "task_telemetry\|task_sensor_fusion\|xMotorStatusQueue\|xMotionCommandQueue" Vehicule/src/ --exclude-dir=.pio

# Vérifier la compilation
cd Vehicule && pio run
```

---

## 📊 RÉSUMÉ DES ÉLÉMENTS À SUPPRIMER

| Élément | Type | Priorité | Impact | Risque |
|---------|------|----------|--------|--------|
| `/MPU/` | Dossier | HAUTE | Aucun | ⚠️ Faible |
| `/Wokwi/` | Dossier | HAUTE | Aucun | ⚠️ Faible |
| `Vehicule_Controller.ino` | Fichier | HAUTE | Aucun | ⚠️ Faible |
| `task_telemetry.cpp` | Fichier | MOYENNE | Aucun | ⚠️ Faible |
| `task_sensor_fusion.cpp` | Fichier | MOYENNE | Aucun | ⚠️ Faible |
| `xMotorStatusQueue` | Code | MOYENNE | Aucun | ⚠️ Faible |
| `xMotionCommandQueue` | Code | MOYENNE | Aucun | ⚠️ Faible |
| `SIMULATION_MODE` | Code | BASSE | Moyen | ⚠️ Moyen |
| `test_motor_led_actuation()` | Fonction | BASSE | Aucun | ⚠️ Faible |
| `docs/Untitled` | Fichier | TRÈS BASSE | Aucun | ⚠️ Faible |

---

## ✅ CHECKLIST FINALE

Avant de finaliser la suppression:

- [ ] Tous les tests de compilation passent
- [ ] Aucune référence orpheline dans le code
- [ ] Documentation mise à jour si nécessaire
- [ ] Commit Git avec message descriptif
- [ ] Test fonctionnel du système (si possible)

---

## 📝 NOTES IMPORTANTES

1. **SIMULATION_MODE**: Si utilisé pour développement/tests, **NE PAS SUPPRIMER**. Vérifier avec l'équipe avant suppression.

2. **xMotionCommandQueue**: Si une implémentation de contrôle de vélocité est prévue dans les 6 prochains mois, **NE PAS SUPPRIMER**.

3. **Documentation**: Les fichiers de documentation peuvent être conservés même s'ils sont incomplets, sauf s'ils sont clairement obsolètes.

4. **Tests**: Les dossiers `/test/` doivent être conservés car ils sont utilisés pour les tests.

---

## 🎯 RÉSULTAT ATTENDU

Après cette cleanup:
- **Réduction de la complexité** du codebase
- **Amélioration de la maintenabilité**
- **Réduction de la confusion** pour les nouveaux développeurs
- **Code plus clair** et focalisé sur les fonctionnalités actives

---

**Dernière mise à jour**: Analyse complète du projet effectuée  
**Prochaine révision**: Après implémentation des suppressions

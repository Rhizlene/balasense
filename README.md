# BalaSense - Smart Racing Biometric Monitor

> Système de monitoring biométrique embarqué pour pilotes de sport automobile

[![Status](https://img.shields.io/badge/status-in%20development-yellow)]()
[![Hardware](https://img.shields.io/badge/hardware-ESP32-blue)]()
[![Sensors](https://img.shields.io/badge/sensors-7%20capteurs-green)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

---

## Description

**BalaSense** est un projet personnel de recherche et développement visant à créer une balaclava intelligente pour le monitoring en temps réel des données biométriques et physiologiques des pilotes de course automobile.

### Objectifs principaux
- **Sécurité** : Détection précoce des signes de fatigue ou stress excessif
- **Performance** : Analyse de l'état physiologique en corrélation avec la performance en piste
- **Innovation** : Application IoT dans le domaine du motorsport de haut niveau

---

## Fonctionnalités clés

### Monitoring biométrique complet (7 capteurs)

| Capteur | Mesure | Utilité |
|---------|--------|---------|
| MAX30102 | Rythme cardiaque (BPM) + HRV + SpO2 | Stress, fatigue, oxygénation |
| MLX90614 | Température frontale (IR) | Fièvre, surchauffe corporelle |
| ICM-20948 | Mouvements 9 axes (G-force) | Impacts, analyse pilotage |
| MH-Z19C | CO₂ respiratoire | Fatigue mentale, respiration |
| GSR/EDA | Conductance peau | Stress physiologique |
| Textile conducteur | Hydratation/transpiration | Déshydratation |
| SpO2 | Saturation oxygène | Performance cardio |

### Transmission temps réel
- Wi-Fi / Bluetooth LE
- API REST / MQTT
- Dashboard web

### Autonomie optimisée
- Batterie Li-Po 1000mAh
- Boost converter intelligent (3.7V → 5V)
- Autonomie cible : 3h minimum

---

## Architecture technique

### Hardware
- **Microcontrôleur** : ESP32 Dev Module (ESP32-PICO-D4)
- **Alimentation** : 
  - Batterie Li-Po 3.7V (1000mAh)
  - Boost MT3608 (3.7V → 5V pour MH-Z19C)
  - Modules TP4056 Type-C (charge)
- **Communication** :
  - Bus I2C partagé : GPIO 25 (SDA), GPIO 26 (SCL)
  - MAX30102 (0x57), MLX90614 (0x5A), ICM-20948 (0x68)
  - UART (MH-Z19C)
  - Analogique (GSR/EDA)
- **Support** : Balaclava textile avec intégration capteurs

### Software Stack
- **Firmware** : C/C++ (PlatformIO/Arduino)
- **Backend** : Node.js + Express / Python Flask
- **Base de données** : InfluxDB (time-series) ou PostgreSQL
- **Frontend** : Dashboard web (Angular/React)
- **Protocoles** : Wi-Fi (HTTP/MQTT) ou Bluetooth LE

---

## Structure du projet
```
balasense/
│
├── firmware/                 # Code embarqué ESP32
│   ├── src/
│   │   ├── main.cpp
│   │   ├── sensors/         # Tests capteurs
│   │   ├── connectivity/    # Wi-Fi / BLE
│   │   └── utils/           # Outils
│   ├── platformio.ini
│   └── README.md
│
├── backend/                  # API & serveur
│   ├── src/
│   └── README.md
│
├── frontend/                 # Dashboard
│   ├── src/
│   └── README.md
│
├── hardware/                 # Schémas électroniques
│   ├── schematics/
│   ├── pcb/
│   └── bom.csv
│
├── docs/                     # Documentation
│
├── data-analysis/            # Scripts d'analyse
│
├── .gitignore
├── LICENSE
└── README.md
```

---

## Démarrage rapide

### Prérequis
- PlatformIO IDE
- Git

### Installation firmware
```bash
# Clone du repository
git clone https://github.com/Rhizlene/balasense.git
cd balasense/firmware

# Upload vers ESP32
pio run --target upload
pio device monitor
```

---

## Données collectées en temps réel

| Donnée | Capteur | Fréquence | Unité | Objectif |
|--------|---------|-----------|-------|----------|
| Rythme cardiaque | MAX30102 | 25-100 Hz | BPM | Stress / Effort |
| HRV | MAX30102 | 1 Hz | ms | Récupération / Fatigue |
| SpO2 | MAX30102 | 1 Hz | % | Oxygénation |
| Température | MLX90614 | 1 Hz | °C | Surchauffe |
| Accélération | ICM-20948 | 50-100 Hz | g | G-force / Impacts |
| Vitesse angulaire | ICM-20948 | 50-100 Hz | °/s | Mouvements tête |
| CO₂ | MH-Z19C | 0.2 Hz | ppm | Respiration |
| GSR | EDA | 10 Hz | µS | Stress |

---

## Roadmap

### Phase 1 : Prototype (Q1 2026)
- [x] Cahier des charges
- [x] Commande matériel
- [x] Tests MAX30102 (64 BPM validé)
- [x] Tests MLX90614 (34°C validé)
- [ ] Tests ICM-20948
- [ ] Intégration multi-capteurs
- [ ] Transmission Wi-Fi basique
- [ ] Dashboard minimal

### Phase 2 : MVP (Q2 2026)
- [ ] Intégration physique balaclava
- [ ] Backend complet avec BDD
- [ ] Dashboard temps réel avancé
- [ ] Tests terrain (simulateur/karting)
- [ ] Documentation complète

### Phase 3 : Optimisation (Q3 2026)
- [ ] Miniaturisation PCB custom
- [ ] Algorithmes analyse avancée (ML)
- [ ] Détection fatigue automatique
- [ ] Mode dégradé & failsafe

---

## Métriques de succès MVP

| Métrique | Objectif | Statut |
|----------|----------|--------|
| Nombre capteurs | 7 | Validé |
| Autonomie batterie | ≥ 2h | 3h estimé |
| Latence transmission | < 500ms | À valider |
| Précision BPM | ±5 BPM | Validé (64 BPM) |
| Perte de données | < 1% | À valider |
| Poids total | < 150g | À mesurer |
| Confort pilote | ≥ 7/10 | À tester |

---

## Tests & Validation

### Capteurs validés

- **MAX30102** : Rythme cardiaque (64 BPM mesuré au repos)
- **MLX90614** : Température IR (34°C mesuré sur peau)
- **ICM-20948** : En cours de test
- **MH-Z19C** : Nécessite boost 5V (en attente breadboard)
- **GSR/EDA** : À tester

---

## Auteur

**Rhiz**  

GitHub : [github.com/Rhizlene]  
Projet personnel - 2026

---

## Vision

**BalaSense** a pour ambition de démocratiser le monitoring biométrique dans le sport automobile, en offrant aux pilotes amateurs et professionnels des données exploitables en temps réel pour optimiser performance et sécurité.

**"Sense the race, feel the data"**


*Dernière mise à jour : Janvier 2026*  
*Version : 0.1.0-alpha*  
*Statut : En développement actif*
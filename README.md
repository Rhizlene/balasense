# 🏎️ BalaSense - Smart Racing Biometric Monitor

> **Système de monitoring biométrique embarqué pour pilotes de sport automobile**

[![Status](https://img.shields.io/badge/status-in%20development-yellow)]()
[![Hardware](https://img.shields.io/badge/hardware-ESP32-blue)]()
[![Sensors](https://img.shields.io/badge/sensors-7%20capteurs-green)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

---

## 📋 Description

**BalaSense** est un projet de recherche et développement visant à créer une balaclava intelligente pour le monitoring en temps réel des données biométriques et physiologiques des pilotes de course automobile.

### Objectifs principaux
- 🛡️ **Sécurité** : Détection précoce des signes de fatigue ou stress excessif
- 📊 **Performance** : Analyse de l'état physiologique en corrélation avec la performance en piste
- 🔬 **Innovation** : Application IoT dans le domaine du motorsport de haut niveau

---

## 🎯 Fonctionnalités clés

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
- Dashboard web Angular

### Autonomie optimisée
- Batterie Li-Po 1000mAh
- Boost converter intelligent (3.7V → 5V)
- Autonomie cible : 3h minimum

---

## 🔧 Architecture technique

### Hardware
- **Microcontrôleur** : ESP32-S2-Saola-1
- **Alimentation** : 
  - Batterie Li-Po 3.7V (1000mAh)
  - Boost MT3608 (3.7V → 5V pour MH-Z19C)
  - Modules TP4056 Type-C (charge)
- **Communication** :
  - Bus I2C partagé (MAX30102, MLX90614, ICM-20948)
  - UART (MH-Z19C)
  - Analogique (GSR/EDA)
- **Support** : Balaclava textile avec intégration capteurs

### Software Stack
- **Firmware** : C/C++ (PlatformIO/Arduino)
- **Backend** : Node.js + Express / Python Flask
- **Base de données** : InfluxDB (time-series) ou PostgreSQL
- **Frontend** : Angular + Chart.js / Plotly
- **Protocoles** : Wi-Fi (HTTP/MQTT) ou Bluetooth LE

---

## 📁 Structure du projet

```
balasense/
│
├── firmware/                 # Code embarqué ESP32
│   ├── src/
│   │   ├── main.cpp
│   │   ├── sensors/         # Drivers capteurs
│   │   ├── connectivity/    # Wi-Fi / BLE
│   │   └── utils/           # Outils
│   ├── platformio.ini
│   └── README.md
│
├── backend/                  # API & serveur
│   ├── src/
│   ├── package.json
│   └── README.md
│
├── frontend/                 # Dashboard Angular
│   ├── src/
│   ├── angular.json
│   └── README.md
│
├── hardware/                 # Schémas électroniques
│   ├── schematics/
│   ├── pcb/
│   └── bom.csv
│
├── docs/                     # Documentation
│   ├── cahier-des-charges.md
│   ├── architecture.md
│   └── user-guide.md
│
├── data-analysis/            # Scripts d'analyse
│   └── notebooks/
│
├── .gitignore
├── LICENSE
└── README.md
```

---

## 🚀 Démarrage rapide

### Prérequis
- PlatformIO IDE (ou Arduino IDE)
- Node.js v18+ (pour backend)
- Angular CLI v17+ (pour frontend)
- Python 3.10+ (pour analyse données)

### Installation firmware

```bash
# Clone du repository
git clone https://github.com/[username]/balasense.git
cd balasense/firmware

# Upload vers ESP32
pio run --target upload
pio device monitor
```

### Lancement backend

```bash
cd backend
npm install
npm run dev
```

### Lancement dashboard

```bash
cd frontend
npm install
ng serve
```

---

## 📊 Données collectées en temps réel

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

## 🗓️ Roadmap

### ✅ Phase 1 : Prototype (Q1 2026)
- [x] Cahier des charges
- [x] Commande matériel
- [ ] Tests capteurs individuels
- [ ] Intégration multi-capteurs
- [ ] Transmission Wi-Fi basique
- [ ] Dashboard minimal

### 🔄 Phase 2 : MVP (Q2 2026)
- [ ] Intégration physique balaclava
- [ ] Backend complet avec BDD
- [ ] Dashboard temps réel avancé
- [ ] Tests terrain (simulateur/karting)
- [ ] Documentation complète

### 📅 Phase 3 : Optimisation (Q3 2026)
- [ ] Miniaturisation PCB custom
- [ ] Algorithmes analyse avancée (ML)
- [ ] Détection fatigue automatique
- [ ] Mode dégradé & failsafe
- [ ] Tests FIA (optionnel)

---

## 📈 Métriques de succès MVP

| Métrique | Objectif | Statut |
|----------|----------|--------|
| Nombre capteurs | 7 | ✅ Validé |
| Autonomie batterie | ≥ 2h | 🎯 3h estimé |
| Latence transmission | < 500ms | ⏳ À valider |
| Précision BPM | ±5 BPM | ⏳ À valider |
| Perte de données | < 1% | ⏳ À valider |
| Poids total | < 150g | ⏳ À mesurer |
| Confort pilote | ≥ 7/10 | ⏳ À tester |

---

## 🧪 Tests & Validation

### Tests unitaires
```bash
# Firmware
cd firmware
pio test

# Backend
cd backend
npm test

# Frontend
cd frontend
ng test
```

### Tests d'intégration
- Validation acquisition multi-capteurs
- Stress test transmission Wi-Fi
- Test autonomie batterie
- Tests en conditions réelles (chaleur, mouvement)

---

## 📖 Documentation

- [Cahier des charges](./docs/cahier-des-charges.md)
- [Architecture technique](./docs/architecture.md)
- [API Documentation](./docs/api-documentation.md)
- [Guide utilisateur](./docs/user-guide.md)
- [Guide de démarrage rapide](./docs/guide-demarrage-rapide.md)

---

## 🤝 Contribution

Les contributions sont les bienvenues ! 

1. Forker le projet
2. Créer une branche feature (`git checkout -b feature/AmazingFeature`)
3. Commiter les changements (`git commit -m 'Add AmazingFeature'`)
4. Pusher vers la branche (`git push origin feature/AmazingFeature`)
5. Ouvrir une Pull Request

---

## 📝 License

Ce projet est sous licence MIT - voir le fichier [LICENSE](LICENSE) pour plus de détails.

---

## 👤 Auteur

**Rhiz**
- Software & Data Engineer Apprentice @ ArcelorMittal
- MSc IoT & Engineering @ EPITECH Marseille
- Passionnée de F1 et technologies motorsport

📧 Contact : [contact]  
🔗 LinkedIn : [profil]  
💼 Portfolio : [site]

---

## 🙏 Remerciements

- EPITECH Marseille - Encadrement pédagogique
- ArcelorMittal France - Support technique
- Communauté ESP32 & Arduino
- SparkFun & Adafruit - Bibliothèques capteurs
- Écuries et pilotes testeurs

---

## 📈 Statistiques du projet

![GitHub last commit](https://img.shields.io/github/last-commit/[username]/balasense)
![GitHub issues](https://img.shields.io/github/issues/[username]/balasense)
![GitHub stars](https://img.shields.io/github/stars/[username]/balasense?style=social)

---

## 🏁 Vision

**BalaSense** a pour ambition de devenir la référence en monitoring biométrique pour le sport automobile, en offrant aux pilotes et équipes techniques des données exploitables en temps réel pour optimiser performance et sécurité.

**"Sense the race, feel the data"** 🏎️💓📊

---

**⚠️ Avertissement** : Ce projet est un prototype de recherche et développement. Il n'est pas certifié pour un usage professionnel en compétition réglementée (FIA). Toujours consulter un professionnel de santé pour l'interprétation de données biométriques.

---

*Dernière mise à jour : Janvier 2026*  
*Version : 0.1.0-alpha*  
*Statut : En développement actif*

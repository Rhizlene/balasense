# BalaSense - Smart Racing Biometric Monitor

> Embedded biometric monitoring system for motorsport drivers

[![Status](https://img.shields.io/badge/status-in%20development-yellow)]()
[![Hardware](https://img.shields.io/badge/hardware-ESP32-blue)]()
[![Sensors](https://img.shields.io/badge/sensors-2%2F6%20validated-orange)]()

---

## Description

**BalaSense** is a personal R&D project aimed at building a smart balaclava for real-time biometric and physiological monitoring of racing drivers.

### Main goals
- **Safety**: Early detection of fatigue and excessive stress
- **Performance**: Physiological state analysis correlated with on-track performance
- **Innovation**: IoT application in high-level motorsport

---

## Physical Architecture

The system is split into two wearable components:

```
┌─────────────────────────────────────┐
│             BALACLAVA               │
│  • ECG electrodes (MAX30003)        │
│  • EDA / GSR electrodes             │
│  • TMP117 (skin temperature)        │
│  • 3mm silicone tube (breathing)    │
└────────────────┬────────────────────┘
                 │ wiring
┌────────────────▼────────────────────┐
│            HANS BOX                 │
│  • ESP32 Dev Module                 │
│  • MAX30003 (ECG, SPI)              │
│  • SCD41  (CO₂ + temp + hum, I²C)  │
│  • SDP810 (respiratory flow, I²C)  │
│  • ICM-20948 (9-axis IMU, I²C)     │
│  • Li-Po 1200mAh battery (503759)   │
│  • TP4056 Type-C + BMS              │
│  • 5×7 cm perfboard                 │
└─────────────────────────────────────┘
```

**Breathing path:** 3mm silicone tube → Y-connector → SDP810 (flow) + SCD41 (CO₂)

---

## Sensors

### Validated ✅

| Sensor | Measurement | Bus | Address | Validated result |
|--------|------------|-----|---------|-----------------|
| **ICM-20948** | 9-axis IMU (accel / gyro / mag) | I²C | 0x68 | ~1.00 g at rest, rotation detected |
| **SCD41** | CO₂ + temperature + humidity | I²C | 0x62 | 485 ppm outdoors confirmed |

> **ICM-20948**: currently wired on GPIO25/26 — to be migrated to GPIO21/22 (shared bus) before final assembly.

### Received — pending test ⏳

| Sensor | Measurement | Bus | Notes |
|--------|------------|-----|-------|
| **MAX30003** | ECG (cardiac signal) | SPI | — |
| **GSR / EDA** | Skin conductance (stress) | Analog | — |
| **SDP810** | Respiratory flow | I²C | — |
| **TMP117** | Skin temperature | I²C | 0x48 |

---

## Technical Architecture

### Hardware

- **Microcontroller**: ESP32 Dev Module
- **Power**: Li-Po 3.7V 1200mAh (503759) + TP4056 Type-C + BMS
- **I²C bus**: GPIO21 (SDA) / GPIO22 (SCL) — shared bus for all I²C sensors
- **SPI**: MAX30003 (ECG)
- **Analog**: GSR/EDA
- **Mechanical support**: 5×7 cm perfboard (HANS box)

### I²C bus — addresses

| Address | Sensor |
|---------|--------|
| 0x62 | SCD41 |
| 0x68 | ICM-20948 |
| 0x48 | TMP117 |
| — | SDP810 (address TBC) |

### Software Stack

- **Firmware**: C/C++ (PlatformIO / Arduino framework)
- **Backend**: Node.js + Express / Python Flask (TBD)
- **Database**: InfluxDB time-series (TBD)
- **Frontend**: Web dashboard (TBD)
- **Protocols**: Wi-Fi (HTTP/MQTT) or Bluetooth LE

---

## Project Structure

```
balasense/
│
├── firmware/                 # ESP32 embedded code
│   ├── src/
│   │   ├── main.cpp
│   │   ├── sensors/         # Per-sensor test files
│   │   └── utils/
│   └── platformio.ini
│
├── doc/                      # Architecture documentation
│   ├── balasense-architecture_securite_pilote.odt
│   └── BALASENSE_architecture_capteurs.docx
│
├── hardware/                 # Schematics
│
├── .gitignore
└── README.md
```

---

## Quick Start

### Prerequisites
- PlatformIO IDE (VS Code)
- Git

### Flash firmware
```bash
git clone https://github.com/Rhizlene/balasense.git
cd balasense/firmware
pio run --target upload
pio device monitor
```

---

## Collected Data

| Data | Sensor | Bus | Rate | Unit |
|------|--------|-----|------|------|
| G-force / Acceleration | ICM-20948 | I²C | 50–100 Hz | g |
| Angular velocity | ICM-20948 | I²C | 50–100 Hz | °/s |
| Magnetic field | ICM-20948 | I²C | 10 Hz | µT |
| CO₂ | SCD41 | I²C | 0.2 Hz | ppm |
| Temperature + humidity | SCD41 | I²C | 0.2 Hz | °C / % |
| ECG | MAX30003 | SPI | — | mV |
| Skin conductance | GSR/EDA | Analog | 10 Hz | µS |
| Respiratory flow | SDP810 | I²C | — | L/min |
| Skin temperature | TMP117 | I²C | 1 Hz | °C |

---

## Roadmap

### Phase 1 — Sensor Validation (Q1–Q2 2026)
- [x] Full hardware architecture defined
- [x] Components ordered and received
- [x] ICM-20948 validated (1.00 g, rotation detected)
- [x] SCD41 validated (485 ppm outdoors)
- [ ] Migrate ICM-20948 to GPIO21/22
- [ ] Validate MAX30003 (ECG)
- [ ] Validate GSR/EDA
- [ ] Validate SDP810 (respiratory flow)
- [ ] Validate TMP117 (skin temperature)

### Phase 2 — Integration (Q2–Q3 2026)
- [ ] Merged multi-sensor firmware
- [ ] Soldering on 5×7 cm perfboard
- [ ] Physical integration: balaclava + HANS box
- [ ] Basic Wi-Fi / BLE transmission
- [ ] Minimal dashboard

### Phase 3 — Field MVP (Q3–Q4 2026)
- [ ] Field tests (simulator / karting)
- [ ] Backend + real-time database
- [ ] Analysis algorithms (fatigue, stress)
- [ ] Full documentation

### Phase 4 — Optimization (2027)
- [ ] Custom miniaturized PCB
- [ ] ML algorithms (automatic fatigue detection)
- [ ] Degraded mode & failsafe

---

## MVP Success Metrics

| Metric | Target | Status |
|--------|--------|--------|
| Sensors validated | 6/6 | 2/6 (33%) |
| Battery life | ≥ 2h | To measure |
| Transmission latency | < 500ms | To validate |
| CO₂ accuracy | ±50 ppm | Validated (485 ppm outdoors) |
| G-force accuracy | ±0.1 g | Validated (1.00 g) |
| Total weight | < 150g | To measure |
| Driver comfort | ≥ 7/10 | To test |

---

## PlatformIO Libraries

| Sensor | Library |
|--------|---------|
| ICM-20948 | `sparkfun/SparkFun 9DoF IMU Breakout - ICM 20948 @ 1.3.2` |
| SCD41 | `sensirion/Sensirion I2C SCD4x @ 0.4.0` + `Sensirion Core @ 0.7.3` |
| MAX30003 | TBD |
| SDP810 | TBD |
| TMP117 | TBD |

---

## Author

**Rhizlene**  
GitHub: [github.com/Rhizlene]  
Personal project — 2026

---

**"Sense the race, feel the data"**

*Last updated: April 2026*  
*Version: 0.2.0-alpha*  
*Status: Active development — 2/6 sensors validated*

# BalaSense - Smart Racing Biometric Monitor

> Embedded biometric monitoring system for motorsport drivers

[![Status](https://img.shields.io/badge/status-in%20development-yellow)]()
[![Hardware](https://img.shields.io/badge/hardware-ESP32-blue)]()
[![Sensors](https://img.shields.io/badge/sensors-3%2F6%20validated-orange)]()

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
│  • GSR module (EDA, Analog)         │
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
| **ICM-20948** | 9-axis IMU (accel / gyro / mag) | I²C | 0x68 | ~1.00 g at rest, rotation detected up to 306 dps |
| **SCD41** | CO₂ + temperature + humidity | I²C | 0x62 | 485 ppm outdoors, 2036 ppm confined — confinement effect confirmed |
| **GSR / EDA** | Skin conductance (stress) | Analog | GPIO32 | Baseline calibrated, stress delta detected — 100kΩ pull-up (1MΩ final) |

### Received — pending test ⏳

| Sensor | Measurement | Bus | Notes |
|--------|------------|-----|-------|
| **MAX30003** | ECG (cardiac signal) | SPI | — |
| **SDP810** | Respiratory flow | I²C | — |
| **TMP117** | Skin temperature | I²C | 0x48 |

---

## Technical Architecture

### Hardware

- **Microcontroller**: ESP32 Dev Module
- **Power**: Li-Po 3.7V 1200mAh (503759) + TP4056 Type-C + BMS
- **I²C bus**: GPIO21 (SDA) / GPIO22 (SCL) — shared bus, 400kHz
- **SPI**: MAX30003 (ECG)
- **Analog**: GSR/EDA — GPIO32 + 100kΩ pull-up to 3.3V (1MΩ in final version)
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
- **Backend**: Python FastAPI + MQTT (Mosquitto)
- **Database**: InfluxDB time-series (TBD)
- **Frontend**: React + Recharts — real-time WebSocket dashboard
- **Protocols**: Wi-Fi bursts (200ms) → MQTT → WebSocket

---

## Firmware Architecture

Multi-sensor non-blocking loop — all sensors run independently via `millis()`:

| Sensor | Rate | Notes |
|--------|------|-------|
| ICM-20948 | 100 Hz | Sustained alert filter: 80 dps / 2.5G over 50ms |
| GSR / EDA | 50 Hz | Delta-based stress detection, boot calibration |
| SCD41 | 0.2 Hz | CO₂ / temp / humidity |
| SDP810 | 25 Hz | Pending integration |
| TMP117 | 1 Hz | Pending integration |
| MAX30003 | 512 Hz | Pending integration |

IMU watchdog + auto-recovery on I²C disconnect.

---

## Project Structure

```
balasense/
│
├── firmware/                 # ESP32 embedded code
│   ├── src/
│   │   ├── main.cpp          # Multi-sensor non-blocking loop
│   │   ├── sensors/          # Per-sensor test files
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

> On first boot, the system waits 5 seconds for GSR electrodes to be placed before calibrating the baseline. Stay calm during calibration.

---

## Collected Data

| Data | Sensor | Bus | Rate | Unit |
|------|--------|-----|------|------|
| G-force / Acceleration | ICM-20948 | I²C | 100 Hz | g |
| Angular velocity | ICM-20948 | I²C | 100 Hz | °/s |
| Magnetic field | ICM-20948 | I²C | 100 Hz | µT |
| CO₂ | SCD41 | I²C | 0.2 Hz | ppm |
| Temperature + humidity | SCD41 | I²C | 0.2 Hz | °C / % |
| Skin conductance (EDA) | GSR | Analog | 50 Hz | raw / kΩ / delta |
| ECG | MAX30003 | SPI | 512 Hz | mV |
| Respiratory flow | SDP810 | I²C | 25 Hz | L/min |
| Skin temperature | TMP117 | I²C | 1 Hz | °C |

---

## Roadmap

### Phase 1 — Sensor Validation (Q1–Q2 2026)
- [x] Full hardware architecture defined
- [x] Components ordered and received
- [x] ICM-20948 validated — 1.00 g at rest, 306 dps peak detected
- [x] SCD41 validated — 485 ppm outdoors, confinement effect confirmed
- [x] GSR/EDA validated — baseline calibration, stress delta working
- [x] Multi-sensor non-blocking firmware (SCD41 + ICM-20948 + GSR)
- [x] IMU watchdog + auto-recovery
- [x] IMU sustained alert filter (anti-vibration)
- [ ] Validate SDP810 (respiratory flow)
- [ ] Validate TMP117 (skin temperature)
- [ ] Validate MAX30003 (ECG)

### Phase 2 — Integration (Q2–Q3 2026)
- [ ] Full 6-sensor merged firmware
- [ ] MQTT transmission via WiFi bursts
- [ ] Soldering on 5×7 cm perfboard
- [ ] Physical integration: balaclava + HANS box
- [ ] Minimal real-time dashboard (React + WebSocket)

### Phase 3 — Field MVP (Q3–Q4 2026)
- [ ] Field tests (simulator / karting)
- [ ] Python backend — signal processing + derived indicators
- [ ] Stress Index, Fatigue Index, G-load accumulation
- [ ] Full documentation

### Phase 4 — Optimization (2027)
- [ ] Custom miniaturized PCB
- [ ] ML algorithms (automatic fatigue detection)
- [ ] Degraded mode & failsafe

---

## MVP Success Metrics

| Metric | Target | Status |
|--------|--------|--------|
| Sensors validated | 6/6 | 3/6 (50%) |
| Battery life | ≥ 2h | To measure |
| Transmission latency | < 500ms | To validate |
| CO₂ accuracy | ±50 ppm | ✅ Validated (485 ppm outdoors) |
| G-force accuracy | ±0.1 g | ✅ Validated (1.00 g at rest) |
| GSR stress detection | delta > 200 | ✅ Validated |
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
*Version: 0.3.0-alpha*  
*Status: Active development — 3/6 sensors validated*
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
│  • EDA / GSR electrodes (CJMCU-6701)│
│  • TMP117 (skin temperature)        │
│  • 3mm silicone tube (breathing)    │
└────────────────┬────────────────────┘
                 │ silicone wires 28-30 AWG + strain relief
┌────────────────▼────────────────────┐
│            HANS BOX                 │
│  • ESP32 Dev Module                 │
│  • MAX30003 / CJMCU-30003 (ECG, SPI)│
│  • SCD41  (CO₂ + temp + hum, I²C)  │
│  • SDP810-500Pa (resp. flow, I²C)  │
│  • ICM-20948 (9-axis IMU, I²C)     │
│  • CJMCU-6701 GSR module (Analog)   │
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
| **ICM-20948** | 9-axis IMU (accel / gyro / mag) | I²C | 0x68 | ~1.00 g at rest, 306 dps peak — AD0 pinned to GND (stable) |
| **SCD41** | CO₂ + temperature + humidity | I²C | 0x62 | 485 ppm outdoors → 2036 ppm confined — confinement effect confirmed |
| **CJMCU-6701 GSR** | Skin conductance (EDA/stress) | Analog | GPIO32 | Baseline calibrated, stress delta detected — module with integrated conditioning circuit, electrodes soldered |

### Received — pending test ⏳

| Sensor | Measurement | Bus | Notes |
|--------|------------|-----|-------|
| **CJMCU-30003 (MAX30003)** | ECG (cardiac signal) | SPI | +5V supply, 3.3V logic — level shifter TBC |
| **SDP810-500Pa** | Respiratory flow | I²C | 0x25 — next to integrate |
| **TMP117** | Skin temperature | I²C | 0x48 |

---

## Technical Architecture

### Hardware

- **Microcontroller**: ESP32 Dev Module
- **Power**: Li-Po 3.7V 1200mAh (503759) + TP4056 Type-C + BMS
- **I²C bus**: GPIO21 (SDA) / GPIO22 (SCL) — shared bus, 400kHz — AD0 pinned on ICM-20948
- **SPI**: CJMCU-30003 (MAX30003 ECG) — +5V supply
- **Analog**: CJMCU-6701 GSR — GPIO32, +5V supply, integrated signal conditioning
- **Mechanical support**: 5×7 cm perfboard (HANS box)

### I²C bus — addresses

| Address | Sensor |
|---------|--------|
| 0x62 | SCD41 |
| 0x68 | ICM-20948 (AD0 → GND) |
| 0x25 | SDP810-500Pa |
| 0x48 | TMP117 |

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
| ICM-20948 | 100 Hz | Sustained alert filter: 80 dps / 2.5G over 50ms — AD0 fix applied |
| CJMCU-6701 GSR | 50 Hz | Delta-based stress detection, 5s boot calibration with electrodes on skin |
| SCD41 | 0.2 Hz | CO₂ / temp / humidity |
| SDP810 | 25 Hz | Pending integration |
| TMP117 | 1 Hz | Pending integration |
| MAX30003 | 512 Hz | Pending integration |

IMU watchdog + auto-recovery on I²C disconnect.

---

## Known Hardware Notes

- **ICM-20948 AD0**: must be wired to GND to lock I²C address at 0x68. Floating AD0 causes random address switching (0x68 ↔ 0x69) and I²C bus corruption.
- **CJMCU-30003**: marked +5V — verify logic level compatibility with ESP32 3.3V before SPI wiring.
- **SDP810-500Pa**: bare sensor (no breakout board) — direct pad soldering required.

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

> On first boot, the system waits 5 seconds for GSR electrodes to be placed on skin before calibrating the baseline. Stay calm during calibration.

---

## Collected Data

| Data | Sensor | Bus | Rate | Unit |
|------|--------|-----|------|------|
| G-force / Acceleration (X/Y/Z) | ICM-20948 | I²C | 100 Hz | g |
| Angular velocity (X/Y/Z) | ICM-20948 | I²C | 100 Hz | °/s |
| Magnetic field (X/Y/Z) | ICM-20948 | I²C | 100 Hz | µT |
| CO₂ | SCD41 | I²C | 0.2 Hz | ppm |
| Ambient temperature + humidity | SCD41 | I²C | 0.2 Hz | °C / % |
| Skin conductance (EDA) | CJMCU-6701 | Analog | 50 Hz | raw / MΩ / delta |
| ECG | MAX30003 | SPI | 512 Hz | mV |
| Respiratory flow | SDP810 | I²C | 25 Hz | L/min |
| Skin temperature | TMP117 | I²C | 1 Hz | °C |

---

## Roadmap

### Phase 1 — Sensor Validation (Q1–Q2 2026)
- [x] Full hardware architecture defined
- [x] Components ordered and received
- [x] ICM-20948 validated — 1.00 g at rest, 306 dps peak, AD0 fix applied
- [x] SCD41 validated — 485 ppm outdoors, confinement effect confirmed
- [x] GSR/EDA validated — CJMCU-6701 module, electrodes soldered, stress delta working
- [x] Multi-sensor non-blocking firmware (SCD41 + ICM-20948 + GSR)
- [x] IMU watchdog + auto-recovery
- [x] IMU sustained alert filter (anti-vibration, 50ms debounce)
- [x] I²C bus stability — AD0 pin fix on ICM-20948
- [ ] Validate SDP810-500Pa (respiratory flow)
- [ ] Validate TMP117 (skin temperature)
- [ ] Validate MAX30003 / CJMCU-30003 (ECG)

### Phase 2 — Integration (Q2–Q3 2026)
- [ ] Full 6-sensor merged firmware
- [ ] MQTT transmission via WiFi bursts (200ms)
- [ ] Soldering on 5×7 cm perfboard
- [ ] Physical integration: balaclava + HANS box
- [ ] Minimal real-time dashboard (React + WebSocket)

### Phase 3 — Field MVP (Q3–Q4 2026)
- [ ] Field tests (simulator / karting)
- [ ] Python backend — signal processing + derived indicators
- [ ] Stress Index (ECG + EDA), Fatigue Index (HRV + Temp + CO₂), G-load accumulation
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
| I²C bus stability | No random disconnect | ✅ Fixed (AD0 → GND) |
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
*Version: 0.3.1-alpha*  
*Status: Active development — 3/6 sensors validated*
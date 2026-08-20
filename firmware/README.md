# firmware/

ESP32 firmware for BalaSense — PlatformIO / Arduino framework. This is the only fully implemented part of the project; everything else in the repo is downstream of the data it produces.

## Structure

```
src/
├── main.cpp        # setup()/loop() — sensor init order, non-blocking scheduling
├── config.h         # pins, I2C addresses, WiFi/MQTT credentials, timing intervals, raw/derived data structs
├── globals.h/.cpp   # extern instances shared across modules (sensor objects, data structs, timing state)
├── telemetry.h/.cpp # CSV/serial summary output + status-level helpers (co2Level, gLevel, gsrLevel)
├── wifi_mqtt.h/.cpp # WiFi connect + MQTT publish (non-blocking, reconnect handling)
├── ecg.h/.cpp        # MAX30003 (SPI) — R-peak detection, HR/RMSSD/pNN50
├── imu.h/.cpp        # ICM-20948 (I2C) — pitch/roll, G-force, jerk, cervical cumulative rotation
├── gsr.h/.cpp        # GSR/EDA (analog GPIO32) — tonic/phasic decomposition, SCR detection
├── sdp810.h/.cpp     # SDP810 (I2C) — respiratory flow, breathing rate, I:E ratio, apnea
├── scd41.h/.cpp      # SCD41 (I2C) — CO2/temp/humidity, slope, absolute humidity, dew point
└── tmp117.h/.cpp     # TMP117 (I2C) — skin temperature, thermal drift rate
```

`platformio.ini` pins the `esp32dev` board/env and all library dependencies (SparkFun ICM-20948, Sensirion SCD4x/Core, Adafruit TMP117 stack, ArduinoJson, PubSubClient).

## Critical init order (`setup()` in `main.cpp`)

SPI → Wire → LEDC/FCLK → **sensors** → WiFi/MQTT. WiFi must be initialized *after* all sensor `init*()` calls — doing it earlier causes I2C bus interference and a Guru Meditation crash. This was a real bug hit during development; don't reorder without re-testing on hardware.

## Loop model

Fully non-blocking, `millis()`-gated per sensor. Each sensor has its own read interval (IMU 100Hz, GSR 50Hz, SDP810 25Hz, ECG 20Hz, TMP117 1Hz, SCD41 0.2Hz) plus periodic summary/CSV/MQTT publish ticks. See `config.h` for exact `*_INTERVAL_MS` values before changing timing.

## Output

- Serial CSV (`DATA` line, 1Hz, 28 columns) — capture with `pio device monitor | findstr /B "DATA" > session.csv`
- MQTT: `balasense/summary` (1Hz JSON, all derived metrics) and `balasense/ecg` (20Hz JSON waveform)

WiFi/MQTT are best-effort and non-blocking — the firmware runs standalone over serial if there's no network.

## Known caveats

- WiFi SSID/password and MQTT broker IP are hardcoded in `config.h` (currently a phone hotspot + laptop IP) — this file is tracked in git, so don't commit real credentials if the repo ever goes public.
- MAX30003 requires GPIO26 wired to FCLK (no onboard oscillator on this module) and ~45s electrode contact time for a stable ECG signal.
- RMSSD/pNN50 accuracy depends on stable electrode contact (conductive gel recommended) — flagged as pending in the root README.

## Build

```bash
cd firmware
pio run --target upload
pio device monitor
```

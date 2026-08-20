# backend/

Planned role: subscribe to the firmware's MQTT topics, compute session-level derived indices that don't belong on the ESP32, and expose the data to the frontend.

## Planned stack

- Python + FastAPI
- MQTT subscriber (Mosquitto broker, topics `balasense/summary` 1Hz and `balasense/ecg` 20Hz — see `firmware/README.md`)
- WebSocket bridge to the frontend for live data
- InfluxDB (time-series storage) — later phase

## Planned responsibilities

- MQTT → WebSocket bridge (real-time passthrough to frontend)
- Session-level indices not computed on-device: Stress Index (HR + GSR phasic), Fatigue Index (RMSSD + CO₂ + skin temp), cardio-respiratory coherence (ECG × SDP810 correlation)
- Session storage/export for offline analysis (feeds `data-analysis/`)

`src/` is currently an empty placeholder directory — nothing to document until code lands.

# frontend/

Planned role: real-time dashboard for the driver/pit crew, consuming the backend's WebSocket feed.

## Planned stack 

- React + Recharts
- WebSocket client (connects to the FastAPI backend in `backend/`)

## Planned responsibilities

- Live view of the 1Hz summary metrics (HR, GSR, CO₂, IMU, breathing, thermal) from `balasense/summary`
- Live ECG waveform view from `balasense/ecg` (20Hz)
- Surfacing derived alert levels already computed in firmware (`co2Level`, `gLevel`, `gsrLevel` — see `firmware/src/telemetry.h`) and, later, backend-computed Stress/Fatigue indices

`src/` is currently an empty placeholder directory — nothing to document until code lands.

# data-analysis/


Planned role: offline exploration and validation of recorded sessions — the place to iterate on signal-processing logic (R-peak detection tuning, HRV metrics, stress/fatigue index formulas) before anything gets ported back into firmware or backend.

## Inputs

Session CSVs captured from the firmware's serial `DATA` output (see `firmware/README.md` for the capture command and `firmware/src/telemetry.h` for the 28-column schema: ECG/HRV, breathing, GSR, thermal, CO₂, IMU).

# docs/

Reference documents and hardware assets for BalaSense

## Contents

- `datasheets/` — component datasheets: `bq25895.pdf` (battery charger IC), `CJMCU-6701.pdf` (GSR module), `icm-20948-v1.5.pdf` (IMU), `max30003.pdf` (ECG AFE), `POLOLU-2595.pdf` (voltage regulator), `Sensirion_Differential_Pressure_Datasheet_SDP8xx_Digital.pdf` (SDP810 flow sensor), `Sensirion_SCD4x_Datasheet.pdf` (CO₂ sensor), `tmp117-high-accuracy-i2c-temperature-monitor.pdf` (skin temp sensor)
- `schem/` — circuit schematic (`circuit_bala.svg`) and rendered image (`cuicuit_bala.png`)
- `branding/` — project logo assets
- `BalaSense_Circuit_Composants.pdf` — full component/wiring reference
- `BalaSense_Power_Strategy.pdf` — battery/power management strategy (LiPo, TP4056, BMS, bq25895)
- `protocole_urgence_lipo.pdf` — LiPo battery emergency safety protocol
- `assets/`, `tests-reports/` — currently empty; reserved for supplementary images and test/validation reports (e.g. sensor validation logs referenced in the root README's Phase 1 checklist)

For the sensor list, pin mapping, and I2C addresses actually used in firmware, see `firmware/README.md` and `firmware/src/config.h` — treat those as the source of truth over any wiring notes here if they ever diverge.

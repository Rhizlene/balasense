#include "tmp117.h"
#include "globals.h"

void initTmp117() {
  if (!tmp117.begin()) { Serial.println("[TMP117] ERROR"); return; }
  Serial.println("[TMP117] OK — 1Hz");
}

void processTmp117Sample() {
  if (!tmp117Data.valid) return;
  if (tmp117Derived.temp_boot < -900) {
    tmp117Derived.temp_boot = tmp117Derived.temp_prev = tmp117Data.temp;
    tmp117Derived.ts_prev = tmp117Data.ts; return;
  }
  tmp117Derived.delta_boot = tmp117Data.temp - tmp117Derived.temp_boot;
  float dt = (tmp117Data.ts - tmp117Derived.ts_prev) / 60000.0f;
  if (dt > 0) tmp117Derived.dT_dt = (tmp117Data.temp - tmp117Derived.temp_prev) / dt;
  tmp117Derived.temp_prev = tmp117Data.temp; tmp117Derived.ts_prev = tmp117Data.ts;
}

void readTmp117() {
  sensors_event_t t; tmp117.getEvent(&t);
  tmp117Data.temp=t.temperature; tmp117Data.ts=millis(); tmp117Data.valid=true;
  processTmp117Sample();
  if (tmp117Data.temp > 38.5f)
    Serial.printf("[TMP117] ⚠ HIGH SKIN TEMP t=%lums | %.2fC (+%.3fC/min)\n",
                  tmp117Data.ts, tmp117Data.temp, tmp117Derived.dT_dt);
}

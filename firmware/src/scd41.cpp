#include "scd41.h"
#include "globals.h"
#include "telemetry.h"

void initScd41() {
  scd4x.begin(Wire); scd4x.stopPeriodicMeasurement(); delay(500);
  if (scd4x.startPeriodicMeasurement()) { Serial.println("[SCD41] ERROR"); while(1); }
  Serial.println("[SCD41] OK — 0.2Hz");
}

void processScd41Sample() {
  if (!scd41.valid) return;
  if (scd41Derived.ts_prev > 0) {
    float dt = (scd41.ts - scd41Derived.ts_prev) / 60000.0f;
    if (dt > 0) scd41Derived.co2_slope = ((float)scd41.co2 - scd41Derived.co2_prev) / dt;
  }
  scd41Derived.co2_prev = scd41.co2; scd41Derived.ts_prev = scd41.ts;
  float T=scd41.temp, RH=scd41.humidity;
  float es = 6.112f*expf(17.67f*T/(T+243.5f));
  scd41Derived.abs_humidity = (es*RH*2.1674f)/(273.15f+T);
  float g = logf(RH/100)+17.67f*T/(243.5f+T);
  scd41Derived.dew_point = 243.5f*g/(17.67f-g);
}

void readScd41() {
  bool ready=false; scd4x.getDataReadyFlag(ready); if (!ready) return;
  if (scd4x.readMeasurement(scd41.co2, scd41.temp, scd41.humidity)) return;
  scd41.ts=millis(); scd41.valid=true; processScd41Sample();
  if (DEBUG_SCD41)
    Serial.printf("\n[SCD41] t=%lums | CO2: %u ppm [%s] | Temp: %.1fC | Hum: %.1f%% | AH:%.2f g/m³ | DP:%.1fC | slope:%.1f ppm/min\n\n",
                  scd41.ts,scd41.co2,co2Level(scd41.co2),scd41.temp,scd41.humidity,
                  scd41Derived.abs_humidity,scd41Derived.dew_point,scd41Derived.co2_slope);
}

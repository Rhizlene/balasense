#include "gsr.h"
#include "globals.h"

void initGsr() {
  pinMode(GSR_PIN,INPUT); analogReadResolution(12); analogSetAttenuation(ADC_11db);
  Serial.println("[GSR]   Place electrodes on skin...");
  Serial.println("[GSR]   Calibrating baseline in 5s — stay calm");
  delay(5000);
  long sum=0;
  for (int i=0; i<100; i++) { sum+=analogRead(GSR_PIN); delay(10); }
  gsrData.baseline=sum/100.0f; gsrData.calibrated=true;
  Serial.printf("[GSR]   Calibrating... baseline=%.0f\n[GSR]   OK — 50Hz\n", gsrData.baseline);
}

void processGsrSample(int raw, float voltage, uint32_t ts) {
  if (!gsrDerived.initialized) {
    gsrDerived.tonic_raw = raw;
    gsrDerived.scr_window_ts = ts;
    gsrDerived.initialized = true;
  }
  gsrDerived.tonic_raw = 0.002f*raw + 0.998f*gsrDerived.tonic_raw;
  if (voltage > 0.01f) {
    float r = 1000.0f*(3.3f/voltage-1.0f);
    gsrDerived.conductance_us = (r>1) ? 1e6f/r : 0;
    float tv = gsrDerived.tonic_raw*(3.3f/4095.0f);
    float tr = (tv>0.01f) ? 1000.0f*(3.3f/tv-1.0f) : 0;
    gsrDerived.tonic_us = (tr>1) ? 1e6f/tr : 0;
    gsrDerived.phasic = gsrDerived.conductance_us - gsrDerived.tonic_us;
  }
  bool above = gsrDerived.phasic > 30.0f;
  if (above && !gsrDerived.prev_above_scr) gsrDerived.scr_window_cnt++;
  gsrDerived.prev_above_scr = above;
  if (ts - gsrDerived.scr_window_ts >= 60000) {
    gsrDerived.scr_per_min = (uint8_t)min(255.0f, gsrDerived.scr_window_cnt);
    gsrDerived.scr_window_cnt = 0;
    gsrDerived.scr_window_ts = ts;
  }
}

void readGsr() {
  gsrData.raw     = analogRead(GSR_PIN);
  gsrData.voltage = gsrData.raw*(3.3f/4095.0f);
  if (gsrData.voltage > 0.01f) gsrData.resistance = 1000.0f*(3.3f/gsrData.voltage-1.0f);
  gsrData.delta = gsrData.raw - gsrData.baseline;
  gsrData.ts=millis(); gsrData.valid=true;
  processGsrSample(gsrData.raw, gsrData.voltage, gsrData.ts);
  if (gsrData.delta > 500)
    Serial.printf("[GSR]  ⚠ STRESS SPIKE t=%lums | raw=%d delta=%+.0f | cond=%.4f µS | phasic=%.4f µS\n",
                  gsrData.ts, gsrData.raw, gsrData.delta, gsrDerived.conductance_us, gsrDerived.phasic);
}

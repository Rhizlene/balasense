#include "telemetry.h"
#include "globals.h"

void printCsvHeader() {
  Serial.println("# BALASENSE — capture: pio device monitor | findstr /B DATA > session.csv");
  Serial.println("ts_ms,hr_bpm,rmssd_ms,pnn50_pct,rr_last_ms,br_bpm,ie_ratio,breath_amp_pa,apnea,gsr_cond_us,gsr_tonic_us,gsr_phasic_us,scr_per_min,skin_temp_c,tmp_delta_boot_c,tmp_dtdt_c_min,co2_ppm,co2_slope_ppm_min,abs_hum_gm3,dew_point_c,g_total,g_lat,g_long,jerk_g_s,pitch_deg,roll_deg,rot_dps,cervical_cumul_deg");
}

void printCsvLine() {
  Serial.printf("DATA,%lu,%.1f,%.1f,%.1f,%u,%.1f,%.2f,%.1f,%d,%.4f,%.4f,%.4f,%u,%.2f,%.3f,%.4f,%u,%.1f,%.2f,%.1f,%.3f,%.3f,%.3f,%.2f,%.1f,%.1f,%.1f,%.1f\n",
    millis(),
    ecgDerived.hr_bpm, ecgDerived.rmssd, ecgDerived.pnn50, ecgDerived.rr_last,
    breathDerived.br_bpm, breathDerived.ie_ratio, breathDerived.amplitude, (int)breathDerived.apnea,
    gsrDerived.conductance_us, gsrDerived.tonic_us, gsrDerived.phasic, gsrDerived.scr_per_min,
    tmp117Data.temp, tmp117Derived.delta_boot, tmp117Derived.dT_dt,
    scd41.co2, scd41Derived.co2_slope, scd41Derived.abs_humidity, scd41Derived.dew_point,
    imuData.totalG, imuDerived.g_lat, imuDerived.g_long, imuDerived.jerk,
    imuDerived.pitch, imuDerived.roll, imuData.totalRot, imuDerived.cervical_cumul);
}

void printSummary() {
  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
  Serial.printf("t=%lums | WiFi:%s | MQTT:%s\n", millis(), wifiOk?"OK":"OFF", mqttOk?"OK":"OFF");
  if (imuOk && imuData.valid)
    Serial.printf("  IMU    | G:%.2f[%s] lat:%.2f long:%.2f | rot:%.1f | peak G:%.2f | pitch:%.1f° roll:%.1f° | jerk:%.1f g/s | cerv:%.0f°\n",
                  imuData.totalG, gLevel(imuData.totalG), imuDerived.g_lat, imuDerived.g_long,
                  imuData.totalRot, imuData.peakG, imuDerived.pitch, imuDerived.roll,
                  imuDerived.jerk, imuDerived.cervical_cumul);
  else Serial.println("  IMU    | [DISCONNECTED]");
  if (scd41.valid)
    Serial.printf("  CO2    | %u ppm [%s] | %.1fC | %.1f%% | AH:%.2f g/m³ | DP:%.1fC | slope:%.1f ppm/min\n",
                  scd41.co2, co2Level(scd41.co2), scd41.temp, scd41.humidity,
                  scd41Derived.abs_humidity, scd41Derived.dew_point, scd41Derived.co2_slope);
  else Serial.println("  CO2    | [NO DATA YET]");
  if (gsrData.valid)
    Serial.printf("  GSR    | raw=%d delta=%+.0f [%s] | cond=%.4f µS | tonic=%.4f | phasic=%.4f | SCR=%u/min\n",
                  gsrData.raw, gsrData.delta, gsrLevel(gsrData.delta),
                  gsrDerived.conductance_us, gsrDerived.tonic_us, gsrDerived.phasic, gsrDerived.scr_per_min);
  else Serial.println("  GSR    | [NO DATA YET]");
  if (sdp810.valid)
    Serial.printf("  SDP810 | P:%.2f Pa | T:%.1fC | BrPM:%.1f | I:E=%.2f | amp:%.1f Pa%s\n",
                  sdp810.pressure, sdp810.temp, breathDerived.br_bpm, breathDerived.ie_ratio,
                  breathDerived.amplitude, breathDerived.apnea?" ⚠ APNEA":"");
  else Serial.println("  SDP810 | [NO DATA YET]");
  if (tmp117Data.valid)
    Serial.printf("  TMP117 | %.2fC | +%.3fC vs boot | %.4fC/min\n",
                  tmp117Data.temp, tmp117Derived.delta_boot, tmp117Derived.dT_dt);
  else Serial.println("  TMP117 | [NO DATA YET]");
  if (ecgOk && ecgData.valid) {
    if (ecgDerived.valid)
      Serial.printf("  ECG    | raw=%ld mv=%.3f | HR:%.1f BPM | RR:%u ms | RMSSD:%.1f ms | pNN50:%.1f%%\n",
                    ecgData.raw, ecgData.mv, ecgDerived.hr_bpm, ecgDerived.rr_last, ecgDerived.rmssd, ecgDerived.pnn50);
    else
      Serial.printf("  ECG    | raw=%ld mv=%.3f | [accumulating RR — need 4+ beats]\n", ecgData.raw, ecgData.mv);
  } else Serial.println("  ECG    | [NO DATA YET]");
  imuData.peakG=0; imuData.peakRot=0;
  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
}

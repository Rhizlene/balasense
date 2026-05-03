#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <SensirionI2CScd4x.h>
#include "ICM_20948.h"
#include <Adafruit_TMP117.h>
#include <Adafruit_Sensor.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

// ── I2C ─────────────────────────────────────────────
#define I2C_SDA 21
#define I2C_SCL 22

// ── GSR ─────────────────────────────────────────────
#define GSR_PIN 32

// ── SDP810 ───────────────────────────────────────────
#define SDP810_ADDR         0x25
#define SDP810_CMD_START    0x3603
#define SDP810_CMD_STOP     0x3FF9
#define SDP810_SCALE_FACTOR 60.0f

// ── MAX30003 ─────────────────────────────────────────
#define MAX30003_CS    15
#define MAX30003_SCK   14
#define MAX30003_MOSI  25
#define MAX30003_MISO  13
#define MAX30003_FCLK  26

// ── Debug flags ──────────────────────────────────────
#define DEBUG_IMU   false
#define DEBUG_SCD41 true

// ── Sensor instances ─────────────────────────────────
SensirionI2CScd4x scd4x;
ICM_20948_I2C     imu;
Adafruit_TMP117   tmp117;

// ── Timing ───────────────────────────────────────────
const uint32_t SCD41_INTERVAL_MS  = 5000;
const uint32_t IMU_INTERVAL_MS    = 10;
const uint32_t GSR_INTERVAL_MS    = 20;
const uint32_t SDP810_INTERVAL_MS = 40;
const uint32_t TMP117_INTERVAL_MS = 1000;
const uint32_t ECG_INTERVAL_MS    = 50;
const uint32_t SUMMARY_INTERVAL   = 2000;
const uint32_t CSV_INTERVAL_MS    = 1000;
const uint32_t IMU_TIMEOUT_MS     = 10000;

uint32_t lastScd41Read  = 0;
uint32_t lastImuRead    = 0;
uint32_t lastGsrRead    = 0;
uint32_t lastSdp810Read = 0;
uint32_t lastTmp117Read = 0;
uint32_t lastEcgRead    = 0;
uint32_t lastSummary    = 0;
uint32_t lastCsv        = 0;
uint32_t lastImuPrint   = 0;
uint32_t lastImuSuccess = 0;

// ── Sensor health ────────────────────────────────────
bool imuOk = false;
bool ecgOk = false;

// ── IMU alert filter ─────────────────────────────────
const float    IMU_ROT_THRESHOLD  = 80.0f;
const float    IMU_G_THRESHOLD    = 2.5f;
const uint32_t IMU_ALERT_DURATION = 50;

uint32_t imuAlertStart = 0;
bool     imuInAlert    = false;

// ════════════════════════════════════════════════════
// Raw data structs
// ════════════════════════════════════════════════════
struct Scd41Data {
  uint16_t co2      = 0;
  float    temp     = 0.0f;
  float    humidity = 0.0f;
  uint32_t ts       = 0;
  bool     valid    = false;
} scd41;

struct ImuData {
  float accX=0, accY=0, accZ=0;
  float gyrX=0, gyrY=0, gyrZ=0;
  float magX=0, magY=0, magZ=0;
  float totalG=0, totalRot=0;
  float peakG=0, peakRot=0;
  uint32_t ts=0;
  bool valid=false;
} imuData;

struct GsrData {
  int      raw=0;
  float    voltage=0.0f;
  float    resistance=0.0f;
  float    baseline=0.0f;
  float    delta=0.0f;
  uint32_t ts=0;
  bool     valid=false;
  bool     calibrated=false;
} gsrData;

struct Sdp810Data {
  float    pressure=0.0f;
  float    temp=0.0f;
  float    flowRate=0.0f;
  uint32_t ts=0;
  bool     valid=false;
} sdp810;

struct Tmp117Data {
  float    temp=0.0f;
  uint32_t ts=0;
  bool     valid=false;
} tmp117Data;

struct EcgData {
  int32_t  raw=0;
  float    mv=0.0f;
  uint32_t ts=0;
  bool     valid=false;
} ecgData;

// ════════════════════════════════════════════════════
// Derived metric structs
// ════════════════════════════════════════════════════

// ECG — R-peak detection + HRV
struct EcgDerived {
  uint16_t rr_buf[20]   = {};
  uint8_t  rr_idx       = 0;
  uint8_t  rr_count     = 0;
  uint32_t last_peak_ts = 0;
  uint32_t max_decay_ts = 0;
  float    running_max  = 0.1f;  // adaptive peak envelope
  bool     prev_above   = false;
  // Computed metrics
  float    hr_bpm  = 0.0f;
  uint16_t rr_last = 0;
  float    rmssd   = 0.0f;  // ms — parasympathetic HRV index
  float    pnn50   = 0.0f;  // % — proportion of successive RR diffs > 50ms
  bool     valid   = false;
} ecgDerived;

// Breathing — zero-crossing analysis on SDP810
struct BreathDerived {
  bool     prev_positive  = false;
  uint32_t last_inhale_ts = 0;
  uint32_t last_exhale_ts = 0;
  uint32_t inhale_dur     = 0;  // ms
  uint32_t exhale_dur     = 0;  // ms
  uint32_t br_intervals[8]= {};
  uint8_t  br_idx         = 0;
  uint8_t  br_count       = 0;
  float    br_bpm         = 0.0f;
  float    ie_ratio       = 1.0f;  // inspiratory/expiratory time ratio
  float    amplitude      = 0.0f;  // Pa, peak inhalation pressure
  float    peak_inhale    = 0.0f;
  bool     apnea          = false;
} breathDerived;

// GSR — tonic/phasic EDA decomposition + conductance in µS
struct GsrDerived {
  float   tonic_raw      = 0.0f;
  float   conductance_us = 0.0f;  // skin conductance in µS (standard EDA unit)
  float   tonic_us       = 0.0f;  // slow component (baseline EDA)
  float   phasic         = 0.0f;  // fast component = conductance - tonic (µS)
  bool    initialized    = false;
  float   scr_window_cnt = 0.0f;  // SCR events accumulator in current 60s window
  uint8_t scr_per_min    = 0;     // skin conductance responses per minute
  uint32_t scr_window_ts = 0;
  bool    prev_above_scr = false;
} gsrDerived;

// IMU — angles + G-load decomposition + cervical dynamics
struct ImuDerived {
  float pitch          = 0.0f;  // degrees (nose up=positive)
  float roll           = 0.0f;  // degrees (right tilt=positive)
  float g_lat          = 0.0f;  // lateral G (accY) — cornering load
  float g_long         = 0.0f;  // longitudinal G (accX) — braking/acceleration
  float jerk           = 0.0f;  // g/s — rate of change of total G
  float g_prev         = 0.0f;
  float cervical_cumul = 0.0f;  // integrated |gyrZ| — cumulative neck rotation (°)
  bool  initialized    = false;
} imuDerived;

// SCD41 — CO₂ trend + psychrometrics
struct Scd41Derived {
  float    co2_slope    = 0.0f;  // ppm/min — ventilation efficiency indicator
  float    abs_humidity = 0.0f;  // g/m³ — absolute humidity (breath moisture load)
  float    dew_point    = 0.0f;  // °C
  uint16_t co2_prev     = 0;
  uint32_t ts_prev      = 0;
} scd41Derived;

// TMP117 — thermal load monitoring
struct Tmp117Derived {
  float    dT_dt      = 0.0f;    // °C/min — rate of skin temperature change
  float    delta_boot = 0.0f;    // °C above baseline at boot
  float    temp_boot  = -999.0f;
  float    temp_prev  = 0.0f;
  uint32_t ts_prev    = 0;
} tmp117Derived;

// ── Alert helpers ─────────────────────────────────────
const char* co2Level(uint16_t co2) {
  if (co2 > 2000) return "CRITICAL";
  if (co2 > 1500) return "ALERT";
  if (co2 > 1000) return "WARNING";
  return "OK";
}
const char* gLevel(float g) {
  if (g > 4.0f) return "HIGH-G";
  if (g > 2.5f) return "MODERATE-G";
  if (g > 1.5f) return "MOVEMENT";
  return "STABLE";
}
const char* gsrLevel(float delta) {
  if (delta >  500) return "HIGH-STRESS";
  if (delta >  200) return "ELEVATED";
  if (delta > -200) return "BASELINE";
  return "LOW";
}

// ════════════════════════════════════════════════════
// ECG derived — R-peak detection + HRV computation
// ════════════════════════════════════════════════════

// Called for every sample popped from the MAX30003 FIFO.
void processEcgSample(float mv, uint32_t ts) {
  float absMv = fabsf(mv);

  // Adaptive max — monte instantanément, décroît lentement (5% par seconde)
  if (absMv > ecgDerived.running_max)
    ecgDerived.running_max = absMv;
  if (ts - ecgDerived.max_decay_ts > 200) {
    ecgDerived.running_max *= 0.99f;
    if (ecgDerived.running_max < 5.0f) ecgDerived.running_max = 5.0f;
    ecgDerived.max_decay_ts = ts;
  }

  // Threshold à 60% du max adaptatif — plus conservateur
  float threshold = ecgDerived.running_max * 0.60f;
  bool  now_above = absMv > threshold;

  if (now_above && !ecgDerived.prev_above) {
    if (ecgDerived.last_peak_ts > 0) {
      uint32_t rr = ts - ecgDerived.last_peak_ts;
      // Fenêtre physiologique stricte : 40-180 BPM
      if (rr >= 333 && rr <= 1500) {
        // Rejet des RR trop différents du précédent (>50% d'écart)
        bool plausible = true;
        if (ecgDerived.rr_count > 0) {
          uint16_t prev_rr = ecgDerived.rr_buf[(ecgDerived.rr_idx + 19) % 20];
          float ratio = (float)rr / (float)prev_rr;
          if (ratio < 0.3f || ratio > 2.5f) plausible = false;
        }
        if (plausible) {
          ecgDerived.rr_buf[ecgDerived.rr_idx] = (uint16_t)rr;
          ecgDerived.rr_idx  = (ecgDerived.rr_idx + 1) % 20;
          if (ecgDerived.rr_count < 20) ecgDerived.rr_count++;
          ecgDerived.rr_last = (uint16_t)rr;
          ecgDerived.hr_bpm  = 60000.0f / rr;
        }
      }
    }
    ecgDerived.last_peak_ts = ts;
  }
  ecgDerived.prev_above = now_above;
}

// Compute RMSSD and pNN50 from the circular RR buffer — called once per second.
void computeHrv() {
  uint8_t n = ecgDerived.rr_count;
  if (n < 4) return;
  if (n > 20) n = 20;

  float   sum_sq = 0.0f;
  uint8_t nn50   = 0;
  for (uint8_t i = 1; i < n; i++) {
    uint8_t a = (ecgDerived.rr_idx + 20 - n + i - 1) % 20;
    uint8_t b = (ecgDerived.rr_idx + 20 - n + i    ) % 20;
    float diff = (float)ecgDerived.rr_buf[b] - (float)ecgDerived.rr_buf[a];
    sum_sq += diff * diff;
    if (fabsf(diff) > 50.0f) nn50++;
  }
  ecgDerived.rmssd = sqrtf(sum_sq / (n - 1));
  ecgDerived.pnn50 = (float)nn50 / (n - 1) * 100.0f;
  ecgDerived.valid = true;
}

// ════════════════════════════════════════════════════
// Breathing derived — zero-crossing analysis on SDP810
// ════════════════════════════════════════════════════
void processBreathSample(float pressure, uint32_t ts) {
  bool positive = pressure > 1.0f;  // ±1 Pa deadband eliminates noise near zero

  if (positive && !breathDerived.prev_positive) {
    // Rising zero-crossing = start of inhalation
    if (breathDerived.last_exhale_ts > 0)
      breathDerived.exhale_dur = ts - breathDerived.last_exhale_ts;

    if (breathDerived.last_inhale_ts > 0) {
      uint32_t period = ts - breathDerived.last_inhale_ts;
      if (period > 1500 && period < 12000) {  // 5–40 BrPM valid range
        breathDerived.br_intervals[breathDerived.br_idx] = period;
        breathDerived.br_idx = (breathDerived.br_idx + 1) % 8;
        if (breathDerived.br_count < 8) breathDerived.br_count++;

        uint32_t sum = 0;
        uint8_t  cnt = min(breathDerived.br_count, (uint8_t)5);
        for (uint8_t i = 0; i < cnt; i++)
          sum += breathDerived.br_intervals[(breathDerived.br_idx + 8 - 1 - i) % 8];
        breathDerived.br_bpm = 60000.0f / (sum / cnt);

        if (breathDerived.inhale_dur > 0 && breathDerived.exhale_dur > 0)
          breathDerived.ie_ratio = (float)breathDerived.inhale_dur / breathDerived.exhale_dur;
      }
    }
    breathDerived.last_inhale_ts = ts;
    breathDerived.peak_inhale    = 0.0f;

  } else if (!positive && breathDerived.prev_positive) {
    // Falling zero-crossing = start of exhalation
    if (breathDerived.last_inhale_ts > 0)
      breathDerived.inhale_dur = ts - breathDerived.last_inhale_ts;
    breathDerived.last_exhale_ts = ts;
    breathDerived.amplitude      = breathDerived.peak_inhale;
  }

  if (positive && pressure > breathDerived.peak_inhale)
    breathDerived.peak_inhale = pressure;

  breathDerived.apnea = breathDerived.last_inhale_ts > 0 &&
                        (ts - breathDerived.last_inhale_ts > 10000);
  breathDerived.prev_positive = positive;
}

// ════════════════════════════════════════════════════
// GSR derived — tonic/phasic EDA + conductance in µS
// ════════════════════════════════════════════════════
void processGsrSample(int raw, float voltage, uint32_t ts) {
  if (!gsrDerived.initialized) {
    gsrDerived.tonic_raw     = (float)raw;
    gsrDerived.scr_window_ts = ts;
    gsrDerived.initialized   = true;
  }

  // Tonic EDA: IIR low-pass, tau ≈ 10s at 50Hz → alpha = 20 / (10000 + 20)
  const float ALPHA = 0.002f;
  gsrDerived.tonic_raw = ALPHA * raw + (1.0f - ALPHA) * gsrDerived.tonic_raw;

  // Convert raw ADC → skin conductance in µS (1 / resistance)
  if (voltage > 0.01f) {
    float r_ohm = 1000.0f * (3.3f / voltage - 1.0f);
    gsrDerived.conductance_us = (r_ohm > 1.0f) ? (1e6f / r_ohm) : 0.0f;

    float tonic_v = gsrDerived.tonic_raw * (3.3f / 4095.0f);
    float tonic_r = (tonic_v > 0.01f) ? (1000.0f * (3.3f / tonic_v - 1.0f)) : 0.0f;
    gsrDerived.tonic_us = (tonic_r > 1.0f) ? (1e6f / tonic_r) : 0.0f;
    gsrDerived.phasic   = gsrDerived.conductance_us - gsrDerived.tonic_us;
  }

  // SCR detection: count rising edges of phasic > 1 µS per minute
  bool above = gsrDerived.phasic > 30.0f;
  if (above && !gsrDerived.prev_above_scr) gsrDerived.scr_window_cnt++;
  gsrDerived.prev_above_scr = above;

  if (ts - gsrDerived.scr_window_ts >= 60000) {
    gsrDerived.scr_per_min    = (uint8_t)min(255.0f, gsrDerived.scr_window_cnt);
    gsrDerived.scr_window_cnt = 0.0f;
    gsrDerived.scr_window_ts  = ts;
  }
}

// ════════════════════════════════════════════════════
// IMU derived — complementary filter + dynamics
// ════════════════════════════════════════════════════
void processImuSample() {
  if (!imuData.valid) return;

  const float dt    = IMU_INTERVAL_MS / 1000.0f;
  const float ALPHA = 0.98f;

  float pitch_acc = atan2f(imuData.accY,
                      sqrtf(imuData.accX*imuData.accX + imuData.accZ*imuData.accZ))
                    * 180.0f / (float)M_PI;
  float roll_acc  = atan2f(-imuData.accX, imuData.accZ) * 180.0f / (float)M_PI;

  if (!imuDerived.initialized) {
    imuDerived.pitch       = pitch_acc;
    imuDerived.roll        = roll_acc;
    imuDerived.g_prev      = imuData.totalG;
    imuDerived.initialized = true;
    return;
  }

  // Complementary filter: 98% gyro integration + 2% gravity correction
  imuDerived.pitch = ALPHA * (imuDerived.pitch + imuData.gyrX * dt)
                   + (1.0f - ALPHA) * pitch_acc;
  imuDerived.roll  = ALPHA * (imuDerived.roll  + imuData.gyrY * dt)
                   + (1.0f - ALPHA) * roll_acc;

  imuDerived.g_lat  = imuData.accY;  // lateral (cornering)
  imuDerived.g_long = imuData.accX;  // longitudinal (braking/acceleration)

  imuDerived.jerk  = fabsf(imuData.totalG - imuDerived.g_prev) / dt;
  imuDerived.g_prev = imuData.totalG;

  imuDerived.cervical_cumul += fabsf(imuData.gyrZ) * dt;
}

// ════════════════════════════════════════════════════
// SCD41 derived — CO₂ trend + psychrometrics
// ════════════════════════════════════════════════════
void processScd41Sample() {
  if (!scd41.valid) return;

  if (scd41Derived.ts_prev > 0) {
    float dt_min = (scd41.ts - scd41Derived.ts_prev) / 60000.0f;
    if (dt_min > 0.0f)
      scd41Derived.co2_slope = ((float)scd41.co2 - (float)scd41Derived.co2_prev) / dt_min;
  }
  scd41Derived.co2_prev = scd41.co2;
  scd41Derived.ts_prev  = scd41.ts;

  // Absolute humidity via Magnus formula
  float T  = scd41.temp;
  float RH = scd41.humidity;
  float e_sat = 6.112f * expf(17.67f * T / (T + 243.5f));
  scd41Derived.abs_humidity = (e_sat * RH * 2.1674f) / (273.15f + T);

  // Dew point (Lawrence approximation, valid 0–60°C)
  float gamma = logf(RH / 100.0f) + 17.67f * T / (243.5f + T);
  scd41Derived.dew_point = 243.5f * gamma / (17.67f - gamma);
}

// ════════════════════════════════════════════════════
// TMP117 derived — thermal load rate
// ════════════════════════════════════════════════════
void processTmp117Sample() {
  if (!tmp117Data.valid) return;

  if (tmp117Derived.temp_boot < -900.0f) {
    tmp117Derived.temp_boot = tmp117Data.temp;
    tmp117Derived.temp_prev = tmp117Data.temp;
    tmp117Derived.ts_prev   = tmp117Data.ts;
    return;
  }

  tmp117Derived.delta_boot = tmp117Data.temp - tmp117Derived.temp_boot;

  if (tmp117Derived.ts_prev > 0) {
    float dt_min = (tmp117Data.ts - tmp117Derived.ts_prev) / 60000.0f;
    if (dt_min > 0.0f)
      tmp117Derived.dT_dt = (tmp117Data.temp - tmp117Derived.temp_prev) / dt_min;
  }
  tmp117Derived.temp_prev = tmp117Data.temp;
  tmp117Derived.ts_prev   = tmp117Data.ts;
}

// ════════════════════════════════════════════════════
// CSV structured output — one line per second
// Capture: pio device monitor | findstr /B DATA > session.csv
// Python:  df = pd.read_csv('session.csv', comment='#')
// ════════════════════════════════════════════════════
void printCsvHeader() {
  Serial.println(
    "# BALASENSE DATA — capture with: pio device monitor | grep ^DATA > session.csv"
  );
  Serial.println(
    "ts_ms,"
    "hr_bpm,rmssd_ms,pnn50_pct,rr_last_ms,"
    "br_bpm,ie_ratio,breath_amp_pa,apnea,"
    "gsr_cond_us,gsr_tonic_us,gsr_phasic_us,scr_per_min,"
    "skin_temp_c,tmp_delta_boot_c,tmp_dtdt_c_min,"
    "co2_ppm,co2_slope_ppm_min,abs_hum_gm3,dew_point_c,"
    "g_total,g_lat,g_long,jerk_g_s,pitch_deg,roll_deg,rot_dps,cervical_cumul_deg"
  );
}

void printCsvLine() {
  Serial.printf(
    "DATA,%lu,"
    "%.1f,%.1f,%.1f,%u,"
    "%.1f,%.2f,%.1f,%d,"
    "%.4f,%.4f,%.4f,%u,"
    "%.2f,%.3f,%.4f,"
    "%u,%.1f,%.2f,%.1f,"
    "%.3f,%.3f,%.3f,%.2f,%.1f,%.1f,%.1f,%.1f\n",
    millis(),
    // Cardiac
    ecgDerived.hr_bpm, ecgDerived.rmssd, ecgDerived.pnn50, ecgDerived.rr_last,
    // Breathing
    breathDerived.br_bpm, breathDerived.ie_ratio,
    breathDerived.amplitude, (int)breathDerived.apnea,
    // EDA / stress
    gsrDerived.conductance_us, gsrDerived.tonic_us,
    gsrDerived.phasic, gsrDerived.scr_per_min,
    // Thermal
    tmp117Data.temp, tmp117Derived.delta_boot, tmp117Derived.dT_dt,
    // Respiration chemistry
    scd41.co2, scd41Derived.co2_slope,
    scd41Derived.abs_humidity, scd41Derived.dew_point,
    // Dynamics
    imuData.totalG, imuDerived.g_lat, imuDerived.g_long,
    imuDerived.jerk, imuDerived.pitch, imuDerived.roll,
    imuData.totalRot, imuDerived.cervical_cumul
  );
}

// ════════════════════════════════════════════════════
// SCD41
// ════════════════════════════════════════════════════
void initScd41() {
  scd4x.begin(Wire);
  scd4x.stopPeriodicMeasurement();
  delay(500);
  uint16_t err = scd4x.startPeriodicMeasurement();
  if (err) { Serial.println("[SCD41] ERROR — init failed"); while (1); }
  Serial.println("[SCD41] OK — 0.2Hz");
}

void readScd41() {
  bool dataReady = false;
  scd4x.getDataReadyFlag(dataReady);
  if (!dataReady) return;
  uint16_t err = scd4x.readMeasurement(scd41.co2, scd41.temp, scd41.humidity);
  if (err) { Serial.println("[SCD41] Read error"); return; }
  scd41.ts    = millis();
  scd41.valid = true;
  processScd41Sample();
  Serial.printf("\n[SCD41] t=%lums | CO2: %u ppm [%s] | Temp: %.1fC | Hum: %.1f%% | AH:%.2f g/m³ | DP:%.1fC | slope:%.1f ppm/min\n\n",
                scd41.ts, scd41.co2, co2Level(scd41.co2),
                scd41.temp, scd41.humidity,
                scd41Derived.abs_humidity, scd41Derived.dew_point,
                scd41Derived.co2_slope);
}

// ════════════════════════════════════════════════════
// IMU
// ════════════════════════════════════════════════════
void initImu() {
  imu.begin(Wire, 0);
  if (imu.status != ICM_20948_Stat_Ok) {
    Serial.println("[IMU]   ERROR — not found");
    imuOk = false;
    return;
  }
  imuOk          = true;
  lastImuSuccess = millis();
  Serial.println("[IMU]   OK — 100Hz");
}

bool recoverImu() {
  Serial.println("[IMU]   Attempting recovery...");
  Wire.end();
  delay(50);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  delay(50);
  imu.begin(Wire, 0);
  if (imu.status != ICM_20948_Stat_Ok) {
    Serial.println("[IMU]   Recovery FAILED");
    imuOk = false;
    return false;
  }
  Serial.println("[IMU]   Recovery OK");
  lastImuSuccess = millis();
  imuOk          = true;
  return true;
}

void checkImuAlert() {
  bool triggered = (imuData.totalG   > IMU_G_THRESHOLD ||
                    imuData.totalRot > IMU_ROT_THRESHOLD);
  if (triggered) {
    if (!imuInAlert) {
      imuAlertStart = millis();
      imuInAlert    = true;
    } else if (millis() - imuAlertStart >= IMU_ALERT_DURATION) {
      Serial.printf("[IMU]  ⚠ SUSTAINED t=%lums | G:%.2f lat:%.2f long:%.2f | rot:%.1f | jerk:%.1f g/s | dur:%lums\n",
                    imuData.ts, imuData.totalG,
                    imuDerived.g_lat, imuDerived.g_long,
                    imuData.totalRot, imuDerived.jerk,
                    millis() - imuAlertStart);
    }
  } else {
    imuInAlert    = false;
    imuAlertStart = 0;
  }
}

void readImu() {
  if (millis() - lastImuSuccess > IMU_TIMEOUT_MS) { imuOk = false; recoverImu(); return; }
  if (!imu.dataReady()) return;
  imu.getAGMT();

  imuData.accX = imu.accX() / 1000.0f;
  imuData.accY = imu.accY() / 1000.0f;
  imuData.accZ = imu.accZ() / 1000.0f;
  imuData.gyrX = imu.gyrX();
  imuData.gyrY = imu.gyrY();
  imuData.gyrZ = imu.gyrZ();
  imuData.magX = imu.magX();
  imuData.magY = imu.magY();
  imuData.magZ = imu.magZ();

  imuData.totalG   = sqrtf(imuData.accX*imuData.accX + imuData.accY*imuData.accY + imuData.accZ*imuData.accZ);
  imuData.totalRot = sqrtf(imuData.gyrX*imuData.gyrX + imuData.gyrY*imuData.gyrY + imuData.gyrZ*imuData.gyrZ);

  if (imuData.totalG   > imuData.peakG)   imuData.peakG   = imuData.totalG;
  if (imuData.totalRot > imuData.peakRot) imuData.peakRot = imuData.totalRot;

  imuData.ts     = millis();
  imuData.valid  = true;
  imuOk          = true;
  lastImuSuccess = millis();

  processImuSample();

  if (DEBUG_IMU && millis() - lastImuPrint >= 100) {
    lastImuPrint = millis();
    Serial.printf("[IMU] t=%lums G:%.2f[%s] lat:%.2f long:%.2f rot:%.1f pitch:%.1f roll:%.1f jerk:%.1f\n",
                  imuData.ts, imuData.totalG, gLevel(imuData.totalG),
                  imuDerived.g_lat, imuDerived.g_long,
                  imuData.totalRot, imuDerived.pitch, imuDerived.roll, imuDerived.jerk);
  }
  checkImuAlert();
}

// ════════════════════════════════════════════════════
// GSR
// ════════════════════════════════════════════════════
void initGsr() {
  pinMode(GSR_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  Serial.println("[GSR]   Place electrodes on skin...");
  Serial.println("[GSR]   Calibrating baseline in 5s — stay calm");
  delay(5000);
  Serial.print("[GSR]   Calibrating...");
  long sum = 0;
  for (int i = 0; i < 100; i++) { sum += analogRead(GSR_PIN); delay(10); }
  gsrData.baseline   = sum / 100.0f;
  gsrData.calibrated = true;
  Serial.printf(" baseline=%.0f\n", gsrData.baseline);
  Serial.println("[GSR]   OK — 50Hz");
}

void readGsr() {
  gsrData.raw     = analogRead(GSR_PIN);
  gsrData.voltage = gsrData.raw * (3.3f / 4095.0f);
  if (gsrData.voltage > 0.01f)
    gsrData.resistance = 1000.0f * (3.3f / gsrData.voltage - 1.0f);
  gsrData.delta = gsrData.raw - gsrData.baseline;
  gsrData.ts    = millis();
  gsrData.valid = true;

  processGsrSample(gsrData.raw, gsrData.voltage, gsrData.ts);

  if (gsrData.delta > 500)
    Serial.printf("[GSR]  ⚠ STRESS SPIKE t=%lums | raw=%d delta=%+.0f | cond=%.4f µS | phasic=%.4f µS\n",
                  gsrData.ts, gsrData.raw, gsrData.delta,
                  gsrDerived.conductance_us, gsrDerived.phasic);
}

// ════════════════════════════════════════════════════
// SDP810
// ════════════════════════════════════════════════════
bool sdp810CheckCrc(uint8_t msb, uint8_t lsb, uint8_t crc) {
  uint8_t data[2] = {msb, lsb};
  uint8_t c = 0xFF;
  for (int i = 0; i < 2; i++) {
    c ^= data[i];
    for (int b = 0; b < 8; b++)
      c = (c & 0x80) ? (c << 1) ^ 0x31 : (c << 1);
  }
  return c == crc;
}

void initSdp810() {
  Wire.beginTransmission(SDP810_ADDR);
  Wire.write(SDP810_CMD_STOP >> 8);
  Wire.write(SDP810_CMD_STOP & 0xFF);
  Wire.endTransmission();
  delay(500);
  Wire.beginTransmission(SDP810_ADDR);
  Wire.write(SDP810_CMD_START >> 8);
  Wire.write(SDP810_CMD_START & 0xFF);
  uint8_t err = Wire.endTransmission();
  if (err != 0) { Serial.println("[SDP810] ERROR — init failed"); return; }
  delay(25);
  Serial.println("[SDP810] OK — 25Hz continuous");
}

void readSdp810() {
  uint8_t buf[9];
  uint8_t received = Wire.requestFrom((uint8_t)SDP810_ADDR, (uint8_t)9);
  if (received != 9) { Serial.println("[SDP810] Read error — wrong byte count"); return; }
  for (int i = 0; i < 9; i++) buf[i] = Wire.read();
  if (!sdp810CheckCrc(buf[0], buf[1], buf[2])) { Serial.println("[SDP810] CRC error — pressure"); return; }
  if (!sdp810CheckCrc(buf[3], buf[4], buf[5])) { Serial.println("[SDP810] CRC error — temperature"); return; }

  int16_t rawDP   = (int16_t)((buf[0] << 8) | buf[1]);
  int16_t rawTemp = (int16_t)((buf[3] << 8) | buf[4]);
  sdp810.pressure = (float)rawDP   / SDP810_SCALE_FACTOR;
  sdp810.temp     = (float)rawTemp / 200.0f;
  sdp810.flowRate = sdp810.pressure * 0.1f;  // placeholder — calibrate with known flow
  sdp810.ts       = millis();
  sdp810.valid    = true;

  processBreathSample(sdp810.pressure, sdp810.ts);

  if (fabsf(sdp810.pressure) > 200.0f)
    Serial.printf("[SDP810] ⚠ HIGH PRESSURE t=%lums | %.1f Pa | BrPM:%.1f I:E=%.2f amp:%.1fPa\n",
                  sdp810.ts, sdp810.pressure,
                  breathDerived.br_bpm, breathDerived.ie_ratio, breathDerived.amplitude);
}

// ════════════════════════════════════════════════════
// TMP117
// ════════════════════════════════════════════════════
void initTmp117() {
  if (!tmp117.begin()) { Serial.println("[TMP117] ERROR — not found"); return; }
  Serial.println("[TMP117] OK — 1Hz");
}

void readTmp117() {
  sensors_event_t temp;
  tmp117.getEvent(&temp);
  tmp117Data.temp  = temp.temperature;
  tmp117Data.ts    = millis();
  tmp117Data.valid = true;

  processTmp117Sample();

  if (tmp117Data.temp > 38.5f)
    Serial.printf("[TMP117] ⚠ HIGH SKIN TEMP t=%lums | %.2fC (+%.3fC/min)\n",
                  tmp117Data.ts, tmp117Data.temp, tmp117Derived.dT_dt);
}

// ════════════════════════════════════════════════════
// MAX30003
// ════════════════════════════════════════════════════
void max30003WriteReg(uint8_t reg, uint32_t data) {
  digitalWrite(MAX30003_CS, LOW);
  delayMicroseconds(10);
  SPI.transfer(reg << 1);
  SPI.transfer((data >> 16) & 0xFF);
  SPI.transfer((data >> 8)  & 0xFF);
  SPI.transfer((data)       & 0xFF);
  delayMicroseconds(10);
  digitalWrite(MAX30003_CS, HIGH);
  delayMicroseconds(10);
}

uint32_t max30003ReadReg(uint8_t reg) {
  uint32_t data = 0;
  digitalWrite(MAX30003_CS, LOW);
  delayMicroseconds(10);
  SPI.transfer((reg << 1) | 0x01);
  delayMicroseconds(10);
  data  = (uint32_t)SPI.transfer(0xFF) << 16;
  data |= (uint32_t)SPI.transfer(0xFF) << 8;
  data |= (uint32_t)SPI.transfer(0xFF);
  delayMicroseconds(10);
  digitalWrite(MAX30003_CS, HIGH);
  delayMicroseconds(10);
  return data;
}

void initMax30003() {
  delay(100);
  max30003WriteReg(0x08, 0x000000);  // soft reset
  delay(500);

  digitalWrite(MAX30003_CS, HIGH); delay(100);
  digitalWrite(MAX30003_CS, LOW);  delay(10);
  digitalWrite(MAX30003_CS, HIGH); delay(100);

  uint32_t info = max30003ReadReg(0x0F);
  Serial.printf("[MAX30003] INFO=0x%06X\n", info);
  if ((info & 0xF00000) != 0x500000) {
    Serial.printf("[MAX30003] ERROR — chip not responding (INFO=0x%06X)\n", info);
    ecgOk = false;
    return;
  }

  max30003WriteReg(0x10, 0x081007); delay(10);  // CNFG_GEN: EN_ECG=1, lead bias 100MΩ
  max30003WriteReg(0x14, 0x800000); delay(10);  // CNFG_EMUX: OPENP=0, OPENN=0, POL=1
  max30003WriteReg(0x15, 0x805000); delay(10);  // CNFG_ECG: 20V/V gain, 512Hz, HPF 0.5Hz
  max30003WriteReg(0x1D, 0x3FC600); delay(10);  // CNFG_RTOR1: R-to-R detection enabled

  uint32_t cnfg_gen = max30003ReadReg(0x10);
  Serial.printf("[MAX30003] CNFG_GEN readback=0x%06X\n", cnfg_gen);

  Serial.print("[MAX30003] Attente PLL...");
  uint32_t pllStart = millis();
  bool pllOk = false;
  while (millis() - pllStart < 10000) {
    uint32_t st = max30003ReadReg(0x01);
    if (!(st & 0x000100)) { Serial.println(" OK"); pllOk = true; break; }
    delay(50);
  }
  if (!pllOk) {
    Serial.println(" TIMEOUT — PLLINT toujours actif");
    Serial.println("[MAX30003] ⚠ Vérifier fil GPIO26 → FCLK");
  }

  max30003WriteReg(0x0A, 0x000000); delay(10);   // FIFO reset
  max30003WriteReg(0x09, 0x000000); delay(200);  // SYNCH — start acquisition

  uint32_t status = max30003ReadReg(0x01);
  Serial.printf("[MAX30003] STATUS final=0x%06X\n", status);
  ecgOk = true;
  Serial.println("[MAX30003] OK — ECG 512Hz");
}

void readMax30003() {
  if (!ecgOk) return;

  uint32_t now = millis();
  int sampleCount = 0;

  // Pré-compter les samples disponibles pour interpoler les timestamps
  for (int i = 0; i < 32; i++) {
    uint32_t fifo = max30003ReadReg(0x21);
    uint8_t  etag = (fifo >> 3) & 0x07;

    if (etag == 0x07) { max30003WriteReg(0x0A, 0x000000); return; }
    if (etag == 0x06) break;

    int32_t raw = (int32_t)(fifo >> 6);
    if (raw & 0x20000) raw |= 0xFFFC0000;

    ecgData.raw   = raw;
    ecgData.mv    = (float)raw * (1.0f / 131072.0f) * 1000.0f;
    // Interpolation : distribue les 50ms entre les samples du burst
    ecgData.ts    = now - (ECG_INTERVAL_MS * (32 - i) / 32);
    ecgData.valid = true;

    processEcgSample(ecgData.mv, ecgData.ts);

    Serial.printf("ECG,%lu,%ld\n", ecgData.ts, ecgData.raw);

    if (etag == 0x02 || etag == 0x03) break;
  }
}

// ════════════════════════════════════════════════════
// Summary
// ════════════════════════════════════════════════════
void printSummary() {
  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
  Serial.printf("t=%lums\n", millis());

  if (imuOk && imuData.valid)
    Serial.printf("  IMU    | G:%.2f[%s] lat:%.2f long:%.2f | rot:%.1f | peak G:%.2f | pitch:%.1f° roll:%.1f° | jerk:%.1f g/s | cerv:%.0f°\n",
                  imuData.totalG, gLevel(imuData.totalG),
                  imuDerived.g_lat, imuDerived.g_long,
                  imuData.totalRot, imuData.peakG,
                  imuDerived.pitch, imuDerived.roll,
                  imuDerived.jerk, imuDerived.cervical_cumul);
  else
    Serial.println("  IMU    | [DISCONNECTED]");

  if (scd41.valid)
    Serial.printf("  CO2    | %u ppm [%s] | %.1fC | %.1f%% | AH:%.2f g/m³ | DP:%.1fC | slope:%.1f ppm/min\n",
                  scd41.co2, co2Level(scd41.co2), scd41.temp, scd41.humidity,
                  scd41Derived.abs_humidity, scd41Derived.dew_point, scd41Derived.co2_slope);
  else
    Serial.println("  CO2    | [NO DATA YET]");

  if (gsrData.valid)
    Serial.printf("  GSR    | raw=%d delta=%+.0f [%s] | cond=%.4f µS | tonic=%.4f | phasic=%.4f | SCR=%u/min\n",
                  gsrData.raw, gsrData.delta, gsrLevel(gsrData.delta),
                  gsrDerived.conductance_us, gsrDerived.tonic_us,
                  gsrDerived.phasic, gsrDerived.scr_per_min);
  else
    Serial.println("  GSR    | [NO DATA YET]");

  if (sdp810.valid)
    Serial.printf("  SDP810 | P:%.2f Pa | T:%.1fC | BrPM:%.1f | I:E=%.2f | amp:%.1f Pa%s\n",
                  sdp810.pressure, sdp810.temp,
                  breathDerived.br_bpm, breathDerived.ie_ratio,
                  breathDerived.amplitude,
                  breathDerived.apnea ? " ⚠ APNEA" : "");
  else
    Serial.println("  SDP810 | [NO DATA YET]");

  if (tmp117Data.valid)
    Serial.printf("  TMP117 | %.2fC | +%.3fC vs boot | %.4fC/min\n",
                  tmp117Data.temp, tmp117Derived.delta_boot, tmp117Derived.dT_dt);
  else
    Serial.println("  TMP117 | [NO DATA YET]");

  if (ecgOk && ecgData.valid) {
    if (ecgDerived.valid)
      Serial.printf("  ECG    | raw=%ld mv=%.3f | HR:%.1f BPM | RR:%u ms | RMSSD:%.1f ms | pNN50:%.1f%%\n",
                    ecgData.raw, ecgData.mv,
                    ecgDerived.hr_bpm, ecgDerived.rr_last,
                    ecgDerived.rmssd, ecgDerived.pnn50);
    else
      Serial.printf("  ECG    | raw=%ld mv=%.3f | [accumulating RR — need 4+ beats]\n",
                    ecgData.raw, ecgData.mv);
  } else
    Serial.println("  ECG    | [NO DATA YET]");

  imuData.peakG   = 0;
  imuData.peakRot = 0;
  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
}

// ════════════════════════════════════════════════════
// Setup & Loop
// ════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  // SPI before Wire — prevents I²C bus conflict
  pinMode(MAX30003_CS, OUTPUT);
  digitalWrite(MAX30003_CS, HIGH);
  SPI.begin(MAX30003_SCK, MAX30003_MISO, MAX30003_MOSI, MAX30003_CS);
  SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  delay(100);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  delay(1000);

  // FCLK 32.768kHz for MAX30003 via LEDC
  ledcSetup(0, 32768, 8);
  ledcAttachPin(MAX30003_FCLK, 0);
  ledcWrite(0, 128);
  delay(100);

  Serial.println("\n=== BalaSense V1 — init ===\n");
  initScd41();
  initImu();
  initGsr();
  initSdp810();
  initTmp117();
  initMax30003();
  Serial.println("\n=== Acquisition started ===\n");

  printCsvHeader();

  lastImuSuccess = millis();
}

void loop() {
  uint32_t now = millis();

  if (now - lastImuRead    >= IMU_INTERVAL_MS)    { lastImuRead    = now; readImu();      }
  if (now - lastScd41Read  >= SCD41_INTERVAL_MS)  { lastScd41Read  = now; readScd41();    }
  if (now - lastGsrRead    >= GSR_INTERVAL_MS)    { lastGsrRead    = now; readGsr();      }
  if (now - lastSdp810Read >= SDP810_INTERVAL_MS) { lastSdp810Read = now; readSdp810();   }
  if (now - lastTmp117Read >= TMP117_INTERVAL_MS) { lastTmp117Read = now; readTmp117();   }
  if (now - lastEcgRead    >= ECG_INTERVAL_MS)    { lastEcgRead    = now; readMax30003(); }
  if (now - lastSummary    >= SUMMARY_INTERVAL)   { lastSummary    = now; printSummary(); }
  if (now - lastCsv        >= CSV_INTERVAL_MS)    { lastCsv        = now; computeHrv(); printCsvLine(); }
}

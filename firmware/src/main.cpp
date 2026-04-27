#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CScd4x.h>
#include "ICM_20948.h"

// ── I2C ─────────────────────────────────────────────
#define I2C_SDA 21
#define I2C_SCL 22

// ── GSR ─────────────────────────────────────────────
#define GSR_PIN 32

// ── Debug flags ──────────────────────────────────────
#define DEBUG_IMU   false
#define DEBUG_SCD41 true

// ── Sensor instances ─────────────────────────────────
SensirionI2CScd4x scd4x;
ICM_20948_I2C     imu;

// ── Timing ───────────────────────────────────────────
const uint32_t SCD41_INTERVAL_MS = 5000;
const uint32_t IMU_INTERVAL_MS   = 10;
const uint32_t GSR_INTERVAL_MS   = 20;
const uint32_t SUMMARY_INTERVAL  = 2000;
const uint32_t IMU_TIMEOUT_MS    = 2000;

uint32_t lastScd41Read  = 0;
uint32_t lastImuRead    = 0;
uint32_t lastGsrRead    = 0;
uint32_t lastSummary    = 0;
uint32_t lastImuPrint   = 0;
uint32_t lastImuSuccess = 0;

// ── IMU health ───────────────────────────────────────
bool imuOk = false;

// ── IMU alert filter ─────────────────────────────────
const float    IMU_ROT_THRESHOLD  = 80.0f;   // dps
const float    IMU_G_THRESHOLD    = 2.5f;    // G
const uint32_t IMU_ALERT_DURATION = 50;      // ms sustained

uint32_t imuAlertStart = 0;
bool     imuInAlert    = false;

// ── Data structs ─────────────────────────────────────
struct Scd41Data {
  uint16_t co2      = 0;
  float    temp     = 0.0f;
  float    humidity = 0.0f;
  uint32_t ts       = 0;
  bool     valid    = false;
} scd41;

struct ImuData {
  float    accX = 0, accY = 0, accZ = 0;
  float    gyrX = 0, gyrY = 0, gyrZ = 0;
  float    magX = 0, magY = 0, magZ = 0;
  float    totalG   = 0;
  float    totalRot = 0;
  float    peakG    = 0;
  float    peakRot  = 0;
  uint32_t ts       = 0;
  bool     valid    = false;
} imuData;

struct GsrData {
  int      raw        = 0;
  float    voltage    = 0.0f;
  float    resistance = 0.0f;
  float    baseline   = 0.0f;
  float    delta      = 0.0f;
  uint32_t ts         = 0;
  bool     valid      = false;
  bool     calibrated = false;
} gsrData;

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
// SCD41
// ════════════════════════════════════════════════════
void initScd41() {
  scd4x.begin(Wire);
  scd4x.stopPeriodicMeasurement();
  delay(500);
  uint16_t err = scd4x.startPeriodicMeasurement();
  if (err) {
    Serial.println("[SCD41] ERROR — init failed");
    while (1);
  }
  Serial.println("[SCD41] OK — 0.2Hz");
}

void readScd41() {
  bool dataReady = false;
  scd4x.getDataReadyFlag(dataReady);
  if (!dataReady) return;

  uint16_t err = scd4x.readMeasurement(
    scd41.co2, scd41.temp, scd41.humidity
  );
  if (err) {
    Serial.println("[SCD41] Read error");
    return;
  }

  scd41.ts    = millis();
  scd41.valid = true;

  Serial.printf("\n[SCD41] t=%lums | CO2: %u ppm [%s] | "
                "Temp: %.1fC | Hum: %.1f%%\n\n",
                scd41.ts, scd41.co2, co2Level(scd41.co2),
                scd41.temp, scd41.humidity);
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

// ── IMU sustained alert filter ───────────────────────
void checkImuAlert() {
  bool triggered = (imuData.totalG   > IMU_G_THRESHOLD ||
                    imuData.totalRot > IMU_ROT_THRESHOLD);

  if (triggered) {
    if (!imuInAlert) {
      imuAlertStart = millis();
      imuInAlert    = true;
    } else if (millis() - imuAlertStart >= IMU_ALERT_DURATION) {
      Serial.printf("[IMU]  ⚠ SUSTAINED t=%lums | "
                    "G:%.2f | rot:%.1f dps | dur:%lums\n",
                    imuData.ts, imuData.totalG, imuData.totalRot,
                    millis() - imuAlertStart);
    }
  } else {
    imuInAlert    = false;
    imuAlertStart = 0;
  }
}

void readImu() {
  if (millis() - lastImuSuccess > IMU_TIMEOUT_MS) {
    imuOk = false;
    recoverImu();
    return;
  }

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

  imuData.totalG = sqrt(
    pow(imuData.accX, 2) +
    pow(imuData.accY, 2) +
    pow(imuData.accZ, 2)
  );
  imuData.totalRot = sqrt(
    pow(imuData.gyrX, 2) +
    pow(imuData.gyrY, 2) +
    pow(imuData.gyrZ, 2)
  );

  if (imuData.totalG   > imuData.peakG)   imuData.peakG   = imuData.totalG;
  if (imuData.totalRot > imuData.peakRot) imuData.peakRot = imuData.totalRot;

  imuData.ts     = millis();
  imuData.valid  = true;
  imuOk          = true;
  lastImuSuccess = millis();

  if (DEBUG_IMU && millis() - lastImuPrint >= 100) {
    lastImuPrint = millis();
    Serial.printf("[IMU] t=%lums G:%.2f[%s] rot:%.1f\n",
                  imuData.ts, imuData.totalG,
                  gLevel(imuData.totalG), imuData.totalRot);
  }

  // Sustained alert — ignores vibration spikes
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
  for (int i = 0; i < 100; i++) {
    sum += analogRead(GSR_PIN);
    delay(10);
  }
  gsrData.baseline   = sum / 100.0f;
  gsrData.calibrated = true;
  Serial.printf(" baseline=%.0f\n", gsrData.baseline);
  Serial.println("[GSR]   OK — 50Hz");
}

void readGsr() {
  gsrData.raw = analogRead(GSR_PIN);

  gsrData.voltage = gsrData.raw * (3.3f / 4095.0f);

  // Resistance with 100kΩ pull-up (1MΩ in final version)
  if (gsrData.voltage > 0.01f) {
    gsrData.resistance = 100.0f * (3.3f / gsrData.voltage - 1.0f);
  }

  gsrData.delta = gsrData.raw - gsrData.baseline;
  gsrData.ts    = millis();
  gsrData.valid = true;

  if (gsrData.delta > 500) {
    Serial.printf("[GSR]  ⚠ STRESS SPIKE t=%lums | "
                  "raw=%d delta=%+.0f\n",
                  gsrData.ts, gsrData.raw, gsrData.delta);
  }
}

// ════════════════════════════════════════════════════
// Summary
// ════════════════════════════════════════════════════
void printSummary() {
  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
  Serial.printf("t=%lums\n", millis());

  if (imuOk && imuData.valid)
    Serial.printf("  IMU  | G:%.2f[%s] rot:%.1f | "
                  "peak G:%.2f rot:%.1f\n",
                  imuData.totalG, gLevel(imuData.totalG),
                  imuData.totalRot,
                  imuData.peakG, imuData.peakRot);
  else
    Serial.println("  IMU  | [DISCONNECTED]");

  if (scd41.valid)
    Serial.printf("  CO2  | %u ppm [%s] | %.1fC | %.1f%%\n",
                  scd41.co2, co2Level(scd41.co2),
                  scd41.temp, scd41.humidity);
  else
    Serial.println("  CO2  | [NO DATA YET]");

  if (gsrData.valid)
    Serial.printf("  GSR  | raw=%d R=%.1fkΩ delta=%+.0f [%s]\n",
                  gsrData.raw, gsrData.resistance,
                  gsrData.delta, gsrLevel(gsrData.delta));
  else
    Serial.println("  GSR  | [NO DATA YET]");

  imuData.peakG   = 0;
  imuData.peakRot = 0;

  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
}

// ════════════════════════════════════════════════════
// Setup & Loop
// ════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  delay(1000);

  Serial.println("\n=== BalaSense V1 — init ===\n");
  initScd41();
  initImu();
  initGsr();
  Serial.println("\n=== Acquisition started ===\n");

  lastImuSuccess = millis();
}

void loop() {
  uint32_t now = millis();

  if (now - lastImuRead >= IMU_INTERVAL_MS) {
    lastImuRead = now;
    readImu();
  }

  if (now - lastScd41Read >= SCD41_INTERVAL_MS) {
    lastScd41Read = now;
    readScd41();
  }

  if (now - lastGsrRead >= GSR_INTERVAL_MS) {
    lastGsrRead = now;
    readGsr();
  }

  if (now - lastSummary >= SUMMARY_INTERVAL) {
    lastSummary = now;
    printSummary();
  }

  // ← SDP810, TMP117, MAX30003 will slot in here
}
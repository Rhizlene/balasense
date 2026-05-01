#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SensirionI2CScd4x.h>
#include "ICM_20948.h"
#include <Adafruit_TMP117.h>
#include <Adafruit_Sensor.h>

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
#define MAX30003_FCLK  26   // GPIO26 → pin FCLK du CJMCU-30003 (fil à brancher)

// ── Debug flags ──────────────────────────────────────
#define DEBUG_IMU   false
#define DEBUG_SCD41 true

// ── Sensor instances ─────────────────────────────────
SensirionI2CScd4x scd4x;
ICM_20948_I2C     imu;
Adafruit_TMP117   tmp117;

// ── Timing ───────────────────────────────────────────
const uint32_t SCD41_INTERVAL_MS   = 5000;
const uint32_t IMU_INTERVAL_MS     = 10;
const uint32_t GSR_INTERVAL_MS     = 20;
const uint32_t SDP810_INTERVAL_MS  = 40;
const uint32_t TMP117_INTERVAL_MS  = 1000;
const uint32_t ECG_INTERVAL_MS     = 10;
const uint32_t SUMMARY_INTERVAL    = 2000;
const uint32_t IMU_TIMEOUT_MS      = 2000;

uint32_t lastScd41Read  = 0;
uint32_t lastImuRead    = 0;
uint32_t lastGsrRead    = 0;
uint32_t lastSdp810Read = 0;
uint32_t lastTmp117Read = 0;
uint32_t lastEcgRead    = 0;
uint32_t lastSummary    = 0;
uint32_t lastImuPrint   = 0;
uint32_t lastImuSuccess = 0;

// ── IMU health ───────────────────────────────────────
bool imuOk = false;
bool ecgOk = false;

// ── IMU alert filter ─────────────────────────────────
const float    IMU_ROT_THRESHOLD  = 80.0f;
const float    IMU_G_THRESHOLD    = 2.5f;
const uint32_t IMU_ALERT_DURATION = 50;

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

struct Sdp810Data {
  float    pressure = 0.0f;
  float    temp     = 0.0f;
  float    flowRate = 0.0f;
  uint32_t ts       = 0;
  bool     valid    = false;
} sdp810;

struct Tmp117Data {
  float    temp  = 0.0f;
  uint32_t ts    = 0;
  bool     valid = false;
} tmp117Data;

struct EcgData {
  int32_t  raw   = 0;
  float    mv    = 0.0f;
  uint32_t ts    = 0;
  bool     valid = false;
} ecgData;

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
  Serial.printf("\n[SCD41] t=%lums | CO2: %u ppm [%s] | Temp: %.1fC | Hum: %.1f%%\n\n",
                scd41.ts, scd41.co2, co2Level(scd41.co2), scd41.temp, scd41.humidity);
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
      Serial.printf("[IMU]  ⚠ SUSTAINED t=%lums | G:%.2f | rot:%.1f dps | dur:%lums\n",
                    imuData.ts, imuData.totalG, imuData.totalRot, millis() - imuAlertStart);
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

  imuData.totalG   = sqrt(pow(imuData.accX,2) + pow(imuData.accY,2) + pow(imuData.accZ,2));
  imuData.totalRot = sqrt(pow(imuData.gyrX,2) + pow(imuData.gyrY,2) + pow(imuData.gyrZ,2));

  if (imuData.totalG   > imuData.peakG)   imuData.peakG   = imuData.totalG;
  if (imuData.totalRot > imuData.peakRot) imuData.peakRot = imuData.totalRot;

  imuData.ts     = millis();
  imuData.valid  = true;
  imuOk          = true;
  lastImuSuccess = millis();

  if (DEBUG_IMU && millis() - lastImuPrint >= 100) {
    lastImuPrint = millis();
    Serial.printf("[IMU] t=%lums G:%.2f[%s] rot:%.1f\n",
                  imuData.ts, imuData.totalG, gLevel(imuData.totalG), imuData.totalRot);
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
  if (gsrData.delta > 500)
    Serial.printf("[GSR]  ⚠ STRESS SPIKE t=%lums | raw=%d delta=%+.0f\n",
                  gsrData.ts, gsrData.raw, gsrData.delta);
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
  sdp810.flowRate = sdp810.pressure * 0.1f;
  sdp810.ts       = millis();
  sdp810.valid    = true;
  if (abs(sdp810.pressure) > 200.0f)
    Serial.printf("[SDP810] ⚠ HIGH PRESSURE t=%lums | %.1f Pa\n", sdp810.ts, sdp810.pressure);
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
  if (tmp117Data.temp > 38.5f)
    Serial.printf("[TMP117] ⚠ HIGH SKIN TEMP t=%lums | %.2fC\n", tmp117Data.ts, tmp117Data.temp);
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
  // SPI déjà initialisé dans setup()
  delay(100);

  // Soft reset
  max30003WriteReg(0x08, 0x000000);
  delay(500);

  // CS sync pulse
  digitalWrite(MAX30003_CS, HIGH); delay(100);
  digitalWrite(MAX30003_CS, LOW);  delay(10);
  digitalWrite(MAX30003_CS, HIGH); delay(100);

  // Verify chip — INFO register
  uint32_t info = max30003ReadReg(0x0F);
  Serial.printf("[MAX30003] INFO=0x%06X\n", info);
  if ((info & 0xF00000) != 0x500000) {
    Serial.printf("[MAX30003] ERROR — chip not responding (INFO=0x%06X)\n", info);
    ecgOk = false;
    return;
  }

  // CNFG_GEN — EN_ECG=1, lead bias 100MΩ sur ECGP+ECGN
  max30003WriteReg(0x10, 0x081007);
  delay(10);

  // CNFG_EMUX — OPENP=0, OPENN=0, POL=1
  max30003WriteReg(0x14, 0x800000);
  delay(10);

  // CNFG_ECG — gain 20V/V, 512Hz, HPF 0.5Hz, LPF 40Hz
  max30003WriteReg(0x15, 0x805000);
  delay(10);

  // CNFG_RTOR1 — R-to-R detection enabled
  max30003WriteReg(0x1D, 0x3FC600);
  delay(10);

  // Vérifier que CNFG_GEN est bien écrit
  uint32_t cnfg_gen = max30003ReadReg(0x10);
  Serial.printf("[MAX30003] CNFG_GEN readback=0x%06X\n", cnfg_gen);

  // Attendre que le PLL soit verrouillé — max 3 secondes
  Serial.print("[MAX30003] Attente PLL...");
  uint32_t pllStart = millis();
  bool pllOk = false;
  while (millis() - pllStart < 3000) {
    uint32_t st = max30003ReadReg(0x01);
    if (!(st & 0x000100)) {
      Serial.println(" OK");
      pllOk = true;
      break;
    }
    delay(50);
  }
  if (!pllOk) {
    Serial.println(" TIMEOUT — PLLINT toujours actif");
    Serial.println("[MAX30003] ⚠ Vérifier fil GPIO26 → FCLK");
  }

  // FIFO reset
  max30003WriteReg(0x0A, 0x000000);
  delay(10);

  // SYNCH — démarrer l'acquisition
  max30003WriteReg(0x09, 0x000000);
  delay(200);

  // Vérifier STATUS final
  uint32_t status = max30003ReadReg(0x01);
  Serial.printf("[MAX30003] STATUS final=0x%06X\n", status);

  ecgOk = true;
  Serial.println("[MAX30003] OK — ECG 512Hz");
}

void readMax30003() {
  if (!ecgOk) return;

  // DEBUG — à retirer après validation
  uint32_t status = max30003ReadReg(0x01);
  uint32_t fifo   = max30003ReadReg(0x21);
  uint8_t  etag   = (fifo >> 3) & 0x07;

  Serial.printf("[ECG DEBUG] STATUS=0x%06X | FIFO=0x%06X | ETAG=%d\n", status, fifo, etag);

  // ETAG=7 : overflow → reset FIFO
  if (etag == 0x07) {
    Serial.println("[MAX30003] FIFO overflow — reset");
    max30003WriteReg(0x0A, 0x000000);
    return;
  }
  // ETAG=6 : FIFO vide, normal
  if (etag == 0x06) return;

  // Extraire valeur ECG 18-bit signée — bits [23:6]
  int32_t raw = (int32_t)(fifo >> 6);
  if (raw & 0x20000) raw |= 0xFFFC0000; // sign extend

  // Conversion en mV
  ecgData.raw   = raw;
  ecgData.mv    = (float)raw * (1.0f / 131072.0f) * 1000.0f;
  ecgData.ts    = millis();
  ecgData.valid = true;
}

// ════════════════════════════════════════════════════
// Summary
// ════════════════════════════════════════════════════
void printSummary() {
  Serial.println("┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄");
  Serial.printf("t=%lums\n", millis());

  if (imuOk && imuData.valid)
    Serial.printf("  IMU    | G:%.2f[%s] rot:%.1f | peak G:%.2f rot:%.1f\n",
                  imuData.totalG, gLevel(imuData.totalG),
                  imuData.totalRot, imuData.peakG, imuData.peakRot);
  else
    Serial.println("  IMU    | [DISCONNECTED]");

  if (scd41.valid)
    Serial.printf("  CO2    | %u ppm [%s] | %.1fC | %.1f%%\n",
                  scd41.co2, co2Level(scd41.co2), scd41.temp, scd41.humidity);
  else
    Serial.println("  CO2    | [NO DATA YET]");

  if (gsrData.valid)
    Serial.printf("  GSR    | raw=%d R=%.1fkΩ delta=%+.0f [%s]\n",
                  gsrData.raw, gsrData.resistance, gsrData.delta, gsrLevel(gsrData.delta));
  else
    Serial.println("  GSR    | [NO DATA YET]");

  if (sdp810.valid)
    Serial.printf("  SDP810 | P:%.2f Pa | T:%.1fC | flow:%.2f\n",
                  sdp810.pressure, sdp810.temp, sdp810.flowRate);
  else
    Serial.println("  SDP810 | [NO DATA YET]");

  if (tmp117Data.valid)
    Serial.printf("  TMP117 | %.2fC\n", tmp117Data.temp);
  else
    Serial.println("  TMP117 | [NO DATA YET]");

  if (ecgOk && ecgData.valid)
    Serial.printf("  ECG    | raw=%ld mv=%.3f\n", ecgData.raw, ecgData.mv);
  else
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

  // ── SPI en premier — avant Wire pour éviter le conflit ──
  pinMode(MAX30003_CS, OUTPUT);
  digitalWrite(MAX30003_CS, HIGH);
  SPI.begin(MAX30003_SCK, MAX30003_MISO, MAX30003_MOSI, MAX30003_CS);
  SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  delay(100);

  // ── Wire ensuite ──
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  delay(1000);

  // ── FCLK 32.768kHz pour MAX30003 ──
  // Un fil doit être branché entre GPIO26 de l'ESP32 et la pin FCLK du CJMCU-30003
  ledcSetup(0, 32768, 1);           // canal 0, 32768Hz, résolution 1 bit
  ledcAttachPin(MAX30003_FCLK, 0);  // GPIO26
  ledcWrite(0, 1);                  // 50% duty cycle
  delay(100);

  Serial.println("\n=== BalaSense V1 — init ===\n");
  initScd41();
  initImu();
  initGsr();
  initSdp810();
  initTmp117();
  initMax30003();
  Serial.println("\n=== Acquisition started ===\n");

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
}

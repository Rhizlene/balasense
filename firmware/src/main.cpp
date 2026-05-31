#include "config.h"
#include "globals.h"
#include "ecg.h"
#include "imu.h"
#include "scd41.h"
#include "sdp810.h"
#include "tmp117.h"
#include "gsr.h"
#include "wifi_mqtt.h"
#include "telemetry.h"

void setup() {
  Serial.begin(115200);

  // SPI avant Wire
  pinMode(MAX30003_CS, OUTPUT); digitalWrite(MAX30003_CS, HIGH);
  SPI.begin(MAX30003_SCK, MAX30003_MISO, MAX30003_MOSI, MAX30003_CS);
  SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  delay(100);

  Wire.begin(I2C_SDA, I2C_SCL); Wire.setClock(400000); delay(1000);

  // FCLK 32.768kHz pour MAX30003
  ledcSetup(0, 32768, 8); ledcAttachPin(MAX30003_FCLK, 0); ledcWrite(0, 128); delay(100);

  Serial.println("\n=== BalaSense V1 — init ===\n");

  // Capteurs d'abord — WiFi après (évite crash Guru Meditation sur I2C)
  initScd41();
  initImu();
  initGsr();
  initSdp810();
  initTmp117();
  initMax30003();

  initWifi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqttReconnect();

  Serial.println("\n=== Acquisition started ===\n");
  printCsvHeader();
  lastImuSuccess = millis();
}

void loop() {
  uint32_t now = millis();

  if (mqttOk) mqtt.loop();
  if (now-lastMqttRecon >= MQTT_RECONNECT_MS && !mqtt.connected()) { lastMqttRecon=now; mqttReconnect(); }

  if (now-lastImuRead    >= IMU_INTERVAL_MS)   { lastImuRead   =now; readImu(); }
  if (now-lastScd41Read  >= SCD41_INTERVAL_MS) { lastScd41Read =now; readScd41(); }
  if (now-lastGsrRead    >= GSR_INTERVAL_MS)   { lastGsrRead   =now; readGsr(); }
  if (now-lastSdp810Read >= SDP810_INTERVAL_MS){ lastSdp810Read=now; readSdp810(); }
  if (now-lastTmp117Read >= TMP117_INTERVAL_MS){ lastTmp117Read=now; readTmp117(); }
  if (now-lastEcgRead    >= ECG_INTERVAL_MS)   { lastEcgRead   =now; readMax30003(); }
  if (now-lastSummary    >= SUMMARY_INTERVAL)  { lastSummary   =now; printSummary(); }
  if (now-lastCsv        >= CSV_INTERVAL_MS)   { lastCsv       =now; computeHrv(); printCsvLine(); }
  if (now-lastMqtt       >= MQTT_INTERVAL_MS)  { lastMqtt      =now; mqttPublishSummary(); }
}

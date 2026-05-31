#include "wifi_mqtt.h"
#include "globals.h"

void initWifi() {
  Serial.printf("[WiFi]  Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t < 10000) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
    Serial.printf(" OK — IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    wifiOk = false;
    Serial.println(" TIMEOUT — running without WiFi");
  }
}

void mqttReconnect() {
  if (!wifiOk || WiFi.status() != WL_CONNECTED) { wifiOk = false; return; }
  if (mqtt.connected()) { mqttOk = true; return; }
  Serial.print("[MQTT]  Connecting...");
  if (mqtt.connect(MQTT_CLIENT)) {
    mqttOk = true;
    Serial.println(" OK");
  } else {
    mqttOk = false;
    Serial.printf(" FAILED (state=%d)\n", mqtt.state());
  }
}

void mqttPublishSummary() {
  if (!mqttOk || !mqtt.connected()) return;
  StaticJsonDocument<512> doc;
  doc["ts"]    = millis();
  doc["hr"]    = round(ecgDerived.hr_bpm * 10) / 10.0;
  doc["rr"]    = ecgDerived.rr_last;
  doc["rmssd"] = round(ecgDerived.rmssd * 10) / 10.0;
  doc["pnn50"] = round(ecgDerived.pnn50 * 10) / 10.0;
  doc["br"]    = round(breathDerived.br_bpm * 10) / 10.0;
  doc["ie"]    = round(breathDerived.ie_ratio * 100) / 100.0;
  doc["apnea"] = breathDerived.apnea ? 1 : 0;
  doc["gsr"]    = round(gsrDerived.conductance_us * 10) / 10.0;
  doc["phasic"] = round(gsrDerived.phasic * 10) / 10.0;
  doc["scr"]    = gsrDerived.scr_per_min;
  doc["tmp"]   = round(tmp117Data.temp * 100) / 100.0;
  doc["dtmp"]  = round(tmp117Derived.dT_dt * 100) / 100.0;
  doc["co2"]   = scd41.co2;
  doc["co2s"]  = round(scd41Derived.co2_slope * 10) / 10.0;
  doc["ah"]    = round(scd41Derived.abs_humidity * 100) / 100.0;
  doc["g"]     = round(imuData.totalG * 1000) / 1000.0;
  doc["glat"]  = round(imuDerived.g_lat * 1000) / 1000.0;
  doc["glong"] = round(imuDerived.g_long * 1000) / 1000.0;
  doc["pitch"] = round(imuDerived.pitch * 10) / 10.0;
  doc["roll"]  = round(imuDerived.roll * 10) / 10.0;
  doc["cerv"]  = round(imuDerived.cervical_cumul * 10) / 10.0;
  doc["jerk"]  = round(imuDerived.jerk * 100) / 100.0;
  char buf[512];
  size_t n = serializeJson(doc, buf);
  mqtt.publish(TOPIC_SUMMARY, buf, n);
}

void mqttPublishEcg(int32_t raw, uint32_t ts) {
  if (!mqttOk || !mqtt.connected()) return;
  char buf[40];
  snprintf(buf, sizeof(buf), "{\"t\":%lu,\"r\":%ld}", ts, raw);
  mqtt.publish(TOPIC_ECG, buf);
}

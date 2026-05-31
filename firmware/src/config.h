#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SensirionI2CScd4x.h>
#include "ICM_20948.h"
#include <Adafruit_TMP117.h>
#include <Adafruit_Sensor.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

// ── WiFi / MQTT ──────────────────────────────────────
#define WIFI_SSID     "BalaSense_Hotspot"
#define WIFI_PASSWORD "balasense2026"
#define MQTT_BROKER   "192.168.43.1"
#define MQTT_PORT     1883
#define MQTT_CLIENT   "balasense_v1"
#define TOPIC_SUMMARY "balasense/summary"
#define TOPIC_ECG     "balasense/ecg"

// ── Pins ─────────────────────────────────────────────
#define I2C_SDA 21
#define I2C_SCL 22
#define GSR_PIN 32
#define MAX30003_CS   15
#define MAX30003_SCK  14
#define MAX30003_MOSI 25
#define MAX30003_MISO 13
#define MAX30003_FCLK 26

// ── SDP810 ───────────────────────────────────────────
#define SDP810_ADDR         0x25
#define SDP810_CMD_START    0x3603
#define SDP810_CMD_STOP     0x3FF9
#define SDP810_SCALE_FACTOR 60.0f

// ── Debug ────────────────────────────────────────────
#define DEBUG_IMU   false
#define DEBUG_SCD41 true

// ── Timing (ms) ──────────────────────────────────────
#define SCD41_INTERVAL_MS  5000u
#define IMU_INTERVAL_MS    10u
#define GSR_INTERVAL_MS    20u
#define SDP810_INTERVAL_MS 40u
#define TMP117_INTERVAL_MS 1000u
#define ECG_INTERVAL_MS    50u
#define SUMMARY_INTERVAL   2000u
#define CSV_INTERVAL_MS    1000u
#define MQTT_INTERVAL_MS   1000u
#define MQTT_RECONNECT_MS  5000u
#define IMU_TIMEOUT_MS     10000u

// ── IMU alert ────────────────────────────────────────
#define IMU_ROT_THRESHOLD  80.0f
#define IMU_G_THRESHOLD    2.5f
#define IMU_ALERT_DURATION 50u

// ════════════════════════════════════════════════════
// Raw data structs
// ════════════════════════════════════════════════════
struct Scd41Data {
  uint16_t co2=0; float temp=0, humidity=0;
  uint32_t ts=0; bool valid=false;
};
struct ImuData {
  float accX=0,accY=0,accZ=0,gyrX=0,gyrY=0,gyrZ=0,magX=0,magY=0,magZ=0;
  float totalG=0,totalRot=0,peakG=0,peakRot=0;
  uint32_t ts=0; bool valid=false;
};
struct GsrData {
  int raw=0; float voltage=0,resistance=0,baseline=0,delta=0;
  uint32_t ts=0; bool valid=false,calibrated=false;
};
struct Sdp810Data {
  float pressure=0,temp=0,flowRate=0;
  uint32_t ts=0; bool valid=false;
};
struct Tmp117Data {
  float temp=0; uint32_t ts=0; bool valid=false;
};
struct EcgData {
  int32_t raw=0; float mv=0;
  uint32_t ts=0; bool valid=false;
};

// ════════════════════════════════════════════════════
// Derived metric structs
// ════════════════════════════════════════════════════
struct EcgDerived {
  uint16_t rr_buf[20]={};
  uint8_t  rr_idx=0,rr_count=0;
  uint32_t last_peak_ts=0,max_decay_ts=0;
  float    running_max=0.1f;
  bool     prev_above=false;
  float    hr_bpm=0,rmssd=0,pnn50=0;
  uint16_t rr_last=0;
  bool     valid=false;
};
struct BreathDerived {
  bool     prev_positive=false;
  uint32_t last_inhale_ts=0,last_exhale_ts=0,inhale_dur=0,exhale_dur=0;
  uint32_t br_intervals[8]={};
  uint8_t  br_idx=0,br_count=0;
  float    br_bpm=0,ie_ratio=1,amplitude=0,peak_inhale=0;
  bool     apnea=false;
};
struct GsrDerived {
  float    tonic_raw=0,conductance_us=0,tonic_us=0,phasic=0;
  bool     initialized=false;
  float    scr_window_cnt=0;
  uint8_t  scr_per_min=0;
  uint32_t scr_window_ts=0;
  bool     prev_above_scr=false;
};
struct ImuDerived {
  float pitch=0,roll=0,g_lat=0,g_long=0,jerk=0,g_prev=0,cervical_cumul=0;
  bool  initialized=false;
};
struct Scd41Derived {
  float    co2_slope=0,abs_humidity=0,dew_point=0;
  uint16_t co2_prev=0;
  uint32_t ts_prev=0;
};
struct Tmp117Derived {
  float    dT_dt=0,delta_boot=0,temp_boot=-999,temp_prev=0;
  uint32_t ts_prev=0;
};

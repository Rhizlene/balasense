#pragma once
#include "config.h"

// ── Sensor instances ─────────────────────────────────
extern SensirionI2CScd4x scd4x;
extern ICM_20948_I2C     imu;
extern Adafruit_TMP117   tmp117;

// ── WiFi / MQTT ──────────────────────────────────────
extern WiFiClient   wifiClient;
extern PubSubClient mqtt;
extern bool wifiOk;
extern bool mqttOk;

// ── Raw data ─────────────────────────────────────────
extern Scd41Data  scd41;
extern ImuData    imuData;
extern GsrData    gsrData;
extern Sdp810Data sdp810;
extern Tmp117Data tmp117Data;
extern EcgData    ecgData;

// ── Derived metrics ───────────────────────────────────
extern EcgDerived    ecgDerived;
extern BreathDerived breathDerived;
extern GsrDerived    gsrDerived;
extern ImuDerived    imuDerived;
extern Scd41Derived  scd41Derived;
extern Tmp117Derived tmp117Derived;

// ── Sensor health ─────────────────────────────────────
extern bool imuOk;
extern bool ecgOk;

// ── Timing ───────────────────────────────────────────
extern uint32_t lastScd41Read, lastImuRead, lastGsrRead, lastSdp810Read;
extern uint32_t lastTmp117Read, lastEcgRead, lastSummary, lastCsv;
extern uint32_t lastMqtt, lastMqttRecon, lastImuPrint, lastImuSuccess;

// ── IMU alert state ───────────────────────────────────
extern uint32_t imuAlertStart;
extern bool     imuInAlert;

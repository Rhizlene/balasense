#include "globals.h"

SensirionI2CScd4x scd4x;
ICM_20948_I2C     imu;
Adafruit_TMP117   tmp117;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
bool wifiOk = false;
bool mqttOk = false;

Scd41Data  scd41;
ImuData    imuData;
GsrData    gsrData;
Sdp810Data sdp810;
Tmp117Data tmp117Data;
EcgData    ecgData;

EcgDerived    ecgDerived;
BreathDerived breathDerived;
GsrDerived    gsrDerived;
ImuDerived    imuDerived;
Scd41Derived  scd41Derived;
Tmp117Derived tmp117Derived;

bool imuOk = false;
bool ecgOk = false;

uint32_t lastScd41Read=0, lastImuRead=0, lastGsrRead=0, lastSdp810Read=0;
uint32_t lastTmp117Read=0, lastEcgRead=0, lastSummary=0, lastCsv=0;
uint32_t lastMqtt=0, lastMqttRecon=0, lastImuPrint=0, lastImuSuccess=0;

uint32_t imuAlertStart = 0;
bool     imuInAlert    = false;

#include "imu.h"
#include "globals.h"

void initImu() {
  imu.begin(Wire, 0);
  if (imu.status != ICM_20948_Stat_Ok) { Serial.println("[IMU]   ERROR"); imuOk=false; return; }
  imuOk=true; lastImuSuccess=millis(); Serial.println("[IMU]   OK — 100Hz");
}

bool recoverImu() {
  Serial.println("[IMU]   Attempting recovery...");
  Wire.end(); delay(50); Wire.begin(I2C_SDA,I2C_SCL); Wire.setClock(400000); delay(50);
  imu.begin(Wire, 0);
  if (imu.status != ICM_20948_Stat_Ok) { Serial.println("[IMU]   Recovery FAILED"); imuOk=false; return false; }
  Serial.println("[IMU]   Recovery OK"); lastImuSuccess=millis(); imuOk=true; return true;
}

void processImuSample() {
  if (!imuData.valid) return;
  const float dt=0.01f, A=0.98f;
  float pa = atan2f(imuData.accY, sqrtf(imuData.accX*imuData.accX+imuData.accZ*imuData.accZ))*180/M_PI;
  float ra = atan2f(-imuData.accX, imuData.accZ)*180/M_PI;
  if (!imuDerived.initialized) {
    imuDerived.pitch=pa; imuDerived.roll=ra; imuDerived.g_prev=imuData.totalG;
    imuDerived.initialized=true; return;
  }
  imuDerived.pitch = A*(imuDerived.pitch+imuData.gyrX*dt)+(1-A)*pa;
  imuDerived.roll  = A*(imuDerived.roll +imuData.gyrY*dt)+(1-A)*ra;
  imuDerived.g_lat  = imuData.accY;
  imuDerived.g_long = imuData.accX;
  imuDerived.jerk   = fabsf(imuData.totalG-imuDerived.g_prev)/dt;
  imuDerived.g_prev = imuData.totalG;
  imuDerived.cervical_cumul += fabsf(imuData.gyrZ)*dt;
}

void checkImuAlert() {
  bool triggered = (imuData.totalG>IMU_G_THRESHOLD || imuData.totalRot>IMU_ROT_THRESHOLD);
  if (triggered) {
    if (!imuInAlert) { imuAlertStart=millis(); imuInAlert=true; }
    else if (millis()-imuAlertStart >= IMU_ALERT_DURATION)
      Serial.printf("[IMU]  ⚠ SUSTAINED t=%lums | G:%.2f lat:%.2f long:%.2f | rot:%.1f | jerk:%.1f g/s | dur:%lums\n",
                    imuData.ts,imuData.totalG,imuDerived.g_lat,imuDerived.g_long,
                    imuData.totalRot,imuDerived.jerk,millis()-imuAlertStart);
  } else { imuInAlert=false; imuAlertStart=0; }
}

void readImu() {
  if (millis()-lastImuSuccess > IMU_TIMEOUT_MS) { imuOk=false; recoverImu(); return; }
  if (!imu.dataReady()) return;
  imu.getAGMT();
  imuData.accX=imu.accX()/1000.0f; imuData.accY=imu.accY()/1000.0f; imuData.accZ=imu.accZ()/1000.0f;
  imuData.gyrX=imu.gyrX(); imuData.gyrY=imu.gyrY(); imuData.gyrZ=imu.gyrZ();
  imuData.magX=imu.magX(); imuData.magY=imu.magY(); imuData.magZ=imu.magZ();
  imuData.totalG  =sqrtf(imuData.accX*imuData.accX+imuData.accY*imuData.accY+imuData.accZ*imuData.accZ);
  imuData.totalRot=sqrtf(imuData.gyrX*imuData.gyrX+imuData.gyrY*imuData.gyrY+imuData.gyrZ*imuData.gyrZ);
  if (imuData.totalG   > imuData.peakG)   imuData.peakG   = imuData.totalG;
  if (imuData.totalRot > imuData.peakRot) imuData.peakRot = imuData.totalRot;
  imuData.ts=millis(); imuData.valid=true; imuOk=true; lastImuSuccess=millis();
  processImuSample(); checkImuAlert();
}

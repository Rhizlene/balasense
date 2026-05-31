#include "ecg.h"
#include "globals.h"
#include "wifi_mqtt.h"

static void max30003WriteReg(uint8_t reg, uint32_t data) {
  digitalWrite(MAX30003_CS,LOW); delayMicroseconds(10);
  SPI.transfer(reg<<1); SPI.transfer((data>>16)&0xFF); SPI.transfer((data>>8)&0xFF); SPI.transfer(data&0xFF);
  delayMicroseconds(10); digitalWrite(MAX30003_CS,HIGH); delayMicroseconds(10);
}

static uint32_t max30003ReadReg(uint8_t reg) {
  uint32_t data=0;
  digitalWrite(MAX30003_CS,LOW); delayMicroseconds(10);
  SPI.transfer((reg<<1)|0x01); delayMicroseconds(10);
  data=(uint32_t)SPI.transfer(0xFF)<<16;
  data|=(uint32_t)SPI.transfer(0xFF)<<8;
  data|=SPI.transfer(0xFF);
  delayMicroseconds(10); digitalWrite(MAX30003_CS,HIGH); delayMicroseconds(10);
  return data;
}

void processEcgSample(float mv, uint32_t ts) {
  float absMv = fabsf(mv);
  if (absMv > ecgDerived.running_max) ecgDerived.running_max = absMv;
  if (ts - ecgDerived.max_decay_ts > 200) {
    ecgDerived.running_max *= 0.99f;
    if (ecgDerived.running_max < 5.0f) ecgDerived.running_max = 5.0f;
    ecgDerived.max_decay_ts = ts;
  }
  bool now_above = absMv > (ecgDerived.running_max * 0.60f);
  if (now_above && !ecgDerived.prev_above) {
    if (ecgDerived.last_peak_ts > 0) {
      uint32_t rr = ts - ecgDerived.last_peak_ts;
      if (rr >= 333 && rr <= 1500) {
        bool plausible = true;
        if (ecgDerived.rr_count > 0) {
          uint16_t prev = ecgDerived.rr_buf[(ecgDerived.rr_idx+19)%20];
          float ratio = (float)rr / (float)prev;
          if (ratio < 0.25f || ratio > 3.0f) plausible = false;
        }
        if (plausible) {
          ecgDerived.rr_buf[ecgDerived.rr_idx] = (uint16_t)rr;
          ecgDerived.rr_idx = (ecgDerived.rr_idx+1) % 20;
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

void computeHrv() {
  uint8_t n = min(ecgDerived.rr_count, (uint8_t)20);
  if (n < 4) return;
  float sum_sq=0; uint8_t nn50=0;
  for (uint8_t i=1; i<n; i++) {
    uint8_t a=(ecgDerived.rr_idx+20-n+i-1)%20;
    uint8_t b=(ecgDerived.rr_idx+20-n+i)%20;
    float diff=(float)ecgDerived.rr_buf[b]-(float)ecgDerived.rr_buf[a];
    sum_sq += diff*diff;
    if (fabsf(diff) > 50.0f) nn50++;
  }
  ecgDerived.rmssd = sqrtf(sum_sq/(n-1));
  ecgDerived.pnn50 = (float)nn50/(n-1)*100.0f;
  ecgDerived.valid = true;
}

void initMax30003() {
  delay(100);
  max30003WriteReg(0x08,0x000000); delay(500);
  digitalWrite(MAX30003_CS,HIGH); delay(100);
  digitalWrite(MAX30003_CS,LOW);  delay(10);
  digitalWrite(MAX30003_CS,HIGH); delay(100);

  uint32_t info = max30003ReadReg(0x0F);
  Serial.printf("[MAX30003] INFO=0x%06X\n", info);
  if ((info&0xF00000) != 0x500000) {
    Serial.println("[MAX30003] ERROR — chip not responding");
    ecgOk = false; return;
  }

  max30003WriteReg(0x10,0x081007); delay(10);
  max30003WriteReg(0x14,0x800000); delay(10);
  max30003WriteReg(0x15,0x805000); delay(10);
  max30003WriteReg(0x1D,0x3FC600); delay(10);

  Serial.printf("[MAX30003] CNFG_GEN readback=0x%06X\n", max30003ReadReg(0x10));
  Serial.print("[MAX30003] Attente PLL...");
  uint32_t s=millis(); bool ok=false;
  while (millis()-s < 10000) {
    if (!(max30003ReadReg(0x01)&0x000100)) { Serial.println(" OK"); ok=true; break; }
    delay(50);
  }
  if (!ok) {
    Serial.println(" TIMEOUT");
    Serial.println("[MAX30003] ⚠ Vérifier fil GPIO26 → FCLK");
  }

  max30003WriteReg(0x0A,0x000000); delay(10);
  max30003WriteReg(0x09,0x000000); delay(200);
  Serial.printf("[MAX30003] STATUS final=0x%06X\n", max30003ReadReg(0x01));
  ecgOk = true; Serial.println("[MAX30003] OK — ECG 512Hz");
}

void readMax30003() {
  if (!ecgOk) return;
  uint32_t now = millis();
  for (int i=0; i<32; i++) {
    uint32_t fifo = max30003ReadReg(0x21);
    uint8_t etag = (fifo>>3)&0x07;
    if (etag==0x07) { max30003WriteReg(0x0A,0x000000); return; }
    if (etag==0x06) break;
    int32_t raw = (int32_t)(fifo>>6);
    if (raw&0x20000) raw|=0xFFFC0000;
    ecgData.raw   = raw;
    ecgData.mv    = (float)raw*(1.0f/131072.0f)*1000.0f;
    ecgData.ts    = now-(ECG_INTERVAL_MS*(32-i)/32);
    ecgData.valid = true;
    processEcgSample(ecgData.mv, ecgData.ts);
    if (mqttOk && mqtt.connected()) mqttPublishEcg(ecgData.raw, ecgData.ts);
    else Serial.printf("ECG,%lu,%ld\n", ecgData.ts, ecgData.raw);
    if (etag==0x02||etag==0x03) break;
  }
}

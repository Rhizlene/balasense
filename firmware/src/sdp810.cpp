#include "sdp810.h"
#include "globals.h"

bool sdp810Crc(uint8_t m, uint8_t l, uint8_t c) {
  uint8_t d[2]={m,l}, x=0xFF;
  for (int i=0; i<2; i++) { x^=d[i]; for (int b=0; b<8; b++) x=(x&0x80)?(x<<1)^0x31:(x<<1); }
  return x==c;
}

void processBreathSample(float pressure, uint32_t ts) {
  bool positive = pressure > 1.0f;
  if (positive && !breathDerived.prev_positive) {
    if (breathDerived.last_exhale_ts > 0)
      breathDerived.exhale_dur = ts - breathDerived.last_exhale_ts;
    if (breathDerived.last_inhale_ts > 0) {
      uint32_t period = ts - breathDerived.last_inhale_ts;
      if (period > 1500 && period < 12000) {
        breathDerived.br_intervals[breathDerived.br_idx] = period;
        breathDerived.br_idx = (breathDerived.br_idx+1)%8;
        if (breathDerived.br_count < 8) breathDerived.br_count++;
        uint32_t sum=0; uint8_t cnt=min(breathDerived.br_count,(uint8_t)5);
        for (uint8_t i=0; i<cnt; i++)
          sum += breathDerived.br_intervals[(breathDerived.br_idx+8-1-i)%8];
        breathDerived.br_bpm = 60000.0f/(sum/cnt);
        if (breathDerived.inhale_dur > 0 && breathDerived.exhale_dur > 0)
          breathDerived.ie_ratio = (float)breathDerived.inhale_dur/breathDerived.exhale_dur;
      }
    }
    breathDerived.last_inhale_ts = ts;
    breathDerived.peak_inhale = 0;
  } else if (!positive && breathDerived.prev_positive) {
    if (breathDerived.last_inhale_ts > 0)
      breathDerived.inhale_dur = ts - breathDerived.last_inhale_ts;
    breathDerived.last_exhale_ts = ts;
    breathDerived.amplitude = breathDerived.peak_inhale;
  }
  if (positive && pressure > breathDerived.peak_inhale) breathDerived.peak_inhale = pressure;
  breathDerived.apnea = breathDerived.last_inhale_ts > 0 && (ts - breathDerived.last_inhale_ts > 10000);
  breathDerived.prev_positive = positive;
}

void initSdp810() {
  Wire.beginTransmission(SDP810_ADDR);
  Wire.write(SDP810_CMD_STOP>>8); Wire.write(SDP810_CMD_STOP&0xFF);
  Wire.endTransmission(); delay(500);
  Wire.beginTransmission(SDP810_ADDR);
  Wire.write(SDP810_CMD_START>>8); Wire.write(SDP810_CMD_START&0xFF);
  if (Wire.endTransmission() != 0) { Serial.println("[SDP810] ERROR"); return; }
  delay(25); Serial.println("[SDP810] OK — 25Hz continuous");
}

void readSdp810() {
  uint8_t buf[9];
  if (Wire.requestFrom((uint8_t)SDP810_ADDR,(uint8_t)9) != 9) {
    Serial.println("[SDP810] Read error — wrong byte count"); return;
  }
  for (int i=0; i<9; i++) buf[i]=Wire.read();
  if (!sdp810Crc(buf[0],buf[1],buf[2]) || !sdp810Crc(buf[3],buf[4],buf[5])) return;
  sdp810.pressure = (int16_t)((buf[0]<<8)|buf[1]) / SDP810_SCALE_FACTOR;
  sdp810.temp     = (int16_t)((buf[3]<<8)|buf[4]) / 200.0f;
  sdp810.flowRate = sdp810.pressure * 0.1f;
  sdp810.ts=millis(); sdp810.valid=true;
  processBreathSample(sdp810.pressure, sdp810.ts);
}

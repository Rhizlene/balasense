#pragma once
#include "config.h"

bool sdp810Crc(uint8_t m, uint8_t l, uint8_t c);
void processBreathSample(float pressure, uint32_t ts);
void initSdp810();
void readSdp810();

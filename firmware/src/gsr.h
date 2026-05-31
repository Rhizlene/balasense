#pragma once
#include "config.h"

void initGsr();
void processGsrSample(int raw, float voltage, uint32_t ts);
void readGsr();

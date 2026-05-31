#pragma once
#include "config.h"

void processEcgSample(float mv, uint32_t ts);
void computeHrv();
void initMax30003();
void readMax30003();

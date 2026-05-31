#pragma once
#include <Arduino.h>

inline const char* co2Level(uint16_t c) {
  if(c>2000)return"CRITICAL"; if(c>1500)return"ALERT"; if(c>1000)return"WARNING"; return"OK";
}
inline const char* gLevel(float g) {
  if(g>4)return"HIGH-G"; if(g>2.5)return"MODERATE-G"; if(g>1.5)return"MOVEMENT"; return"STABLE";
}
inline const char* gsrLevel(float d) {
  if(d>500)return"HIGH-STRESS"; if(d>200)return"ELEVATED"; if(d>-200)return"BASELINE"; return"LOW";
}

void printCsvHeader();
void printCsvLine();
void printSummary();

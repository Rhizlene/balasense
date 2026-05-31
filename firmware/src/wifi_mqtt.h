#pragma once
#include "config.h"

void initWifi();
void mqttReconnect();
void mqttPublishSummary();
void mqttPublishEcg(int32_t raw, uint32_t ts);

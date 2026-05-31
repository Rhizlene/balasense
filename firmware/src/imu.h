#pragma once
#include "config.h"

void initImu();
bool recoverImu();
void processImuSample();
void checkImuAlert();
void readImu();

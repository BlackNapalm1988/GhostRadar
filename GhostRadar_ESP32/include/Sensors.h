#pragma once
#include <Arduino.h>

void Sensors_begin();
bool Sensors_pollLetter(char &outLetter);

float Sensors_getLastTempC();
float Sensors_getLastHumidity();
uint8_t Sensors_getBatteryPercent();
uint8_t Sensors_getWifiStrengthPercent();

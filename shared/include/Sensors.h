#pragma once
#include <Arduino.h>

struct SensorPins {
  int dhtPin;
  int dhtType;
  int i2cSda;
  int i2cScl;
};

void Sensors_configure(const SensorPins& pins);
void Sensors_begin();
bool Sensors_pollLetter(char &outLetter);

float Sensors_getLastTempC();
float Sensors_getLastHumidity();
uint8_t Sensors_getBatteryPercent();
uint8_t Sensors_getWifiStrengthPercent();

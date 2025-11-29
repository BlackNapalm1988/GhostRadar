#pragma once
#include <Arduino.h>

struct WifiEntropy {
  int strongest;
  int weakest;
  float variance;
  int count;
};

extern const unsigned long WIFI_SCAN_INTERVAL_MS;

void WifiRadar_begin();
void WifiRadar_update();
void WifiRadar_draw();
void WifiRadar_forceImmediateScan();
WifiEntropy WifiRadar_getEntropy();
void WifiRadar_startTask();

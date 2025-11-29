#pragma once
#include <Arduino.h>

// UI mode state shared between touch handler and main loop.
enum UiMode {
  UI_MODE_MAIN,
  UI_MODE_SETTINGS
};

struct TouchCalibration {
  int16_t minX;
  int16_t maxX;
  int16_t minY;
  int16_t maxY;
};

void TouchUI_configure(uint8_t csPin, const TouchCalibration& calibration);
void TouchUI_begin();
void TouchUI_update();
UiMode TouchUI_getMode();
void TouchUI_setMode(UiMode mode);

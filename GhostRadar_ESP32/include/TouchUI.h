#pragma once
#include <Arduino.h>

// UI mode state shared between touch handler and main loop.
enum UiMode {
  UI_MODE_MAIN,
  UI_MODE_SETTINGS
};

void TouchUI_begin();
void TouchUI_update();
UiMode TouchUI_getMode();
void TouchUI_setMode(UiMode mode);

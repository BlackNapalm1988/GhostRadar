#include "TouchUI.h"
#include "config_core.h"
#include "Display.h"
#include "Dictionary.h"
#include "Settings.h"
#include "WifiRadar.h"
#include <XPT2046_Touchscreen.h>

static uint8_t touchCsPin = 255;
static TouchCalibration touchCal = { 0, 4095, 0, 4095 };
static XPT2046_Touchscreen* ts = nullptr;
// Tracks whether we are showing the main UI or the settings overlay.
static UiMode currentMode = UI_MODE_MAIN;
static const unsigned long TOUCH_DEBOUNCE_MS = 120;
static const uint16_t TOUCH_PRESSURE_MIN = 180;  // ignore noise / ghost touches
static const uint16_t TOUCH_PRESSURE_MAX = 4000; // sanity cap to discard wild reads
static bool swipeActive = false;
static bool swipeTriggered = false;
static int16_t swipeStartX = 0;
static int16_t swipeStartY = 0;
static const int SWIPE_DISTANCE = 40; // pixels required to trigger settings

// Settings row geometry (keep in sync with Display_drawSettingsScreen)
static const int SETTINGS_ROW_TOP[]    = { 36, 96, 156 };
static const int SETTINGS_ROW_HEIGHT   = 52;

void TouchUI_configure(uint8_t csPin, const TouchCalibration& calibration) {
  touchCsPin = csPin;
  touchCal = calibration;
  if (ts) {
    delete ts;
    ts = nullptr;
  }
  if (touchCsPin != 255) {
    ts = new XPT2046_Touchscreen(touchCsPin);
  }
}

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

UiMode TouchUI_getMode() {
  return currentMode;
}

void TouchUI_setMode(UiMode mode) {
  currentMode = mode;
}

void TouchUI_begin() {
  if (!ts && touchCsPin != 255) {
    ts = new XPT2046_Touchscreen(touchCsPin);
  }
  if (!ts) {
    Serial.println(F("Touch not configured; skipping touch init"));
    return;
  }
  ts->begin();
  ts->setRotation(1);   // adjust if axes are weird
}

static bool handleSwipeGesture(int16_t sx, int16_t sy) {
  if (!swipeActive) {
    swipeActive = true;
    swipeTriggered = false;
    swipeStartX = sx;
    swipeStartY = sy;
    return false;
  }

  if (!swipeTriggered) {
    int16_t dx = sx - swipeStartX;
    int16_t dy = sy - swipeStartY;
    // Look for a right-to-left swipe with limited vertical drift.
    if (dx <= -SWIPE_DISTANCE && abs(dy) < (SWIPE_DISTANCE / 2)) {
      swipeTriggered = true;
      return true;
    }
  }
  return false;
}

static void enterSettings() {
  currentMode = UI_MODE_SETTINGS;
  Display_drawSettingsScreen(Settings_get());
}

static void exitSettings() {
  currentMode = UI_MODE_MAIN;
  Display_drawStaticFrame();
  Display_clearWordArea();
  WifiRadar_forceImmediateScan();
}

static void adjustBrightness(bool increase) {
  DeviceSettings &s = Settings_get();
  int delta = increase ? 1 : -1;
  int newLevel = (int)s.brightnessLevel + delta;
  newLevel = Settings_clampBrightness((uint8_t)newLevel, 3);
  s.brightnessLevel = (uint8_t)newLevel;
  Display_applyBrightness(s.brightnessLevel);
}

static void adjustDictionary(bool increase) {
  DeviceSettings &s = Settings_get();
  uint8_t count = Dictionary_getCount();
  if (count == 0) return;
  int delta = increase ? 1 : -1;
  int newIdx = (int)s.dictionaryIndex + delta;
  if (newIdx < 0) newIdx = 0;
  if (newIdx >= count) newIdx = count - 1;
  if (newIdx == s.dictionaryIndex) return;
  s.dictionaryIndex = (uint8_t)newIdx;
  Dictionary_setActiveIndex(s.dictionaryIndex);
}

static void adjustVariance(bool increase) {
  int delta = increase ? 1 : -1;
  Settings_stepVariance(delta);
}

static void handleSettingsTouch(int16_t sx, int16_t sy) {
  // Three stacked rows; left half decrements, right half increments.
  // Rows tuned for landscape settings layout.
  bool right = sx >= (SCREEN_W / 2);
  if (sy >= SETTINGS_ROW_TOP[0] && sy <= SETTINGS_ROW_TOP[0] + SETTINGS_ROW_HEIGHT) {
    adjustBrightness(right);
  } else if (sy >= SETTINGS_ROW_TOP[1] && sy <= SETTINGS_ROW_TOP[1] + SETTINGS_ROW_HEIGHT) {
    adjustDictionary(right);
  } else if (sy >= SETTINGS_ROW_TOP[2] && sy <= SETTINGS_ROW_TOP[2] + SETTINGS_ROW_HEIGHT) {
    adjustVariance(right);
  } else if (sy >= SCREEN_H - 28) {
    exitSettings();
    return;
  }
  Display_drawSettingsScreen(Settings_get());
}

void TouchUI_update() {
  static unsigned long lastTouchMs = 0;
  unsigned long now = millis();

  if (!ts) return;

  if (!ts->touched()) {
    // Reset swipe state when finger lifts.
    swipeActive = false;
    swipeTriggered = false;
    return;
  }

  TS_Point p = ts->getPoint();
  if (p.z < TOUCH_PRESSURE_MIN || p.z > TOUCH_PRESSURE_MAX) {
    return; // likely noise
  }

  // Map raw → screen (landscape 320x240)
  int16_t sx = map(p.x, touchCal.minX, touchCal.maxX, 0, SCREEN_W);
  int16_t sy = map(p.y, touchCal.maxY, touchCal.minY, 0, SCREEN_H); // inverted Y

  sx = clamp16(sx, 0, SCREEN_W - 1);
  sy = clamp16(sy, 0, SCREEN_H - 1);

  bool swipe = handleSwipeGesture(sx, sy);
  if (swipe) {
    // Right-to-left swipe toggles settings screen.
    if (currentMode == UI_MODE_MAIN) {
      enterSettings();
    } else {
      exitSettings();
    }
    lastTouchMs = now;
    return;
  }

  if (now - lastTouchMs < TOUCH_DEBOUNCE_MS) return;  // debounce
  lastTouchMs = now;

  Serial.print("Touch: raw(");
  Serial.print(p.x);
  Serial.print(",");
  Serial.print(p.y);
  Serial.print(") -> screen(");
  Serial.print(sx);
  Serial.print(",");
  Serial.print(sy);
  Serial.println(")");

  if (currentMode == UI_MODE_SETTINGS) {
    handleSettingsTouch(sx, sy);
    return;
  }

  // Radar area: force WiFi rescan
  const DisplayLayout& layout = Display_getLayout();
  if (sx >= layout.radarX && sx <= layout.radarX + layout.radarW &&
      sy >= layout.radarY && sy <= layout.radarY + layout.radarH) {
    WifiRadar_forceImmediateScan();
  }
}

#include "Settings.h"
#include <math.h>

static DeviceSettings settings;
static const float VARIANCE_STEPS[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
static const uint8_t VARIANCE_STEP_COUNT =
    sizeof(VARIANCE_STEPS) / sizeof(VARIANCE_STEPS[0]);

DeviceSettings &Settings_get() { return settings; }

ComplicationType Settings_parseComplicationType(const String &s) {
  if (s.equalsIgnoreCase("off") || s.equalsIgnoreCase("none"))
    return ComplicationType::None;
  if (s.equalsIgnoreCase("temperature_c"))
    return ComplicationType::TemperatureC;
  if (s.equalsIgnoreCase("humidity_percent"))
    return ComplicationType::HumidityPercent;
  if (s.equalsIgnoreCase("battery_percent"))
    return ComplicationType::BatteryPercent;
  if (s.equalsIgnoreCase("wifi_strength_percent"))
    return ComplicationType::WifiStrengthPercent;
  return ComplicationType::None;
}

const char *Settings_complicationTypeToString(ComplicationType type) {
  switch (type) {
  case ComplicationType::TemperatureC:
    return "temperature_c";
  case ComplicationType::HumidityPercent:
    return "humidity_percent";
  case ComplicationType::BatteryPercent:
    return "battery_percent";
  case ComplicationType::WifiStrengthPercent:
    return "wifi_strength_percent";
  case ComplicationType::None:
  default:
    break;
  }
  return "off";
}

void Settings_loadDefaults() {
  settings.brightnessLevel = 2; // medium
  settings.dictionaryIndex = 0; // first word list
  settings.varianceScale = 1.0f;
  settings.heartbeatBpm = 120; // base heartbeat speed
  settings.loggingEnabled = true;
  settings.loggingLevel = LOG_LEVEL_INFO;

  settings.ui.topLeft.type = ComplicationType::TemperatureC;
  settings.ui.topLeft.label = "T";
  settings.ui.topRight.type = ComplicationType::HumidityPercent;
  settings.ui.topRight.label = "H";
  settings.ui.bottomLeft.type = ComplicationType::BatteryPercent;
  settings.ui.bottomLeft.label = "BAT";
  settings.ui.bottomRight.type = ComplicationType::WifiStrengthPercent;
  settings.ui.bottomRight.label = "WiFi";
}

float Settings_clampVariance(float v) {
  if (v < VARIANCE_STEPS[0])
    return VARIANCE_STEPS[0];
  if (v > VARIANCE_STEPS[VARIANCE_STEP_COUNT - 1])
    return VARIANCE_STEPS[VARIANCE_STEP_COUNT - 1];
  return v;
}

uint8_t Settings_clampBrightness(uint8_t level, uint8_t maxLevel) {
  if (level > maxLevel)
    return maxLevel;
  return level;
}

uint8_t Settings_clampDictionaryIndex(uint8_t idx, uint8_t maxIndex) {
  if (idx > maxIndex)
    return maxIndex;
  return idx;
}

uint16_t Settings_clampHeartbeat(uint16_t bpm, uint16_t minBpm,
                                 uint16_t maxBpm) {
  if (bpm < minBpm)
    return minBpm;
  if (bpm > maxBpm)
    return maxBpm;
  return bpm;
}

LoggingLevel Settings_parseLoggingLevel(const String &s) {
  if (s.equalsIgnoreCase("debug"))
    return LOG_LEVEL_DEBUG;
  if (s.equalsIgnoreCase("warn") || s.equalsIgnoreCase("warning"))
    return LOG_LEVEL_WARN;
  if (s.equalsIgnoreCase("error"))
    return LOG_LEVEL_ERROR;
  return LOG_LEVEL_INFO;
}

const char *Settings_loggingLevelToString(LoggingLevel level) {
  switch (level) {
  case LOG_LEVEL_ERROR:
    return "error";
  case LOG_LEVEL_WARN:
    return "warn";
  case LOG_LEVEL_INFO:
    return "info";
  case LOG_LEVEL_DEBUG:
    return "debug";
  }
  return "info";
}

static uint8_t nearestVarianceIndex(float v) {
  uint8_t bestIdx = 0;
  float bestDiff = fabsf(VARIANCE_STEPS[0] - v);
  for (uint8_t i = 1; i < VARIANCE_STEP_COUNT; i++) {
    float d = fabsf(VARIANCE_STEPS[i] - v);
    if (d < bestDiff) {
      bestDiff = d;
      bestIdx = i;
    }
  }
  return bestIdx;
}

float Settings_stepVariance(int8_t direction) {
  uint8_t idx = nearestVarianceIndex(settings.varianceScale);
  int next = (int)idx + (int)direction;
  if (next < 0)
    next = 0;
  if (next >= VARIANCE_STEP_COUNT)
    next = VARIANCE_STEP_COUNT - 1;
  settings.varianceScale = VARIANCE_STEPS[next];
  return settings.varianceScale;
}

const ComplicationConfig &Settings_getTopLeftComplication() {
  return settings.ui.topLeft;
}

const ComplicationConfig &Settings_getTopRightComplication() {
  return settings.ui.topRight;
}

const ComplicationConfig &Settings_getBottomLeftComplication() {
  return settings.ui.bottomLeft;
}

const ComplicationConfig &Settings_getBottomRightComplication() {
  return settings.ui.bottomRight;
}

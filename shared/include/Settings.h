#pragma once
#include <Arduino.h>

enum class ComplicationType : uint8_t {
  None = 0,
  TemperatureC,
  HumidityPercent,
  BatteryPercent,
  WifiStrengthPercent
};

struct ComplicationConfig {
  ComplicationType type = ComplicationType::None;
  String label;
};

struct UiSettings {
  ComplicationConfig topLeft;
  ComplicationConfig topRight;
  ComplicationConfig bottomLeft;
  ComplicationConfig bottomRight;
};

// Global settings shared across modules.
struct DeviceSettings {
  uint8_t brightnessLevel;  // 0..3
  uint8_t dictionaryIndex;  // active word list
  float   varianceScale;    // entropy scaling factor
  uint16_t heartbeatBpm;    // heartbeat animation speed
  bool loggingEnabled;
  uint8_t loggingLevel;     // see LoggingLevel enum
  UiSettings ui;
};

enum LoggingLevel : uint8_t {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_WARN,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
};

DeviceSettings& Settings_get();
void Settings_loadDefaults();

// Step variance along predefined safe stops (0.5 -> 2.0), returns new value.
float Settings_stepVariance(int8_t direction);
float Settings_clampVariance(float v);
uint8_t Settings_clampBrightness(uint8_t level, uint8_t maxLevel = 3);
uint8_t Settings_clampDictionaryIndex(uint8_t idx, uint8_t maxIndex);
uint16_t Settings_clampHeartbeat(uint16_t bpm, uint16_t minBpm = 40, uint16_t maxBpm = 240);
LoggingLevel Settings_parseLoggingLevel(const String& s);
const char* Settings_loggingLevelToString(LoggingLevel level);
ComplicationType Settings_parseComplicationType(const String& s);
const char* Settings_complicationTypeToString(ComplicationType type);

const ComplicationConfig& Settings_getTopLeftComplication();
const ComplicationConfig& Settings_getTopRightComplication();
const ComplicationConfig& Settings_getBottomLeftComplication();
const ComplicationConfig& Settings_getBottomRightComplication();

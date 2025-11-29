#include "SDManager.h"
#include "Display.h"
#include "Settings.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <time.h>

namespace {
const char *CONFIG_DIR = "/config";
const char *LOGS_DIR = "/logs";
const char *SESSIONS_DIR = "/logs/sessions";
const char *DICTIONARY_DIR = "/dictionary";
const char *UI_DIR = "/ui";

const char *SYSTEM_CONFIG_PATH = "/config/system.json";
const char *EVENT_LOG_PATH = "/logs/events.log";

bool sdAvailable = false;
bool loggingActive = false;
File sessionFile;
String currentSessionPath;

bool ensureDir(const char *path) {
  if (SD.exists(path))
    return true;
  bool ok = SD.mkdir(path);
  if (!ok) {
    Serial.print(F("Failed to create dir: "));
    Serial.println(path);
  }
  return ok;
}

String formatTimestamp() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  if (now > 0 && localtime_r(&now, &timeinfo)) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d-%02d-%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buf);
  }

  unsigned long ms = millis();
  char fallback[28];
  snprintf(fallback, sizeof(fallback), "0000-00-00T00-00-%lu",
           (unsigned long)(ms / 1000UL));
  return String(fallback);
}

String sessionFilename() {
  String ts = formatTimestamp();
  ts.replace(":", "-");
  return String(SESSIONS_DIR) + "/" + ts + ".csv";
}

LoggingLevel currentLoggingLevel() {
  return (LoggingLevel)Settings_get().loggingLevel;
}

bool levelAllows(LoggingLevel messageLevel) {
  return (int)messageLevel <= (int)currentLoggingLevel();
}

void writeComplication(JsonObject obj, const ComplicationConfig &cfg) {
  obj["type"] = Settings_complicationTypeToString(cfg.type);
  obj["label"] = cfg.label;
}

void applyComplication(JsonVariant src, ComplicationConfig &target) {
  if (!src.is<JsonObject>())
    return;
  JsonObject obj = src.as<JsonObject>();
  const char *typeStr = obj["type"] | nullptr;
  if (typeStr) {
    target.type = Settings_parseComplicationType(String(typeStr));
  }
  const char *labelStr = obj["label"] | nullptr;
  if (labelStr != nullptr) {
    target.label = String(labelStr);
  }
}
} // namespace

namespace SDManager {
bool begin(SPIClass &bus, int csPin) {
  sdAvailable = SD.begin(csPin, bus);
  if (!sdAvailable) {
    Serial.println(F("SD init failed!"));
  } else {
    Serial.println(F("SD init OK."));
  }
  loggingActive = Settings_get().loggingEnabled;
  // Update UI indicators with fresh SD/logging state.
  Display_setSdPresent(sdAvailable);
  Display_setLoggingEnabled(loggingActive);
  return sdAvailable;
}

bool available() { return sdAvailable; }

void ensureDirectories() {
  if (!sdAvailable)
    return;
  const char *dirs[] = {CONFIG_DIR, LOGS_DIR, SESSIONS_DIR, DICTIONARY_DIR,
                        UI_DIR};
  for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
    ensureDir(dirs[i]);
  }
}

bool saveDefaultSystemConfigIfMissing() {
  if (!sdAvailable)
    return false;
  if (SD.exists(SYSTEM_CONFIG_PATH))
    return true;

  ensureDir(CONFIG_DIR);

  DeviceSettings &s = Settings_get();
  DynamicJsonDocument doc(768);
  doc["brightness"] = s.brightnessLevel;
  doc["heartbeat_speed"] = s.heartbeatBpm;
  doc["language"] = "en";
  JsonObject logging = doc["logging"].to<JsonObject>();
  logging["enabled"] = s.loggingEnabled;
  logging["level"] =
      Settings_loggingLevelToString((LoggingLevel)s.loggingLevel);

  JsonObject ui = doc["ui"].to<JsonObject>();
  JsonObject complications = ui["complications"].to<JsonObject>();
  writeComplication(complications["top_left"].to<JsonObject>(), s.ui.topLeft);
  writeComplication(complications["top_right"].to<JsonObject>(), s.ui.topRight);
  writeComplication(complications["bottom_left"].to<JsonObject>(),
                    s.ui.bottomLeft);
  writeComplication(complications["bottom_right"].to<JsonObject>(),
                    s.ui.bottomRight);

  File f = SD.open(SYSTEM_CONFIG_PATH, FILE_WRITE);
  if (!f) {
    Serial.println(F("Failed to create default system.json"));
    return false;
  }
  serializeJsonPretty(doc, f);
  f.close();
  return true;
}

bool loadSystemConfig() {
  if (!sdAvailable)
    return false;

  if (!SD.exists(SYSTEM_CONFIG_PATH)) {
    Serial.println(F("system.json missing; using defaults"));
    Settings_loadDefaults();
    loggingActive = Settings_get().loggingEnabled;
    Display_setLoggingEnabled(loggingActive);
    return false;
  }

  File f = SD.open(SYSTEM_CONFIG_PATH, FILE_READ);
  if (!f) {
    Serial.println(F("Failed to open system.json; using defaults"));
    Settings_loadDefaults();
    loggingActive = Settings_get().loggingEnabled;
    Display_setLoggingEnabled(loggingActive);
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.print(F("system.json parse error: "));
    Serial.println(err.c_str());
    Settings_loadDefaults();
    loggingActive = Settings_get().loggingEnabled;
    Display_setLoggingEnabled(loggingActive);
    return false;
  }

  DeviceSettings &s = Settings_get();
  s.brightnessLevel =
      Settings_clampBrightness(doc["brightness"] | s.brightnessLevel, 3);
  s.heartbeatBpm =
      Settings_clampHeartbeat(doc["heartbeat_speed"] | s.heartbeatBpm);

  JsonVariant logging = doc["logging"];
  if (!logging.isNull()) {
    s.loggingEnabled = logging["enabled"] | s.loggingEnabled;
    const char *lvl = logging["level"] | Settings_loggingLevelToString(
                                             (LoggingLevel)s.loggingLevel);
    s.loggingLevel = Settings_parseLoggingLevel(String(lvl));
  }

  JsonVariant ui = doc["ui"];
  if (!ui.isNull()) {
    JsonVariant complications = ui["complications"];
    if (complications.is<JsonObject>()) {
      JsonObject compObj = complications.as<JsonObject>();
      applyComplication(compObj["top_left"], s.ui.topLeft);
      applyComplication(compObj["top_right"], s.ui.topRight);
      applyComplication(compObj["bottom_left"], s.ui.bottomLeft);
      applyComplication(compObj["bottom_right"], s.ui.bottomRight);
    }
  }

  Display_applyBrightness(s.brightnessLevel);
  Display_setHeartbeatBpm(s.heartbeatBpm);

  loggingActive = s.loggingEnabled;
  Display_setLoggingEnabled(loggingActive);
  return true;
}

void logEvent(const String &line) {
  if (!sdAvailable)
    return;
  if (!Settings_get().loggingEnabled)
    return;
  if (!levelAllows(LOG_LEVEL_INFO))
    return;

  ensureDir(LOGS_DIR);
  File f = SD.open(EVENT_LOG_PATH, FILE_WRITE);
  if (!f) {
    Serial.println(F("Failed to open event log"));
    return;
  }
  f.seek(f.size());
  String ts = formatTimestamp();
  f.print(ts);
  f.print(" ");
  f.println(line);
  f.close();
  Display_notifySdActivity();
}

void startSessionLog() {
  if (!sdAvailable)
    return;
  loggingActive = Settings_get().loggingEnabled;
  if (sessionFile) {
    sessionFile.close();
  }
  if (!loggingActive)
    return;

  ensureDir(SESSIONS_DIR);

  currentSessionPath = sessionFilename();
  sessionFile = SD.open(currentSessionPath.c_str(), FILE_WRITE);
  if (!sessionFile) {
    Serial.println(F("Failed to open session log"));
    return;
  }
  sessionFile.println(F("timestamp,source,type,value,extra"));
  sessionFile.flush();
  Display_setLoggingEnabled(loggingActive);
}

void logSessionLine(const String &line) {
  if (!sdAvailable || !loggingActive)
    return;
  if (!sessionFile)
    return;

  String ts = formatTimestamp();
  sessionFile.print(ts);
  sessionFile.print(",");
  sessionFile.println(line);
  sessionFile.flush();
  Display_notifySdActivity();
}

void endSessionLog() {
  if (sessionFile) {
    sessionFile.close();
  }
}
} // namespace SDManager

#pragma once
#include <Arduino.h>
#include <SPI.h>

namespace SDManager {
  bool begin(SPIClass &bus, int csPin);

  void ensureDirectories();

  bool loadSystemConfig();
  bool saveDefaultSystemConfigIfMissing();

  void logEvent(const String &line);
  void startSessionLog();
  void logSessionLine(const String &line);
  void endSessionLog();

  bool available();
}

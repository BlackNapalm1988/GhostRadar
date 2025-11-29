#include <Arduino.h>
#include <SPI.h>
#include "BoardConfig.h"
#include "config_core.h"
#include "Display.h"
#include "Sensors.h"
#include "Dictionary.h"
#include "WifiRadar.h"
#include "TouchUI.h"
#include "Settings.h"
#include "SDManager.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println(F("=== GhostRadar ESP32 boot ==="));
  Serial.println(Board_getName());

  Settings_loadDefaults();

  Board_initPins();
  Board_initDisplay();
  Board_initTouch();
  Board_initSensors();

  SDManager::begin(Board_getSdSpi(), Board_getSdCsPin());
  SDManager::ensureDirectories();
  SDManager::saveDefaultSystemConfigIfMissing();
  SDManager::loadSystemConfig();

  // Now init the rest of the system
  Display_begin();
  Display_applyBrightness(Settings_get().brightnessLevel);
  Display_drawStaticFrame();

  TouchUI_begin();
  Sensors_begin();
  Dictionary_begin();
  WifiRadar_begin();
  WifiRadar_startTask();

  SDManager::startSessionLog();
  SDManager::logEvent("Boot complete");
}


void loop() {
  // Touch UI
  TouchUI_update();
  UiMode mode = TouchUI_getMode();
  if (mode == UI_MODE_SETTINGS) {
    return;
  }

  // Sensors -> letter
  char newLetter = 0;
  bool gotLetter = Sensors_pollLetter(newLetter);
  if (gotLetter) {
    Display_updateStatusBar(Sensors_getLastTempC(), Sensors_getLastHumidity());

    Dictionary_appendLetter(newLetter);
    String sensorExtra = "temp=" + String(Sensors_getLastTempC(), 1) + ";hum=" + String(Sensors_getLastHumidity(), 1);
    SDManager::logSessionLine("sensors,letter," + String(newLetter) + "," + sensorExtra);

    String hit;
    if (Dictionary_checkForWord(hit)) {
      Display_displayWord(hit);
      const char* dictName = Dictionary_getActiveName();
      String dictLabel = dictName ? String(dictName) : String("unknown");
      SDManager::logSessionLine("dictionary,word," + hit + ",dict=" + dictLabel);
    }
  }

  // Heartbeat tick each loop; show letter or dash if no letter this cycle.
  Display_heartbeatStep(gotLetter ? newLetter : 0);
  // Animate heartbeat strip
  Display_updateHeartbeat();

  // WiFi radar
  WifiRadar_draw();
}

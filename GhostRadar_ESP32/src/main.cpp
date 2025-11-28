#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "Display.h"
#include "Sensors.h"
#include "Dictionary.h"
#include "WifiRadar.h"
#include "TouchUI.h"
#include "Settings.h"
#include "SDManager.h"

// Create SPI bus objects
SPIClass hspi(HSPI);  // For SD card (separate bus)

// VSPI pins
static const int VSPI_SCLK = 18;
static const int VSPI_MISO = 19;
static const int VSPI_MOSI = 23;
// static const int LCD_CS    = 5;
// static const int TOUCH_CS  = 22;

// HSPI pins
// static const int HSPI_SCLK = 14;
// static const int HSPI_MISO = 12;
// static const int HSPI_MOSI = 13;
// static const int SD_CS     = 15;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println(F("=== GhostRadar ESP32 boot ==="));

  Settings_loadDefaults();

  // Init UI bus on default VSPI (used by TFT + touch)
  SPI.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, LCD_CS);
  pinMode(LCD_CS, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  // Init SD bus
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  pinMode(HSPI_CS, OUTPUT);
  digitalWrite(HSPI_CS, HIGH);

  SDManager::begin(hspi, HSPI_CS);
  SDManager::ensureDirectories();
  SDManager::saveDefaultSystemConfigIfMissing();
  SDManager::loadSystemConfig();

  // Now init the rest of the system
  Display_begin();  // we'll adjust this next
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

#pragma once
#include <SPI.h>
#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include "Settings.h"

struct DisplayLayout {
  int headerX, headerY, headerW, headerH;
  int mainX, mainY, mainW, mainH;
  int heartbeatX, heartbeatY, heartbeatW, heartbeatH;
  int moduleLeftW;
  int moduleRightW;
  int radarX, radarY, radarW, radarH;
  int radarCenterX, radarCenterY, radarRadius;
};

void Display_begin();
void Display_drawStaticFrame();
void Display_updateStatusBar(float temp, float humidity);
void Display_displayLetter(char l);
void Display_displayWord(const String& w);
void Display_clearWordArea();
void Display_setNightMode(bool enabled);
void Display_setSdPresent(bool present);
void Display_setLoggingEnabled(bool enabled);
void Display_setWifiActive(bool active);
void Display_notifySdActivity();
void Display_notifyWifiScan();
void Display_drawSettingsScreen(const DeviceSettings& s);
void Display_applyBrightness(uint8_t level);
void Display_setHeartbeatBpm(uint16_t bpm);
uint8_t Display_getBrightnessLevel();
uint16_t Display_dimColor(uint16_t color);
const DisplayLayout& Display_getLayout();

// Word overlay drawn over radar
void Display_setOverlayWord(const String& w);
void Display_drawOverlayWord();

// Heartbeat scroller
// Heartbeat scroller: pass letter ('A'..'Z') or 0 for dash cycle; call once per loop.
void Display_heartbeatStep(char letterOrZero);
void Display_updateHeartbeat();

// Module placeholders
bool ModuleLeft_enabled();
void ModuleLeft_draw(Adafruit_ILI9341 &tft, int x, int y, int w, int h);
bool ModuleRight_enabled();
void ModuleRight_draw(Adafruit_ILI9341 &tft, int x, int y, int w, int h);

// Expose TFT so WifiRadar can draw on it
Adafruit_ILI9341& Display_tft();

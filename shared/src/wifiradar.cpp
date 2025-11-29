#include "WifiRadar.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "Display.h"
#include "config_core.h"
#include "Sensors.h"
#include "Settings.h"
#include <Adafruit_GFX.h>

// WiFi radar scans now run on a background FreeRTOS task pinned to core 0 so
// the main UI loop on core 1 stays responsive.

const unsigned long WIFI_SCAN_INTERVAL_MS = 5000;

static const int WIFI_MAX_AP = 20;
static int wifiApCount = 0;
static int wifiRssiList[WIFI_MAX_AP];
static int wifiChannelList[WIFI_MAX_AP];

static WifiEntropy wifiEnt = { -100, -100, 0.0f, 0 };
static unsigned long lastWifiScanMs = 0;
static bool wifiRadarDirty = false;
static bool wifiScanInProgress = false;
static volatile bool wifiForceImmediateScan = false;

static TaskHandle_t wifiScanTaskHandle = nullptr;
static SemaphoreHandle_t wifiDataMutex = nullptr;

static const UBaseType_t WIFI_SCAN_TASK_PRIORITY = 1;
static const uint32_t WIFI_SCAN_TASK_STACK_SIZE = 4096;
static const BaseType_t WIFI_SCAN_TASK_CORE = 0;
static const TickType_t WIFI_SCAN_TASK_DELAY = pdMS_TO_TICKS(100);

// Off-screen buffer for flicker-free radar drawing
static GFXcanvas16 radarCanvas(RADAR_W, RADAR_H);
static GFXcanvas16 radarStatic(RADAR_W, RADAR_H);
static bool radarStaticReady = false;
static bool radarReady() {
  return radarCanvas.getBuffer() != nullptr;
}
static float sweepAngle = 0.0f; // radians, animated

// Cached data for continuous redraw even when no new scan arrives.
static int cachedApCount = 0;
static int cachedRssi[WIFI_MAX_AP];
static int cachedChannel[WIFI_MAX_AP];
static bool cachedValid = false;
static unsigned long lastRadarDrawMs = 0;
static const unsigned long RADAR_FRAME_INTERVAL_MS = 40; // ~25 fps cap

static float clampFloat(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static float mapFloat(float x, float in_min, float in_max,
                      float out_min, float out_max, bool clampResult = true) {
  if (in_max - in_min == 0) return out_min;
  float v = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  if (clampResult) return clampFloat(v, out_min, out_max);
  return v;
}

enum class TextAlign {
  LEFT,
  CENTER,
  RIGHT
};

static void drawSmallText(GFXcanvas16 &gfx, const String& text, int anchorX, int anchorY, TextAlign align) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx.setFont();
  gfx.setTextSize(1);
  gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int textX = anchorX;
  if (align == TextAlign::RIGHT) textX -= w;
  else if (align == TextAlign::CENTER) textX -= w / 2;

  int textY = (anchorY > (gfx.height() / 2)) ? (anchorY - (int)h) : anchorY;

  if (textX < 0) textX = 0;
  if (textX + (int)w > gfx.width()) textX = max(0, gfx.width() - (int)w);
  if (textY < 0) textY = 0;

  gfx.setTextColor(Display_dimColor(0xC618));
  gfx.setCursor(textX, textY);
  gfx.print(text);
}

static void drawComplication(const ComplicationConfig& cfg, int anchorX, int anchorY, TextAlign align) {
  if (cfg.type == ComplicationType::None) return;

  String value;
  switch (cfg.type) {
    case ComplicationType::TemperatureC:
      value = String((int)Sensors_getLastTempC()) + "C";
      break;
    case ComplicationType::HumidityPercent:
      value = String((int)Sensors_getLastHumidity()) + "%";
      break;
    case ComplicationType::BatteryPercent:
      value = String(Sensors_getBatteryPercent()) + "%";
      break;
    case ComplicationType::WifiStrengthPercent:
      value = String(Sensors_getWifiStrengthPercent()) + "%";
      break;
    case ComplicationType::None:
    default:
      return;
  }

  if (cfg.label.length() > 0) {
    value = cfg.label + " " + value;
  }

  drawSmallText(radarCanvas, value, anchorX, anchorY, align);
}

void WifiRadar_begin() {
  if (!wifiDataMutex) {
    wifiDataMutex = xSemaphoreCreateMutex();
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  lastWifiScanMs = millis() - WIFI_SCAN_INTERVAL_MS;  // force initial scan
  wifiScanInProgress = false;
  wifiForceImmediateScan = false;
  Display_setWifiActive(false);
}

void WifiRadar_forceImmediateScan() {
  wifiForceImmediateScan = true;
}

WifiEntropy WifiRadar_getEntropy() {
  WifiEntropy copy;
  if (wifiDataMutex && xSemaphoreTake(wifiDataMutex, portMAX_DELAY) == pdTRUE) {
    copy = wifiEnt;
    xSemaphoreGive(wifiDataMutex);
  } else {
    copy = wifiEnt;
  }
  return copy;
}

void WifiRadar_update() {
  // Check if an async scan finished
  int scanStatus = WiFi.scanComplete();
  if (scanStatus == WIFI_SCAN_FAILED) {
    if (wifiDataMutex && xSemaphoreTake(wifiDataMutex, portMAX_DELAY) == pdTRUE) {
      wifiScanInProgress = false;
      wifiApCount = 0;
      wifiEnt.count = 0;
      wifiEnt.strongest = -100;
      wifiEnt.weakest = -100;
      wifiEnt.variance = 0.0f;
      wifiRadarDirty = true;
      Display_setWifiActive(false);
      xSemaphoreGive(wifiDataMutex);
    }
  } else if (scanStatus >= 0 && wifiScanInProgress) {
    int n = scanStatus;
    if (wifiDataMutex && xSemaphoreTake(wifiDataMutex, portMAX_DELAY) == pdTRUE) {
      wifiScanInProgress = false;
      wifiApCount = 0;

      if (n > WIFI_MAX_AP) n = WIFI_MAX_AP;
      wifiEnt.count = n;

      if (n == 0) {
        wifiEnt.strongest = -100;
        wifiEnt.weakest = -100;
        wifiEnt.variance = 0.0f;
        wifiRadarDirty = true;
        WiFi.scanDelete();
      } else {
        long sum = 0;
        int strongest = -200;
        int weakest = 0;

        for (int i = 0; i < n; i++) {
          int r = WiFi.RSSI(i);
          int ch = WiFi.channel(i);
          wifiRssiList[i] = r;
          wifiChannelList[i] = ch;
          wifiApCount++;

          if (i == 0) weakest = r;
          else if (r < weakest) weakest = r;
          if (r > strongest) strongest = r;

          sum += r;
        }

        float avg = (float)sum / n;
        float varSum = 0.0f;
        for (int i = 0; i < n; i++) {
          float d = (float)wifiRssiList[i] - avg;
          varSum += d * d;
        }
        float variance = (n > 1) ? varSum / (n - 1) : 0.0f;

        wifiEnt.strongest = strongest;
        wifiEnt.weakest   = weakest;
        wifiEnt.variance  = variance;

        wifiRadarDirty = true;
        WiFi.scanDelete();  // free scan results
      }
      Display_setWifiActive(false);
      xSemaphoreGive(wifiDataMutex);
    }
  }

  unsigned long now = millis();
  bool startScan = false;

  if (wifiDataMutex && xSemaphoreTake(wifiDataMutex, portMAX_DELAY) == pdTRUE) {
    if (!wifiScanInProgress &&
        (wifiForceImmediateScan || now - lastWifiScanMs >= WIFI_SCAN_INTERVAL_MS)) {
      wifiScanInProgress = true;
      wifiForceImmediateScan = false;
      lastWifiScanMs = now;
      startScan = true;
      Display_setWifiActive(true);
      Display_notifyWifiScan();
    }
    xSemaphoreGive(wifiDataMutex);
  }

  if (startScan) {
    WiFi.scanNetworks(true, true);  // async, show hidden
  }
}

static void WifiRadar_scanTask(void *parameter) {
  for (;;) {
    WifiRadar_update();
    vTaskDelay(WIFI_SCAN_TASK_DELAY);
  }
}

void WifiRadar_startTask() {
  if (wifiScanTaskHandle != nullptr) return;
  xTaskCreatePinnedToCore(
    WifiRadar_scanTask,
    "wifiRadarScan",
    WIFI_SCAN_TASK_STACK_SIZE,
    nullptr,
    WIFI_SCAN_TASK_PRIORITY,
    &wifiScanTaskHandle,
    WIFI_SCAN_TASK_CORE);
}

void WifiRadar_draw() {
  unsigned long now = millis();

  int apCountCopy = cachedApCount;
  int rssiCopy[WIFI_MAX_AP];
  int channelCopy[WIFI_MAX_AP];
  bool hasData = cachedValid;

  // Update cache when new scan data is ready
  if (wifiDataMutex && xSemaphoreTake(wifiDataMutex, portMAX_DELAY) == pdTRUE) {
    if (wifiRadarDirty) {
      wifiRadarDirty = false;
      cachedApCount = wifiApCount;
      for (int i = 0; i < WIFI_MAX_AP; i++) {
        cachedRssi[i] = wifiRssiList[i];
        cachedChannel[i] = wifiChannelList[i];
      }
      cachedValid = true;
      apCountCopy = cachedApCount;
      hasData = true;
    }
    xSemaphoreGive(wifiDataMutex);
  }
  // Use cached arrays for rendering
  if (hasData) {
    for (int i = 0; i < WIFI_MAX_AP; i++) {
      rssiCopy[i] = cachedRssi[i];
      channelCopy[i] = cachedChannel[i];
    }
  } else {
    apCountCopy = 0;
  }

  Adafruit_ILI9341 &tft = Display_tft();
  const DisplayLayout& layout = Display_getLayout();

  if (!radarReady()) {
    tft.fillRect(layout.radarX, layout.radarY, layout.radarW, layout.radarH, Display_dimColor(0x0000));
    tft.setFont();
    tft.setTextColor(Display_dimColor(0x8410));
    tft.setCursor(layout.radarX + 4, layout.radarY + 12);
    tft.print("RADAR MEM");
    tft.setCursor(layout.radarX + 4, layout.radarY + 24);
    tft.print("UNAVAILABLE");
    return;
  }

  // Frame cap unless new scan data arrived; still allow overlay refresh.
  bool shouldDraw = hasData || (now - lastRadarDrawMs) >= RADAR_FRAME_INTERVAL_MS;
  if (!shouldDraw) {
    Display_drawOverlayWord();
    return;
  }
  lastRadarDrawMs = now;

  // Radar coordinates relative to canvas
  const int radarCenterX = layout.radarW / 2;
  const int radarCenterY = layout.radarH / 2;
  const int radarR = layout.radarRadius;
  uint16_t circleColor = Display_dimColor(0x4208); // very dark greyish red

  // Pre-render static background once to reduce per-frame work.
  if (!radarStaticReady && radarStatic.getBuffer() != nullptr) {
    uint16_t bg = Display_dimColor(0x0000);
    radarStatic.fillScreen(bg);
    radarStatic.drawCircle(radarCenterX, radarCenterY, radarR,         circleColor);
    radarStatic.drawCircle(radarCenterX, radarCenterY, radarR * 2 / 3, circleColor);
    radarStatic.drawCircle(radarCenterX, radarCenterY, radarR / 3,     circleColor);
    radarStatic.drawLine(radarCenterX - radarR, radarCenterY, radarCenterX + radarR, radarCenterY, circleColor);
    radarStatic.drawLine(radarCenterX, radarCenterY - radarR, radarCenterX, radarCenterY + radarR, circleColor);
    radarStaticReady = true;
  }

  // Copy static background into dynamic canvas.
  if (radarStatic.getBuffer() != nullptr && radarStaticReady) {
    memcpy(radarCanvas.getBuffer(), radarStatic.getBuffer(), RADAR_W * RADAR_H * sizeof(uint16_t));
  } else {
    uint16_t bg = Display_dimColor(0x0000);
    radarCanvas.fillScreen(bg);
    // Fallback: draw rings and crosshair directly when static buffer unavailable.
    radarCanvas.drawCircle(radarCenterX, radarCenterY, radarR,         circleColor);
    radarCanvas.drawCircle(radarCenterX, radarCenterY, radarR * 2 / 3, circleColor);
    radarCanvas.drawCircle(radarCenterX, radarCenterY, radarR / 3,     circleColor);
    radarCanvas.drawLine(radarCenterX - radarR, radarCenterY, radarCenterX + radarR, radarCenterY, circleColor);
    radarCanvas.drawLine(radarCenterX, radarCenterY - radarR, radarCenterX, radarCenterY + radarR, circleColor);
  }

  // Sweeping radar line (animated)
  sweepAngle += 0.12f; // adjust speed to taste
  if (sweepAngle > 2.0f * PI) sweepAngle -= 2.0f * PI;
  int sweepX = radarCenterX + (int)(cosf(sweepAngle) * radarR);
  int sweepY = radarCenterY + (int)(sinf(sweepAngle) * radarR);
  uint16_t sweepBright = Display_dimColor(0xF800);
  uint16_t sweepDim = Display_dimColor(0x8000);
  radarCanvas.drawLine(radarCenterX, radarCenterY, sweepX, sweepY, sweepBright);
  int trailX = radarCenterX + (int)(cosf(sweepAngle - 0.1f) * (radarR - 6));
  int trailY = radarCenterY + (int)(sinf(sweepAngle - 0.1f) * (radarR - 6));
  radarCanvas.drawLine(radarCenterX, radarCenterY, trailX, trailY, sweepDim);

  if (apCountCopy > 0 && hasData) {
    for (int i = 0; i < apCountCopy; i++) {
      int rssi = rssiCopy[i];
      int ch   = channelCopy[i];

      float rNorm = mapFloat((float)rssi, -100.0f, -30.0f, 1.0f, 0.0f, true);
      float radius = 5.0f + rNorm * (radarR - 5.0f);

      float angle;
      if (ch >= 1 && ch <= 13) {
        angle = ((ch - 1) / 12.0f) * 2.0f * PI;
      } else {
        angle = (i / (float)apCountCopy) * 2.0f * PI;
      }

      int px = radarCenterX + (int)(cosf(angle) * radius);
      int py = radarCenterY + (int)(sinf(angle) * radius);

      // Red-only intensity: bright = strong, dim = weak
      uint16_t color;
      if (rssi > -60) {
        color = Display_dimColor(0xF800); // bright red
      } else if (rssi > -75) {
        color = Display_dimColor(0x8000); // dark red
      } else {
        color = circleColor; // very dark
      }

      radarCanvas.fillCircle(px, py, 3, color); // larger dots (~25%+)
    }
  }

  const int inset = 4;
  int compLeftX = inset;
  int compRightX = layout.radarW - inset;
  int compTopY = inset;
  int compBottomY = layout.radarH - inset;

  drawComplication(Settings_getTopLeftComplication(), compLeftX, compTopY, TextAlign::LEFT);
  drawComplication(Settings_getTopRightComplication(), compRightX, compTopY, TextAlign::RIGHT);
  drawComplication(Settings_getBottomLeftComplication(), compLeftX, compBottomY, TextAlign::LEFT);
  drawComplication(Settings_getBottomRightComplication(), compRightX, compBottomY, TextAlign::RIGHT);

  tft.drawRGBBitmap(layout.radarX, layout.radarY,
                    radarCanvas.getBuffer(),
                    layout.radarW, layout.radarH);

  // Redraw overlay after radar refresh so it stays on top
  Display_drawOverlayWord();
}

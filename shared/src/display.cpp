#include "Display.h"
#include "Dictionary.h"
#include "Settings.h"
#include "config_core.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <math.h>

// --- Color scheme (RGB565) ---
static const uint16_t COL_BG_BASE = 0x0000;    // pure black
static const uint16_t COL_PANEL_BASE = 0x2104; // very dark grey
static const uint16_t COL_TEXT_BASE =
    0xC618; // soft light grey (not pure white)
static const uint16_t COL_TEXT_DIM_BASE = 0x8410;    // dim grey
static const uint16_t COL_ACCENT_RED_BASE = 0xF800;  // bright red
static const uint16_t COL_ACCENT_SOFT_BASE = 0x8000; // dark red

static const float BRIGHTNESS_SCALES[] = {0.35f, 0.6f, 1.0f, 1.25f};
static const uint8_t BRIGHTNESS_LEVEL_COUNT =
    sizeof(BRIGHTNESS_SCALES) / sizeof(BRIGHTNESS_SCALES[0]);

struct UiPalette {
  uint16_t bg;
  uint16_t panel;
  uint16_t text;
  uint16_t textDim;
  uint16_t accent;
  uint16_t accentSoft;
};

struct UiStyle {
  UiPalette base;
  UiPalette night;
  bool nightMode;
  float brightnessScale;

  UiPalette current() const { return nightMode ? night : base; }

  uint16_t scale(uint16_t color) const {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    float s = brightnessScale;
    r = (uint8_t)min(31, (int)(r * s));
    g = (uint8_t)min(63, (int)(g * s));
    b = (uint8_t)min(31, (int)(b * s));
    return (r << 11) | (g << 5) | b;
  }

  uint16_t bg() const { return scale(current().bg); }
  uint16_t panel() const { return scale(current().panel); }
  uint16_t text() const { return scale(current().text); }
  uint16_t textDim() const { return scale(current().textDim); }
  uint16_t accent() const { return scale(current().accent); }
  uint16_t accentSoft() const { return scale(current().accentSoft); }
};

static UiStyle uiStyle = {{COL_BG_BASE, COL_PANEL_BASE, COL_TEXT_BASE,
                           COL_TEXT_DIM_BASE, COL_ACCENT_RED_BASE,
                           COL_ACCENT_SOFT_BASE},
                          {0x0000, 0x1082, 0xBDF7, 0x7BEF, 0xF980,
                           0x9000}, // night variant: warmer, softer
                          false,
                          BRIGHTNESS_SCALES[2]};

static DisplayHardwareConfig hwConfig = {-1, -1, -1, -1};
static Adafruit_ILI9341 *tftPtr = nullptr;
#define tft (*tftPtr)
static char lastDisplayedLetter = 0;
static uint8_t currentBrightnessLevel = 2;

static int DISPLAY_BL_PIN = -1;

static const int DISPLAY_BL_CHANNEL = 7;
static bool backlightConfigured = false;

// Layout cached for other modules
static DisplayLayout layout;

// Heartbeat scroller buffer (double-buffered windowed blit)
static GFXcanvas16 heartbeatCanvas(HEARTBEAT_W, HEARTBEAT_H);
static unsigned long lastHeartbeatStepMs = 0;
static const int HEARTBEAT_SPACING = 24;
static const unsigned long HEARTBEAT_SCROLL_INTERVAL_BASE_MS = 35;
static const uint16_t HEARTBEAT_SCROLL_BPM_BASE = 120;
static unsigned long heartbeatScrollIntervalMs =
    HEARTBEAT_SCROLL_INTERVAL_BASE_MS;
static const int HEARTBEAT_MAX_ENTRIES = 24; // keep ~last 20 chars safely
static int heartbeatDirtyX0 = 0;
static int heartbeatDirtyX1 = HEARTBEAT_W;
static uint16_t heartbeatBpmTarget = HEARTBEAT_SCROLL_BPM_BASE;
static uint16_t heartbeatBpmCurrent = HEARTBEAT_SCROLL_BPM_BASE;
static unsigned long lastHeartbeatEaseMs = 0;
static const unsigned long HEARTBEAT_EASE_INTERVAL_MS = 80;

struct HeartbeatEntry {
  char c;
  int x; // pixel position relative to visible window
};
static HeartbeatEntry heartbeatEntries[HEARTBEAT_MAX_ENTRIES];
static int heartbeatEntryCount = 0;

// Word overlay
static String overlayWord;

// Header/status state
static bool tftReady = false;
static bool statusSdPresent = false;
static bool statusLoggingEnabled = false;
static bool statusWifiActive = false;
static unsigned long sdPulseUntilMs = 0;
static unsigned long wifiPulseUntilMs = 0;

// Word history (right module)
static const int WORD_HISTORY_MAX = 5;
static String wordHistory[WORD_HISTORY_MAX];
static int wordHistoryCount = 0;

// Overlay fade
static unsigned long overlayFadeStartMs = 0;
static const unsigned long OVERLAY_FADE_MS = 600;

void Display_configure(const DisplayHardwareConfig &cfg) {
  hwConfig = cfg;
  DISPLAY_BL_PIN = cfg.blPin;
}

static bool ensureTft() {
  if (tftPtr)
    return true;
  if (hwConfig.cs < 0 || hwConfig.dc < 0) {
    Serial.println(F("Display pins not configured"));
    return false;
  }
  tftPtr = new Adafruit_ILI9341(hwConfig.cs, hwConfig.dc, hwConfig.rst);
  return tftPtr != nullptr;
}

static void pushWordHistory(const String &w) {
  if (w.length() == 0)
    return;
  for (int i = min(wordHistoryCount, WORD_HISTORY_MAX - 1); i > 0; i--) {
    wordHistory[i] = wordHistory[i - 1];
  }
  wordHistory[0] = w;
  if (wordHistoryCount < WORD_HISTORY_MAX)
    wordHistoryCount++;
}

static void computeLayout() {
  // Asymmetric layout: radar anchored to the left, recents on the right.
  const int LEFT_BADGE_W =
      0; // letter badge floats over radar, not reserving column space
  const int RIGHT_PANEL_MIN = 90;  // reserve more space so RECENTS fits cleanly
  const float RADAR_SCALE = 0.95f; // applied to radius, not canvas size

  layout.headerX = UI_PADDING;
  layout.headerY = UI_PADDING;
  layout.headerW = INNER_W;
  layout.headerH = HEADER_H;

  layout.mainX = UI_PADDING;
  layout.mainY = layout.headerY + layout.headerH + 2; // 2px margin below header
  layout.mainW = INNER_W;
  layout.mainH = INNER_H - HEADER_H - HEARTBEAT_H - 2;

  layout.heartbeatX = UI_PADDING;
  layout.heartbeatY = SCREEN_H - UI_PADDING - HEARTBEAT_H;
  layout.heartbeatW = HEARTBEAT_W;
  layout.heartbeatH = HEARTBEAT_H;

  layout.moduleLeftW = LEFT_BADGE_W;
  layout.moduleRightW =
      max(RIGHT_PANEL_MIN,
          INNER_W - RADAR_W - 4); // 4px gutter between radar and recents

  // Keep canvas dimensions tied to the allocated radarCanvas (RADAR_W/H) to
  // avoid stride mismatches.
  layout.radarW = RADAR_W;
  layout.radarH = RADAR_H;
  layout.radarX = layout.mainX;
  layout.radarY = layout.mainY;
  layout.radarCenterX = layout.radarX + layout.radarW / 2;
  layout.radarCenterY = layout.radarY + layout.radarH / 2;
  int baseRadius = min(layout.radarW, layout.radarH) / 2 - 4;
  layout.radarRadius = (int)(baseRadius * RADAR_SCALE);
}

static uint16_t applyBrightness(uint16_t color) { return uiStyle.scale(color); }

uint16_t Display_dimColor(uint16_t color) { return applyBrightness(color); }

static uint16_t colBg() { return uiStyle.bg(); }
static uint16_t colPanel() { return uiStyle.panel(); }
static uint16_t colText() { return uiStyle.text(); }
static uint16_t colTextDim() { return uiStyle.textDim(); }
static uint16_t colAccent() { return uiStyle.accent(); }
static uint16_t colAccentSoft() { return uiStyle.accentSoft(); }

Adafruit_ILI9341 &Display_tft() {
  ensureTft();
  return *tftPtr;
}

static void ensureBacklightConfigured() {
  if (backlightConfigured || DISPLAY_BL_PIN < 0)
    return;
  ledcSetup(DISPLAY_BL_CHANNEL, 5000, 8);
  ledcAttachPin(DISPLAY_BL_PIN, DISPLAY_BL_CHANNEL);
  backlightConfigured = true;
}

void Display_applyBrightness(uint8_t level) {
  level = Settings_clampBrightness(level, BRIGHTNESS_LEVEL_COUNT - 1);
  currentBrightnessLevel = level;
  uiStyle.brightnessScale = BRIGHTNESS_SCALES[level];

  if (DISPLAY_BL_PIN >= 0) {
    ensureBacklightConfigured();
    int duty = (int)(uiStyle.brightnessScale * 255.0f);
    if (duty > 255)
      duty = 255;
    ledcWrite(DISPLAY_BL_CHANNEL, duty);
  }
}

uint8_t Display_getBrightnessLevel() { return currentBrightnessLevel; }

const DisplayLayout &Display_getLayout() { return layout; }

static bool heartbeatReady() { return heartbeatCanvas.getBuffer() != nullptr; }

static void updateHeartbeatInterval(uint16_t bpm) {
  uint16_t clamped = Settings_clampHeartbeat(bpm);
  heartbeatScrollIntervalMs = (uint32_t)HEARTBEAT_SCROLL_INTERVAL_BASE_MS *
                              HEARTBEAT_SCROLL_BPM_BASE /
                              (clamped == 0 ? 1 : clamped);
  heartbeatBpmCurrent = clamped;
}

static void heartbeatReset() {
  if (!heartbeatReady()) {
    Serial.println(F("heartbeat canvas alloc failed; skipping scroller"));
    return;
  }
  heartbeatCanvas.fillScreen(colBg());
  heartbeatEntryCount = 0;
  heartbeatDirtyX0 = 0;
  heartbeatDirtyX1 = HEARTBEAT_W;
  heartbeatBpmCurrent = heartbeatBpmTarget;
}

void Display_begin() {
  if (!ensureTft()) {
    Serial.println(F("Display init skipped; missing driver"));
    return;
  }
  computeLayout();
  tft.begin();
  tft.setRotation(1); // Landscape: 320x240
  tftReady = true;
  updateHeartbeatInterval(Settings_get().heartbeatBpm);
}

static bool isPulsing(unsigned long untilMs, unsigned long now) {
  return untilMs != 0 && now < untilMs;
}

static uint16_t statusColor(bool active, bool pulsing) {
  if (!active)
    return colTextDim();
  return pulsing ? colAccent() : colAccentSoft();
}

static uint16_t lerpColor(uint16_t a, uint16_t b, float t) {
  if (t <= 0.0f)
    return a;
  if (t >= 1.0f)
    return b;
  uint8_t ar = (a >> 11) & 0x1F;
  uint8_t ag = (a >> 5) & 0x3F;
  uint8_t ab = a & 0x1F;
  uint8_t br = (b >> 11) & 0x1F;
  uint8_t bg = (b >> 5) & 0x3F;
  uint8_t bb = b & 0x1F;
  uint8_t rr = ar + (uint8_t)((br - ar) * t);
  uint8_t rg = ag + (uint8_t)((bg - ag) * t);
  uint8_t rb = ab + (uint8_t)((bb - ab) * t);
  return (rr << 11) | (rg << 5) | rb;
}

static void drawSdIcon(int x, int y, int size, uint16_t color, uint16_t bg) {
  tft.fillRect(x, y + 2, size - 2, size - 4, bg);
  tft.drawRoundRect(x, y + 2, size - 2, size - 4, 2, color);
  tft.fillRect(x + 3, y + 5, size - 8, size - 10, color);
}

static void drawLogIcon(int x, int y, int size, uint16_t color, uint16_t bg) {
  tft.fillRect(x, y, size, size, bg);
  int lineH = 2;
  int pad = 2;
  for (int i = 0; i < 3; i++) {
    int ly = y + pad + i * (lineH + 2);
    tft.fillRect(x + pad, ly, size - pad * 2, lineH, color);
  }
  tft.drawRect(x + 1, y + 1, size - 2, size - 2, color);
}

static void drawWifiIcon(int x, int y, int size, uint16_t color, uint16_t bg) {
  tft.fillRect(x, y, size, size, bg);
  int cx = x + size / 2;
  int cy = y + size / 2 + 1;
  tft.drawCircle(cx, cy, size / 2 - 1, color);
  tft.drawCircle(cx, cy, size / 3, color);
  tft.fillCircle(cx, cy + 2, 2, color);
}

static void drawHeaderStatusArea() {
  if (!tftReady)
    return;
  uint16_t bg = colPanel();
  const int areaW = 94;
  const int inset = 2;
  int areaX = layout.headerX + layout.headerW - areaW - inset;
  int areaY = layout.headerY + inset;
  int areaH = layout.headerH - inset * 2;
  tft.fillRect(areaX, areaY, areaW, areaH, bg);

  unsigned long now = millis();
  bool sdPulse = isPulsing(sdPulseUntilMs, now);
  bool wifiPulse = isPulsing(wifiPulseUntilMs, now);

  // Icons on the far right
  const int iconSize = 14;
  int iconY = areaY + (areaH - iconSize) / 2;
  int iconX = areaX + areaW - iconSize - 4;
  drawWifiIcon(iconX, iconY, iconSize, statusColor(statusWifiActive, wifiPulse),
               bg);
  iconX -= iconSize + 6;
  drawLogIcon(iconX, iconY, iconSize, statusColor(statusLoggingEnabled, false),
              bg);
  iconX -= iconSize + 6;
  drawSdIcon(iconX, iconY, iconSize, statusColor(statusSdPresent, sdPulse), bg);
}

static void drawLetterBadge(char l) {
  // Floating badge in the top-left of radar region
  int badgeW = 56;
  int badgeH = 56;
  int areaX = layout.radarX + 6;
  int areaY = layout.radarY + 6;

  tft.fillRoundRect(areaX, areaY, badgeW, badgeH, 6, colPanel());
  tft.drawRoundRect(areaX, areaY, badgeW, badgeH, 6, colAccentSoft());

  if (l == 0)
    return;

  char buf[2] = {l, '\0'};
  tft.setFont(&FreeSans24pt7b);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);

  int centerX = areaX + badgeW / 2;
  int centerY = areaY + badgeH / 2;

  int16_t textX = centerX - (w / 2);
  int16_t textY = centerY + (h / 2) - 4;

  tft.setTextColor(colAccentSoft());
  tft.setCursor(textX + 2, textY + 2);
  tft.print(buf);

  tft.setTextColor(colAccent());
  tft.setCursor(textX, textY);
  tft.print(buf);
  tft.setFont(); // restore default
}

static void drawWordHistoryPanel() {
  int areaX = layout.mainX + layout.mainW - layout.moduleRightW;
  int areaY = layout.mainY;
  int areaW = layout.moduleRightW;
  int areaH = layout.mainH;
  tft.fillRect(areaX, areaY, areaW, areaH, colBg());
  tft.fillRoundRect(areaX + 2, areaY + 2, areaW - 4, areaH - 4, 4, colPanel());

  const char *label = "RECENTS";
  int16_t bx, by;
  uint16_t bw, bh;
  tft.setFont();
  tft.getTextBounds(label, 0, 0, &bx, &by, &bw, &bh);
  int labelX = areaX + (areaW - bw) / 2;
  if (labelX < areaX + 4)
    labelX = areaX + 4;
  tft.setTextColor(colAccent());
  tft.setCursor(labelX, areaY + 14);
  tft.print(label);

  int maxLines = min(wordHistoryCount, WORD_HISTORY_MAX);
  int lineY = areaY + 28;
  for (int i = 0; i < maxLines; i++) {
    tft.setTextColor(colText(), colPanel());
    tft.setCursor(areaX + 6, lineY);
    String word = wordHistory[i];
    if (word.length() > 8) {
      word = word.substring(0, 7) + ".";
    }
    tft.print(word);
    lineY += 14;
    if (lineY > areaY + areaH - 12)
      break;
  }
}

static void drawHeader() {
  uint16_t bg = colPanel();
  int radius = 6;
  tft.fillRoundRect(layout.headerX, layout.headerY, layout.headerW,
                    layout.headerH, radius, bg);

  // Title: OVILUS on the left
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1); // keep consistent scaling
  tft.setTextColor(colAccent());
  int titleInset = 6;
  int titleX = layout.headerX + titleInset;
  int titleY = layout.headerY + layout.headerH - 6;
  // Apply ~5% visual shrink by using the smaller FreeSans9 for the label
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(titleX, titleY);
  tft.print("OVILUS");
  tft.setFont(); // restore default for rest

  drawHeaderStatusArea();
}

static void drawModules() {
  // Clear left/right module regions; actual modules may render later.
  uint16_t bg = colBg();
  if (layout.moduleLeftW > 0) {
    tft.fillRect(layout.mainX, layout.mainY, layout.moduleLeftW, layout.mainH,
                 bg);
  }
  tft.fillRect(layout.mainX + layout.mainW - layout.moduleRightW, layout.mainY,
               layout.moduleRightW, layout.mainH, bg);

  if (ModuleLeft_enabled()) {
    ModuleLeft_draw(tft, layout.mainX, layout.mainY, layout.moduleLeftW,
                    layout.mainH);
  }
  if (ModuleRight_enabled()) {
    ModuleRight_draw(tft, layout.mainX + layout.mainW - layout.moduleRightW,
                     layout.mainY, layout.moduleRightW, layout.mainH);
  }
}

void Display_drawStaticFrame() {
  lastDisplayedLetter = 0;
  computeLayout();

  tft.fillScreen(colBg());
  drawHeader();
  drawModules();

  // Clear radar + heartbeat areas
  tft.fillRect(layout.radarX, layout.radarY, layout.radarW, layout.radarH,
               colBg());
  tft.fillRect(layout.heartbeatX, layout.heartbeatY, layout.heartbeatW,
               layout.heartbeatH, colBg());
  heartbeatReset();
  drawLetterBadge(0);
  drawWordHistoryPanel();
}

void Display_updateStatusBar(float temp, float humidity) {
  (void)temp;
  (void)humidity;
  drawHeaderStatusArea();
}

void Display_setNightMode(bool enabled) {
  uiStyle.nightMode = enabled;
  if (tftReady) {
    Display_drawStaticFrame();
    drawHeaderStatusArea();
  }
}

void Display_setSdPresent(bool present) {
  statusSdPresent = present;
  if (tftReady)
    drawHeaderStatusArea();
}

void Display_setLoggingEnabled(bool enabled) {
  statusLoggingEnabled = enabled;
  if (tftReady)
    drawHeaderStatusArea();
}

void Display_setWifiActive(bool active) {
  statusWifiActive = active;
  if (tftReady)
    drawHeaderStatusArea();
}

void Display_notifySdActivity() {
  sdPulseUntilMs = millis() + 450;
  if (tftReady)
    drawHeaderStatusArea();
}

void Display_notifyWifiScan() {
  wifiPulseUntilMs = millis() + 450;
  if (tftReady)
    drawHeaderStatusArea();
}

void Display_displayLetter(char l) {
  if (l == lastDisplayedLetter)
    return;
  lastDisplayedLetter = l;

  drawLetterBadge(l);
}

void Display_setOverlayWord(const String &w) {
  if (overlayWord != w) {
    overlayWord = w;
    overlayFadeStartMs = millis();
  }
}

void Display_displayWord(const String &w) {
  Display_setOverlayWord(w);
  pushWordHistory(w);
  drawWordHistoryPanel();
  Display_drawOverlayWord();
}

void Display_drawOverlayWord() {
  if (overlayWord.length() == 0)
    return;

  // Draw centered over radar region
  int16_t x1, y1;
  uint16_t ww, hh;
  tft.setFont(&FreeSans12pt7b);
  tft.getTextBounds(overlayWord, 0, 0, &x1, &y1, &ww, &hh);

  int16_t cx = layout.radarX + (layout.radarW - ww) / 2;
  int16_t cy = layout.radarY + (layout.radarH + hh) / 2;

  unsigned long now = millis();
  if (overlayFadeStartMs == 0)
    overlayFadeStartMs = now;
  float t = (float)(now - overlayFadeStartMs) / (float)OVERLAY_FADE_MS;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  uint16_t mainColor = lerpColor(colAccentSoft(), colAccent(), t);

  tft.setTextColor(colAccentSoft());
  tft.setCursor(cx + 2, cy + 2);
  tft.print(overlayWord);

  tft.setTextColor(mainColor);
  tft.setCursor(cx, cy);
  tft.print(overlayWord);
  tft.setFont();
}

void Display_clearWordArea() {
  // Overlay persists naturally; clearing just removes the stored word.
  overlayWord = "";
}

void Display_heartbeatStep(char letterOrZero) {
  if (!heartbeatReady())
    return;
  if (letterOrZero == 0)
    return; // only draw real letters
  // Drop oldest if at capacity
  if (heartbeatEntryCount >= HEARTBEAT_MAX_ENTRIES) {
    memmove(heartbeatEntries, heartbeatEntries + 1,
            (HEARTBEAT_MAX_ENTRIES - 1) * sizeof(HeartbeatEntry));
    heartbeatEntryCount = HEARTBEAT_MAX_ENTRIES - 1;
  }
  int startX = HEARTBEAT_W;
  if (heartbeatEntryCount > 0) {
    int lastX = heartbeatEntries[heartbeatEntryCount - 1].x;
    if (lastX > startX)
      startX = lastX + HEARTBEAT_SPACING;
  }
  heartbeatEntries[heartbeatEntryCount].c = letterOrZero;
  heartbeatEntries[heartbeatEntryCount].x = startX;
  heartbeatEntryCount++;
  heartbeatDirtyX0 = 0;
  heartbeatDirtyX1 = HEARTBEAT_W;
}

void Display_updateHeartbeat() {
  unsigned long now = millis();
  bool pulseChanged = false;
  if (sdPulseUntilMs && now > sdPulseUntilMs) {
    sdPulseUntilMs = 0;
    pulseChanged = true;
  }
  if (wifiPulseUntilMs && now > wifiPulseUntilMs) {
    wifiPulseUntilMs = 0;
    pulseChanged = true;
  }
  if (pulseChanged && tftReady) {
    drawHeaderStatusArea();
  }

  if (!heartbeatReady())
    return;

  // Ease toward target BPM to avoid abrupt jumps
  if (heartbeatBpmCurrent != heartbeatBpmTarget &&
      now - lastHeartbeatEaseMs >= HEARTBEAT_EASE_INTERVAL_MS) {
    lastHeartbeatEaseMs = now;
    int delta = (int)heartbeatBpmTarget - (int)heartbeatBpmCurrent;
    if (delta > 5)
      delta = 5;
    if (delta < -5)
      delta = -5;
    heartbeatBpmCurrent = (uint16_t)((int)heartbeatBpmCurrent + delta);
    updateHeartbeatInterval(heartbeatBpmCurrent);
  }

  if (now - lastHeartbeatStepMs >= heartbeatScrollIntervalMs) {
    lastHeartbeatStepMs = now;
    // Advance all entries left; drop those fully off-screen.
    int writeIdx = 0;
    for (int i = 0; i < heartbeatEntryCount; i++) {
      heartbeatEntries[i].x -= 1;
      if (heartbeatEntries[i].x < -HEARTBEAT_SPACING)
        continue;
      heartbeatEntries[writeIdx++] = heartbeatEntries[i];
    }
    heartbeatEntryCount = writeIdx;
    heartbeatDirtyX0 = 0;
    heartbeatDirtyX1 = HEARTBEAT_W;
  }
  // Redraw heartbeat strip (dirty range only)
  heartbeatCanvas.fillRect(heartbeatDirtyX0, 0,
                           heartbeatDirtyX1 - heartbeatDirtyX0, HEARTBEAT_H,
                           colBg());
  uint16_t lineC = colTextDim();
  int midY = HEARTBEAT_H / 2;
  heartbeatCanvas.drawLine(heartbeatDirtyX0, midY, heartbeatDirtyX1, midY,
                           lineC);

  heartbeatCanvas.setFont(&FreeSans12pt7b);
  heartbeatCanvas.setTextColor(colAccent());
  for (int i = 0; i < heartbeatEntryCount; i++) {
    char buf[2] = {heartbeatEntries[i].c, '\0'};
    int16_t x1, y1;
    uint16_t w, h;
    heartbeatCanvas.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    int16_t tx = heartbeatEntries[i].x - w / 2 + HEARTBEAT_SPACING / 2;
    int16_t ty = (HEARTBEAT_H + h) / 2;
    heartbeatCanvas.setCursor(tx, ty);
    heartbeatCanvas.print(buf);
  }
  heartbeatCanvas.setFont();

  tft.drawRGBBitmap(layout.heartbeatX + heartbeatDirtyX0, layout.heartbeatY,
                    heartbeatCanvas.getBuffer() + heartbeatDirtyX0,
                    heartbeatDirtyX1 - heartbeatDirtyX0, HEARTBEAT_H);

  heartbeatDirtyX0 = 0;
  heartbeatDirtyX1 = HEARTBEAT_W;
}

static void drawRowLabel(int y, const char *label) {
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colAccent(), colBg());
  tft.setCursor(UI_PADDING, y);
  tft.print(label);
  tft.setFont();
}

static void drawBrightnessRow(int top, int height, uint8_t level) {
  int midY = top + height / 2 + 6;
  drawRowLabel(midY, "BRIGHT");
  int barX = 120;
  int barW = 160;
  int barH = 26;
  int barY = top + (height - barH) / 2;
  tft.drawRoundRect(barX, barY, barW, barH, 4, colAccentSoft());

  int stepW = barW / BRIGHTNESS_LEVEL_COUNT;
  for (uint8_t i = 0; i < BRIGHTNESS_LEVEL_COUNT; i++) {
    int cx = barX + i * stepW + stepW / 2 - 4;
    uint16_t c = (i == level) ? colAccent() : colTextDim();
    tft.fillCircle(cx + 4, barY + barH / 2, 6, c);
  }
}

static void drawDictionaryRow(int top, int height, uint8_t idx) {
  int midY = top + height / 2 + 6;
  drawRowLabel(midY, "DICT");
  int boxX = 120;
  int boxW = 160;
  int boxH = 28;
  int boxY = top + (height - boxH) / 2;
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 4, colAccentSoft());

  const char *name = Dictionary_getNameForIndex(idx);
  if (!name)
    name = "N/A";
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colText(), colBg());
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);
  int cx = boxX + (boxW - w) / 2;
  if (cx < boxX + 4)
    cx = boxX + 4;
  tft.setCursor(cx, boxY + boxH - 6);
  tft.print(name);
  tft.setFont();
}

static void drawVarianceRow(int top, int height, float varianceScale) {
  int midY = top + height / 2 + 6;
  drawRowLabel(midY, "VAR");
  int barX = 120;
  int barW = 160;
  int barH = 22;
  int barY = top + (height - barH) / 2;
  tft.drawRoundRect(barX, barY, barW, barH, 4, colAccentSoft());

  float clamped = Settings_clampVariance(varianceScale);
  const float stops[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
  const uint8_t stopCount = sizeof(stops) / sizeof(stops[0]);
  int stepW = barW / stopCount;

  for (uint8_t i = 0; i < stopCount; i++) {
    int cx = barX + i * stepW + stepW / 2;
    uint16_t c =
        (fabsf(stops[i] - clamped) < 0.01f) ? colAccent() : colTextDim();
    tft.fillRect(cx - 4, barY + 4, 8, barH - 8, c);
  }
}

void Display_drawSettingsScreen(const DeviceSettings &s) {
  tft.fillScreen(colBg());

  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(colAccent(), colBg());
  tft.setCursor(UI_PADDING, UI_PADDING + 16);
  tft.print("SETTINGS");
  tft.setFont();

  uint8_t clampedBrightness =
      Settings_clampBrightness(s.brightnessLevel, BRIGHTNESS_LEVEL_COUNT - 1);
  uint8_t dictIdx = s.dictionaryIndex;
  uint8_t dictCount = Dictionary_getCount();
  if (dictCount > 0 && dictIdx >= dictCount)
    dictIdx = dictCount - 1;

  // Settings rows (keep in sync with TouchUI SETTINGS_ROW_* constants)
  const int rowTop[] = {36, 96, 156};
  const int rowH = 52;

  drawBrightnessRow(rowTop[0], rowH, clampedBrightness);
  drawDictionaryRow(rowTop[1], rowH, dictIdx);
  drawVarianceRow(rowTop[2], rowH, s.varianceScale);

  tft.setTextColor(colTextDim(), colBg());
  tft.setCursor(UI_PADDING + 40, SCREEN_H - UI_PADDING - 6);
  tft.print("BACK");
  tft.setCursor(SCREEN_W - 160, SCREEN_H - UI_PADDING - 6);
  tft.print("HOLD HEADER");
}

void Display_setHeartbeatBpm(uint16_t bpm) {
  heartbeatBpmTarget = Settings_clampHeartbeat(bpm);
}

// --- Module scaffolding (currently disabled) ---
bool ModuleLeft_enabled() { return false; }
void ModuleLeft_draw(Adafruit_ILI9341 &, int, int, int, int) {}
bool ModuleRight_enabled() { return false; }
void ModuleRight_draw(Adafruit_ILI9341 &, int, int, int, int) {}

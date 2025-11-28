#pragma once
#include <Arduino.h>

// --- DHT11 ---
#define DHTPIN 27
#define DHTTYPE DHT11

// --- MPU6050 I2C ---
#define I2C_SDA 25
#define I2C_SCL 33

// --- TFT ILI9341 SPI ---
#define LCD_CS   5
#define TFT_DC   2
#define TFT_RST  4

// --- XPT2046 Touch ---
#define TOUCH_CS 22

// --- SD CARD ---
#define HSPI_CS 15
#define HSPI_MOSI 13
#define HSPI_MISO 12
#define HSPI_SCK 14

// --- Touch calibration (from your raw readings) ---
#define TS_MINX 680
#define TS_MAXX 3400
#define TS_MINY 650
#define TS_MAXY 3250

// --- Display layout (landscape 320x240) ---
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const int UI_PADDING = 8;      // conceptual inset; not drawn
static const int HEADER_H = 24;
static const int HEARTBEAT_H = 36;
static const int MODULE_BOX_W = 70;

static const int INNER_W = SCREEN_W - 2 * UI_PADDING;
static const int INNER_H = SCREEN_H - 2 * UI_PADDING;

static const int HEARTBEAT_W = INNER_W;
// Keep buffer larger than the visible strip but not too large for heap.
static const int HEARTBEAT_BUFFER_W = HEARTBEAT_W + (HEARTBEAT_W / 2); // 1.5x visible width

static const int RADAR_W = INNER_W - 2 * MODULE_BOX_W;
static const int RADAR_H = INNER_H - HEADER_H - HEARTBEAT_H;

// --- Letter generation ---
const unsigned long SAMPLE_PERIOD_MS = 200;
const int STABLE_SAMPLES_REQUIRED = 3;
const int LETTER_BUFFER_SIZE = 32;

// --- Optional TFT backlight pin (PWM) ---
// #define TFT_BL 21  // uncomment and set if your board exposes a backlight pin

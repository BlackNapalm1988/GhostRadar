#pragma once
#include <Arduino.h>

#define BOARD_NAME "ESP-WROOM-32"

// --- DHT11 ---
#define DHT_PIN 27
#define DHT_TYPE 11

// --- MPU6050 I2C ---
#define I2C_SDA 25
#define I2C_SCL 33

// --- TFT ILI9341 SPI (VSPI) ---
#define VSPI_SCLK 18
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define LCD_CS   5
#define TFT_DC   2
#define TFT_RST  4
// Backlight is tied to 3.3V on this board; leave as -1 if not wired.
#define TFT_BL_PIN -1

// --- XPT2046 Touch ---
#define TOUCH_CS 22

// --- SD CARD (HSPI) ---
#define HSPI_CS 15
#define HSPI_MOSI 13
#define HSPI_MISO 12
#define HSPI_SCK 14

// --- Touch calibration (from your raw readings) ---
#define TS_MINX 680
#define TS_MAXX 3400
#define TS_MINY 650
#define TS_MAXY 3250

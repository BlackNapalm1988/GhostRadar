#include <Arduino.h>
#include <SPI.h>
#include "BoardConfig.h"
#include "BoardPins.h"
#include "Display.h"
#include "TouchUI.h"
#include "Sensors.h"

static SPIClass sdSpi(HSPI);

const char* Board_getName() {
  return BOARD_NAME;
}

void Board_initPins() {
  // UI bus for display + touch (VSPI)
  SPI.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, LCD_CS);
  pinMode(LCD_CS, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  // SD card bus (HSPI)
  sdSpi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  pinMode(HSPI_CS, OUTPUT);
  digitalWrite(HSPI_CS, HIGH);
}

void Board_initDisplay() {
  DisplayHardwareConfig cfg{LCD_CS, TFT_DC, TFT_RST, TFT_BL_PIN};
  Display_configure(cfg);
}

void Board_initTouch() {
  TouchCalibration cal{TS_MINX, TS_MAXX, TS_MINY, TS_MAXY};
  TouchUI_configure(TOUCH_CS, cal);
}

void Board_initSensors() {
  SensorPins pins{DHT_PIN, DHT_TYPE, I2C_SDA, I2C_SCL};
  Sensors_configure(pins);
}

SPIClass& Board_getSdSpi() { return sdSpi; }
int Board_getSdCsPin() { return HSPI_CS; }

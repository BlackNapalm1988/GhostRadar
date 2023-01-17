#include <Wire.h>
#include "drawMenus.h"

void setup() {
  // Calibration data Returned from Touch_calibrate.ino
  uint16_t calData[5] = { 212, 3442, 304, 3431, 1 };
  tft.setTouch(calData);
  tft.getSetup(user);

  // Start the serial monitor, debugging only
  Serial.begin(115200);

  // Intitalize the screen, sensors, etc.
  tft.init();
  dht.begin();
  
  // Fix the screen resolution and clear it.
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);

}

void loop() {
  // put your main code here, to run repeatedly:
  uint16_t x = 0, y = 0;

  bool isBootScreen = true;
  bool isMenuActive = false;

  bool pressed = tft.getTouch(&x, &y);

  if (!pressed && isBootScreen) {
    drawSplashScreen();
    pressed = tft.getTouch(&x, &y);
  }
  else if (pressed && !isMenuActive) {
    isMenuActive = true;
    tft.fillScreen(TFT_BLACK);
    
    while (isMenuActive) {
      drawMenuScreen();
      delay(10);

      tft.setTextSize(2);
      tft.drawNumber(getTemperature(), 60, 170);
      tft.drawNumber(getHumidity(), 165, 170);
      tft.drawNumber(x, 165, 140);
      tft.drawNumber(y, 60, 140);

    }
  }
  
  }
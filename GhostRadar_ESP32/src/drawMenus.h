#include <SPI.h>
#include <TFT_eSPI.h>
#include <String>
#include "loadSensor.h"

TFT_eSPI tft = TFT_eSPI(); // Invoke library

setup_t user; // The library defines the type "setup_t" as a struct
              // Calling tft.getSetup(user) populates it with the settings

void drawMenuScreen() {
  tft.setTextFont(0);

  // Draw the Border
  tft.drawFastHLine(10, 10, 220, TFT_RED);
  tft.drawFastHLine(10, 310, 220, TFT_RED);
  tft.drawFastVLine(10, 10, 300, TFT_RED);
  tft.drawFastVLine(230, 10, 300, TFT_RED);

  // Draw Grid as Buttons
  tft.drawFastHLine(10, 190, 220, TFT_RED);
  tft.drawFastHLine(10, 230, 220, TFT_RED);
  tft.drawFastHLine(10, 270, 220, TFT_RED);
  tft.drawFastVLine(120, 190, 120, TFT_RED);

  // Print Temp/Humidity
  tft.setTextSize(2);
  tft.drawString("aT: ", 20, 170); tft.drawString("H: ", 135, 170); 
  tft.drawString("M: ", 135, 140); tft.drawString("iT: ", 20, 140);

  tft.drawNumber(getTemperature(), 25, 80);
  tft.drawNumber(getHumidity(), 135, 100);

  // Draw Labels as Buttons
  tft.setTextSize(2);
  tft.drawString("START", 35, 205); tft.drawString("STOP", 150, 205); 
  tft.drawString("SAVE", 40, 245); tft.drawString("CLEAR", 150, 245); 
  tft.drawString("EXIT", 40, 285); tft.drawString("SETTINGS", 130, 285);

}

void drawSplashScreen() {

  // Screen is 240 by 320, Splash Screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("GhostRadar", 35, 40);
  tft.setTextSize(2);
  tft.drawString("Version 1", 70, 80);

  // Continue Button Prompt
  tft.setTextSize(1);
  tft.drawString("TOUCH ANYWHERE TO CONTINUE", 45, 165);

  // Continue Button Border
  tft.drawFastHLine(35, 150, 175, TFT_WHITE);
  tft.drawFastHLine(35, 180, 175, TFT_WHITE);
  tft.drawFastVLine(35, 150, 30, TFT_WHITE);
  tft.drawFastVLine(210, 150, 30, TFT_WHITE);


  // Draw the Border
  tft.drawFastHLine(10, 10, 220, TFT_RED);
  tft.drawFastHLine(10, 310, 220, TFT_RED);
  tft.drawFastVLine(10, 10, 300, TFT_RED);
  tft.drawFastVLine(230, 10, 300, TFT_RED);
  
  // Draw the Footer Text
  tft.setTextSize(1);
  tft.drawString("www.blacknapalm.com", 60, 240);  
  tft.drawString("Research into the Paranormal (RIP)", 20, 260);
  tft.drawString("An OpenSource Project", 55, 280);

}
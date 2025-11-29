#pragma once
#include <SPI.h>

// Called early in setup() to configure pins, buses, etc.
void Board_initPins();

// Provide display wiring and any required bus setup.
void Board_initDisplay();

// Configure touch controller pins/calibration.
void Board_initTouch();

// Configure sensor wiring (DHT/I2C/etc.).
void Board_initSensors();

// Optional: provide board name / ID for logs or diagnostics.
const char* Board_getName();

// Accessors for board-specific storage bus.
SPIClass& Board_getSdSpi();
int Board_getSdCsPin();

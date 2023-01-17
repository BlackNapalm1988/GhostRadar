#include "DHT.h"
#include <Arduino.h>

#define DHTPIN 27     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

DHT dht(DHTPIN, DHTTYPE);

// Get a reading from the DHT11 Temp and humidity sensor (Requires 1 Second Delay atleast upon intial boot)
int getTemperature(){
  delay(2000);
  int ambientTemp = dht.readTemperature(true);
  return ambientTemp;
}

int getHumidity(){
  delay(2000);
  int hum = dht.readHumidity();
  return hum;
}

// Get a reading from the internal hall sensor on the ESP32
int getHallSensor(){
  delay(2000);
  int hall = hallRead();
  return hall;
}
#include "Sensors.h"
#include "config.h"
#include "Settings.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <math.h>
#include "WifiRadar.h"   // for WiFi entropy

// DHT
static DHT dht(DHTPIN, DHTTYPE);
static float lastTempC = 25.0f;
static float lastHumidity = 50.0f;
static unsigned long lastDhtReadMs = 0;
static const unsigned long DHT_READ_INTERVAL_MS = 2000;

// MPU6050
static Adafruit_MPU6050 mpu;
static bool mpuReady = false;

// Entropy windows
static const int ENTROPY_WINDOW = 10;
static float accelMagWindow[ENTROPY_WINDOW];
static float gyroMagWindow[ENTROPY_WINDOW];
static int entropyIndex = 0;
static int entropyCount = 0;

// Letter generation
static unsigned long lastSampleTime = 0;
static uint32_t sampleCounter = 0;
static char currentCandidateLetter = 0;
static int stableCount = 0;

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

static float computeVariance(const float *arr, int n) {
  if (n <= 1) return 0.0f;
  float sum = 0.0f;
  for (int i = 0; i < n; i++) sum += arr[i];
  float mean = sum / n;
  float var = 0.0f;
  for (int i = 0; i < n; i++) {
    float d = arr[i] - mean;
    var += d * d;
  }
  var /= (n - 1);
  return var;
}

static void updateDhtIfNeeded() {
  unsigned long now = millis();
  if (now - lastDhtReadMs < DHT_READ_INTERVAL_MS) return;
  lastDhtReadMs = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) lastTempC = t;
  if (!isnan(h)) lastHumidity = h;
}

static void readMpuMagnitudes(float &accelMag, float &gyroMag) {
  if (!mpuReady) {
    accelMag = 0.0f;
    gyroMag = 0.0f;
    return;
  }
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  accelMag = sqrt(a.acceleration.x * a.acceleration.x +
                  a.acceleration.y * a.acceleration.y +
                  a.acceleration.z * a.acceleration.z);

  gyroMag = sqrt(g.gyro.x * g.gyro.x +
                 g.gyro.y * g.gyro.y +
                 g.gyro.z * g.gyro.z);
}

static void updateEntropyWindows(float accelMag, float gyroMag,
                                 float &accelEntropyNorm,
                                 float &gyroEntropyNorm) {
  accelMagWindow[entropyIndex] = accelMag;
  gyroMagWindow[entropyIndex] = gyroMag;
  entropyIndex = (entropyIndex + 1) % ENTROPY_WINDOW;
  if (entropyCount < ENTROPY_WINDOW) entropyCount++;

  float accelVar = computeVariance(accelMagWindow, entropyCount);
  float gyroVar  = computeVariance(gyroMagWindow, entropyCount);

  accelEntropyNorm = mapFloat(accelVar, 0.0f, 5.0f, 0.0f, 1.0f, true);
  gyroEntropyNorm  = mapFloat(gyroVar,  0.0f, 5.0f, 0.0f, 1.0f, true);
}

static float computeCombinedScore() {
  updateDhtIfNeeded();

  float tempNorm  = mapFloat(lastTempC,    10.0f, 40.0f, 0.0f, 1.0f, true);
  float humidNorm = mapFloat(lastHumidity, 20.0f, 90.0f, 0.0f, 1.0f, true);

  float accelMag = 0.0f, gyroMag = 0.0f;
  readMpuMagnitudes(accelMag, gyroMag);

  float accelNorm = mapFloat(accelMag,  0.0f, 20.0f, 0.0f, 1.0f, true);
  float gyroNorm  = mapFloat(gyroMag,   0.0f, 300.0f, 0.0f, 1.0f, true);

  float accelEntropyNorm = 0.0f;
  float gyroEntropyNorm  = 0.0f;
  updateEntropyWindows(accelMag, gyroMag, accelEntropyNorm, gyroEntropyNorm);

  int hallRaw = hallRead();
  float hallNorm = mapFloat((float)hallRaw, -200.0f, 200.0f, 0.0f, 1.0f, true);

  // WiFi entropy from radar module
  WifiEntropy we = WifiRadar_getEntropy();
  float wifiStrengthNorm = mapFloat((float)we.strongest, -100.0f, -30.0f, 0.0f, 1.0f, true);
  float wifiVarNorm      = mapFloat(we.variance, 0.0f, 400.0f, 0.0f, 1.0f, true);
  float wifiCountNorm    = mapFloat((float)we.count, 0.0f, 10.0f, 0.0f, 1.0f, true);

  // Variance scaling from settings: higher = more chaotic, lower = calmer.
  float varianceScale = Settings_get().varianceScale;
  accelEntropyNorm = clampFloat(accelEntropyNorm * varianceScale, 0.0f, 1.0f);
  gyroEntropyNorm  = clampFloat(gyroEntropyNorm * varianceScale, 0.0f, 1.0f);
  wifiVarNorm      = clampFloat(wifiVarNorm * varianceScale, 0.0f, 1.0f);

  float score =
    0.5f * tempNorm +
    0.5f * humidNorm +
    1.2f * accelNorm +
    1.2f * gyroNorm +
    1.0f * accelEntropyNorm +
    1.0f * gyroEntropyNorm +
    0.3f * hallNorm +
    0.8f * wifiStrengthNorm +
    0.7f * wifiVarNorm +
    0.5f * wifiCountNorm;

  float weightTotal =
    0.5f + 0.5f + 1.2f + 1.2f +
    1.0f + 1.0f + 0.3f +
    0.8f + 0.7f + 0.5f;

  score /= weightTotal;
  return clampFloat(score, 0.0f, 1.0f);
}

static float addEntropy(float baseScore) {
  uint32_t t = millis() ^ (sampleCounter * 2654435761UL);
  float noise = (float)(t & 0xFF) / 255.0f;
  float mixed = 0.8f * baseScore + 0.2f * noise;
  return clampFloat(mixed, 0.0f, 1.0f);
}

static char scoreToLetter(float score) {
  int idx = (int)(score * 26.0f);
  if (idx < 0) idx = 0;
  if (idx > 25) idx = 25;
  return (char)('A' + idx);
}

void Sensors_begin() {
  dht.begin();

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found; continuing without IMU data.");
    mpuReady = false;
  } else {
    mpuReady = true;
    Serial.println("MPU6050 found");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  for (int i = 0; i < ENTROPY_WINDOW; i++) {
    accelMagWindow[i] = 0.0f;
    gyroMagWindow[i] = 0.0f;
  }

  lastSampleTime = millis();
}

bool Sensors_pollLetter(char &outLetter) {
  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_PERIOD_MS) {
    return false;
  }
  lastSampleTime = now;
  sampleCounter++;

  float score = computeCombinedScore();
  score = addEntropy(score);
  char letter = scoreToLetter(score);

  if (letter == currentCandidateLetter) {
    stableCount++;
  } else {
    currentCandidateLetter = letter;
    stableCount = 1;
  }

  if (stableCount >= STABLE_SAMPLES_REQUIRED) {
    outLetter = currentCandidateLetter;
    stableCount = 0;
    return true;
  }
  return false;
}

float Sensors_getLastTempC() {
  return lastTempC;
}

float Sensors_getLastHumidity() {
  return lastHumidity;
}

uint8_t Sensors_getBatteryPercent() {
  // Placeholder until a real battery ADC line is wired; assume full battery.
  return 100;
}

uint8_t Sensors_getWifiStrengthPercent() {
  WifiEntropy we = WifiRadar_getEntropy();
  if (we.count <= 0) return 0;
  float pct = mapFloat((float)we.strongest, -100.0f, -30.0f, 0.0f, 100.0f, true);
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return (uint8_t)(pct + 0.5f);
}

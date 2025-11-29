# GhostRadar – ESP-WROOM-32

Board-specific PlatformIO project for the ESP-WROOM-32 wiring (TJCTM24024 ILI9341 + XPT2046 touch, DHT11, MPU6050, SD on HSPI).

## Wiring
- DHT11 data `GPIO27`.
- MPU6050 I2C `SDA=25`, `SCL=33`.
- ILI9341 TFT: `CS=5`, `DC=2`, `RST=4`, `MOSI=23`, `MISO=19`, `SCK=18`.
- XPT2046 touch: `CS=22` (IRQ unused), shares the VSPI bus.
- SD card (HSPI): `CS=15`, `MOSI=13`, `MISO=12`, `SCK=14`.
- Touch calibration constants live in `include/BoardPins.h` (`TS_MINX/TS_MAXX/TS_MINY/TS_MAXY`).

See `pin_assignments.md` for a copy/paste reference.

## Project Layout (board folder)
- `platformio.ini` – points to the shared core (`../../shared/src`, `../../shared/include`).
- `src/BoardConfig.cpp` – initializes SPI buses and feeds pins/calibration into the shared modules.
- `include/BoardPins.h` – pin map, sensor type, touch calibration, board name.
- `data/` – SPIFFS dictionaries (`words.txt`, `paranormal.txt`, `short.txt`).

## Build & Flash
1) From this folder: `pio run` to build.
2) Flash firmware: `pio run -t upload`.
3) Upload SPIFFS data: `pio run -t uploadfs`.
4) Serial monitor (115200 baud): `pio device monitor`.

## Runtime Notes
- Letter cadence uses `SAMPLE_PERIOD_MS` and `STABLE_SAMPLES_REQUIRED` (shared `config_core.h`).
- Touch gestures: tap radar to rescan WiFi; swipe right-to-left to toggle Settings.
- If touch coordinates drift, recalibrate the `TS_*` values in `BoardPins.h`.

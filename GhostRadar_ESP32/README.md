# GhostRadar ESP32

GhostRadar_ESP32 is a PlatformIO/Arduino project that blends sensor noise (DHT11 temperature/humidity, MPU6050 motion, ESP32 hall sensor) with nearby WiFi activity to generate letter entropy, render it on a 2.4" ILI9341 display, and surface matching words from a SPIFFS dictionary. A simple touch UI lets you clear the buffer or force WiFi rescans.

## Hardware & Pinout
- ESP32 DevKit-style board.
- DHT11 data `GPIO27`.
- MPU6050 I2C `SDA=25`, `SCL=33`.
- ILI9341 TFT: `CS=5`, `DC=2`, `RST=4`, `MOSI=23`, `MISO=19`, `SCK=18`.
- XPT2046 touch: `CS=22` (IRQ unused), shares the SPI bus with the TFT.
- SD card (HSPI): `CS=15`, `MOSI=13`, `MISO=12`, `SCK=14`.
- Touch calibration lives in `include/config.h` (`TS_MINX/TS_MAXX/TS_MINY/TS_MAXY`).

See `pin_assignments.md` for a copy/paste reference.

## Project Layout
- `src/` core modules: `main.cpp`, `display.cpp`, `sensors.cpp`, `dictionary.cpp`, `wifiradar.cpp`, `touchui.cpp`, `Settings.cpp`, `SDManager.cpp`.
- `include/` headers for the above plus vendor configs for TFT_eSPI.
- `data/words.txt` default SPIFFS dictionary (upload with `pio run -t uploadfs`).
- `sd_config_tool.py` desktop helper to generate SD card `config/system.json`.

## Build & Flash
Requires PlatformIO (VS Code extension or `pio` CLI).

1) Install dependencies: `pio run` (fetches libraries declared in `platformio.ini`).
2) Flash firmware: `pio run -t upload`.
3) Upload SPIFFS data (dictionary): `pio run -t uploadfs`.
4) Optional: serial monitor at 115200 baud: `pio device monitor`.

## Runtime & UI
- Letter cadence is driven by `SAMPLE_PERIOD_MS` and `STABLE_SAMPLES_REQUIRED` (`include/config.h`).
- Letters accumulate until they match a dictionary entry; hits overlay the radar and push into the recents panel.
- Touch gestures:
  - Tap radar area → force WiFi rescan (also bumps entropy).
  - Swipe right-to-left anywhere → toggle Settings screen.
- Settings screen rows (tap left/right halves):
  - Brightness level (0–3 PWM/scale).
  - Active dictionary (SPIFFS `/words.txt`, `/paranormal.txt`, `/short.txt` with fallback list).
  - Entropy variance scaling (0.5–2.0).

## SD Card & Logging
- SD card is optional but enables event/session logging and SD-backed `config/system.json`.
- `sd_config_tool.py` mirrors the firmware defaults and clamps brightness to the 0–3 range used by the device.
- Firmware creates `/config`, `/logs`, `/logs/sessions`, `/dictionary`, `/ui` on first boot with SD inserted.
- Session CSV logs live under `/logs/sessions/<timestamp>.csv`; event log at `/logs/events.log`.

## Customization
- Edit `data/words.txt` before running `uploadfs` to ship a custom word list.
- Adjust entropy weights/thresholds in `src/sensors.cpp` to change letter generation dynamics.
- Swap complications (small radar labels) via SD config or by editing defaults in `Settings_loadDefaults()`.

## Notes & Pitfalls
- `Sensors_getBatteryPercent()` is currently a stub (always 100%) until a battery ADC line is wired.
- WiFi runs in STA/scan-only mode; no credentials are stored or joined.
- If touch coordinates drift, recalibrate the `TS_*` constants in `include/config.h`.
- Vendored TFT/Widget libraries live under `include/` purely for offline builds; PlatformIO still pulls the versions pinned in `platformio.ini`.

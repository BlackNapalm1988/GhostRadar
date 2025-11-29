# 📡 GhostRadar — Multi-Board ESP32 Firmware

**GhostRadar** is a modular ESP32 firmware system that fuses environmental sensor noise (DHT11, MPU6050, ESP32 hall sensor) with WiFi-scan entropy to generate letters, detect words from a dictionary, and render a dynamic UI on an SPI TFT display.

This repository supports **multiple hardware targets** using a shared core under `shared/` and per-board PlatformIO projects under `boards/`.

---

# 🏗 Repository Structure

```
GhostRadar/
│
├── shared/                 # Board-agnostic core logic (C++ source + headers)
│   ├── src/                # Display, Sensors, TouchUI, WifiRadar, SDManager, etc.
│   └── include/            # Shared headers + BoardConfig.h interface
│
├── boards/
│   ├── esp_wroom_32/       # Full PlatformIO project for ESP32 DevKit (ILI9341 build)
│   │   ├── src/            # main.cpp + BoardConfig.cpp (board glue)
│   │   ├── include/        # BoardPins.h (pins, buses, calibration)
│   │   ├── data/           # SPIFFS filesystem dictionaries
│   │   └── platformio.ini  # Board-local PIO configuration
│   │
│   └── <other_board>/      # Each additional board follows the same pattern
│
├── sd_config_tool.py       # Desktop helper to generate and edit SD JSON config
└── README.md               # (this file)
```

---

# 🧠 Architecture Overview

GhostRadar is structured into **three layers**:

---

## 1. Shared Core (`shared/`)

Contains all board-agnostic logic:

### Display Subsystem
- Drawing routines
- Heartbeat strip
- Static radar frame
- Status bar
- Color palette

### Touch UI Subsystem
- Calibration
- Touch event processing
- UI mode switching

### Sensors Subsystem
- DHT11 temperature/humidity
- MPU6050 motion with smoothing/variance
- ESP32 hall sensor
- WiFi RSSI variance (entropy driver)

### WifiRadar Subsystem
- WiFi scanning task
- Entropy extraction
- Radar rendering
- Complications (temperature, humidity, WiFi %, battery)

### Dictionary Subsystem
- Sliding letter buffer
- Matching against dictionary files
- Popup animation on detected words

### Settings Subsystem
- Brightness
- Logging level
- Complication configuration
- Variance/sensitivity
- JSON config loading from SD

### SDManager
- Mount SD card
- Load/save JSON system config
- Session logging
- Event logging

### Board Integration API
`shared/include/BoardConfig.h` defines the hardware abstraction:

```
Board_initPins()
Board_initDisplay()
Board_initTouch()
Board_initSensors()
Board_getSdSpi()
Board_getSdCsPin()
Board_getName()
```

Each board implements these in its own folder.

---

## 2. Board Layer (`boards/<boardname>/`)

Each board project contains:

### `src/main.cpp`
Initializes hardware via the BoardConfig API, then calls the shared subsystem `*_begin()` functions.

### `src/BoardConfig.cpp`
Implements wiring to shared core:
- SPI/TFT wiring
- Touch controller wiring
- I2C lines for sensors
- SD SPI bus and CS pin

### `include/BoardPins.h`
Holds all per-board pin definitions and calibration constants.

### `data/`
SPIFFS dictionaries and optional system JSON.

### `platformio.ini`
Board-specific PlatformIO config, pulling in the shared core via `src_dir` and `src_filter`.

---

## 3. SD Config Tool

`sd_config_tool.py` allows editing:
- Logging levels  
- Enabled complications  
- Brightness  
- Dictionary selection  
- Sensitivity/variance scaling  
- Custom labels  
- SD paths  

Useful for end-users without coding knowledge.

---

# 🚀 Building for ESP-WROOM-32

### Build & Flash
```bash
cd boards/esp_wroom_32/
pio run
pio run -t upload
```

### Upload SPIFFS filesystem
```bash
pio run -t uploadfs
```

### Serial Monitor
```bash
pio device monitor -b 115200
```

### VS Code (recommended)
1. Open folder `boards/esp_wroom_32/`
2. Use PlatformIO Sidebar:
   - ▶ **Build**
   - ➤ **Upload**
   - 📁 **Upload Filesystem**
   - 🔍 **Monitor**

---

# 🛠 Customizing for Hardware

To change wiring or hardware features:

### Edit board pins
```
boards/<board>/include/BoardPins.h
```

### Implement board-specific hardware initialization
```
boards/<board>/src/BoardConfig.cpp
```

### Shared constants / layout
```
shared/include/config_core.h
```

---

# ➕ Adding a New Board

1. Copy an existing board folder:
   ```
   cp -r boards/esp_wroom_32 boards/my_new_board
   ```

2. Update PlatformIO config:
   ```
   boards/my_new_board/platformio.ini
   ```

3. Adjust pins:
   ```
   boards/my_new_board/include/BoardPins.h
   ```

4. Implement wiring logic:
   ```
   boards/my_new_board/src/BoardConfig.cpp
   ```

5. Add SPIFFS files (optional):
   ```
   boards/my_new_board/data/
   ```

6. Build & flash:
   ```bash
   cd boards/my_new_board
   pio run
   ```

---

# 📁 Dictionaries

Each board may include SPIFFS files:

```
words.txt
paranormal.txt
short.txt
system.json
```

Upload using:
```bash
pio run -t uploadfs
```

---

# 🔍 Logging & SD Behavior

GhostRadar writes:

### **Session logs**
Rows of timestamped entropy, temperature, humidity, motion, letter, and matched words.

### **Event logs**
Key events (boot, settings load, SD issues).

### **System config**
Automatically generated JSON if missing.

Logging level is configurable through:
- SD JSON (`system.json`)
- Settings defaults

---

# 🧩 Entropy System Overview

Entropy comes from:

- WiFi RSSI variance
- Temperature fluctuations
- Humidity changes
- Accelerometer motion variance
- Hall sensor noise
- Timing jitter

GhostRadar generates letters continuously:
- Missing entropy → “-”
- Letters form a buffer
- Buffer is checked against dictionary entries

---

# 🖥 Display Overview

Supports ILI9341 displays with:

- Header (status bar)
- Radar visualization
- Heartbeat strip (sliding entropy buffer)
- Popup matched words
- Configurable brightness

---

# ❤️ Credits

Uses:

- ESP32 Arduino Core  
- Adafruit GFX + Adafruit ILI9341  
- XPT2046 Touchscreen  
- DHT library  
- MPU6050 library  
- ArduinoJson  
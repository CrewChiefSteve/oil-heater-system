# Smart Oil Heater Controller — Project Documentation

**Project:** Smart Oil Heater Controller  
**Author:** Steve (crewchiefsteve)  
**Date:** December 30-31, 2025  
**Platform:** PlatformIO with Arduino framework  
**Purpose:** Thermostat controller for race car oil pre-heating

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Hardware Evolution](#2-hardware-evolution)
3. [Final Hardware Configuration](#3-final-hardware-configuration)
4. [Problems Encountered and Solutions](#4-problems-encountered-and-solutions)
5. [Software Architecture](#5-software-architecture)
6. [PlatformIO Configuration](#6-platformio-configuration)
7. [Pin Mappings](#7-pin-mappings)
8. [Calibration Data](#8-calibration-data)
9. [Lessons Learned](#9-lessons-learned)
10. [Future Enhancements](#10-future-enhancements)

---

## 1. Project Overview

### Purpose

This project creates a standalone thermostat controller for pre-heating race car engine oil before track sessions. Warm oil provides better lubrication at startup and reduces engine wear during cold starts.

### Features

- Real-time temperature monitoring via K-type thermocouple
- Touchscreen interface for setpoint adjustment
- Thermostat logic with configurable hysteresis
- Safety shutoff at maximum temperature
- Relay control for heater element
- Visual status display (current temp, setpoint, heater state)

### Use Case

Before a race session, the oil heater warms the engine oil to a target temperature (typically 180-200°F). The controller automatically cycles the heater to maintain temperature, preventing overheating while ensuring the oil reaches optimal viscosity.

---

## 2. Hardware Evolution

This project went through three hardware iterations before finding a working solution.

### Attempt 1: Waveshare ESP32-S3-Touch-LCD-7

**Board:** Waveshare ESP32-S3-Touch-LCD-7 (7" 800×480 display)

**Why chosen:**
- Large, high-resolution display
- ESP32-S3 with plenty of GPIO
- Built-in touch, CAN, RS485, I2C headers
- Professional appearance

**What happened:**
- Discovered a critical bug in Arduino-ESP32 3.x / ESP-IDF 5.5.x
- I2C multi-byte write operations fail with `ESP_ERR_INVALID_STATE` (error 259)
- Single-byte writes work; 2+ byte writes fail
- MCP23017 requires 2-byte writes for ALL operations (register address + data)
- Bug is in the new `esp32-hal-i2c-ng.c` driver

**Evidence:**
```
i2c_master_transmit: bus=1 addr=0x20 handle=0x3fced72c size=1  → OK
i2c_master_transmit: bus=1 addr=0x20 handle=0x3fced72c size=2  → FAIL (259)
```

**Attempts to fix:**
- Different I2C bus (Wire vs Wire1)
- Different GPIO pins
- Lower I2C frequencies (10kHz, 50kHz, 100kHz)
- Direct ESP-IDF API calls (same failure)
- Retry logic with delays

**Conclusion:** Bug is fundamental to the ESP-IDF 5.5.x I2C driver on ESP32-S3. Would require downgrading to Arduino-ESP32 2.0.17 and losing Waveshare board support.

**Status:** Abandoned for this project. May revisit with older platform version.

---

### Attempt 2: ESP32-2432S028 (CYD 2.8")

**Board:** ESP32-2432S028 "Cheap Yellow Display" (2.8" 320×240 display)

**Why chosen:**
- Original ESP32 (not S3) — no I2C driver bug
- Integrated display + touch + SD card
- Well-documented by maker community
- Inexpensive (~$15)

**What happened:**
- I2C worked immediately with MCP23017
- Display worked after configuration
- Touch controller completely unresponsive
- XPT2046 returning maxed-out values (4095, 4095, 4095)
- Multiple touch library configurations attempted
- Touch SPI communication failing

**Attempts to fix:**
- Different touch libraries
- Various SPI frequencies
- Pin reconfiguration
- TFT_eSPI built-in touch support

**Conclusion:** Possible hardware defect or incompatible touch controller variant.

**Status:** Abandoned. Board saved for non-touch projects.

---

### Attempt 3: ESP32 3.5" CYD (ST7796) ✅ SUCCESS

**Board:** ESP32 3.5" CYD with ST7796 driver (3.5" 480×320 display)

**Why chosen:**
- Same ESP32 platform (no I2C bug)
- Larger display than 2.8"
- Resistive touch (RTP) with XPT2046
- Compatible with existing wiring

**Result:** Fully functional after resolving several issues (see Section 4).

---

## 3. Final Hardware Configuration

### Main Controller

| Component | Specification |
|-----------|---------------|
| Board | ESP32 3.5" CYD |
| MCU | ESP32-WROOM-32 |
| Display | 3.5" TFT, ST7796 driver, 480×320 |
| Touch | Resistive (XPT2046), shared SPI |
| Flash | 4MB |
| Framework | Arduino via PlatformIO |

### External Components

| Component | Model | Interface |
|-----------|-------|-----------|
| I/O Expander | MCP23017 | I2C @ 0x27 |
| Thermocouple Interface | MAX6675 | Bit-bang SPI via MCP23017 |
| Relay Module | Generic 3.3V logic | GPIO via MCP23017 |
| Temperature Sensor | K-Type Thermocouple | MAX6675 |

### Wiring Summary

```
ESP32 3.5" CYD                    MCP23017
┌─────────────────┐               ┌─────────────────┐
│ GPIO32 (SDA) ───┼───────────────┤ SDA             │
│ GPIO25 (SCL) ───┼───────────────┤ SCL             │
│ 3.3V ───────────┼───────────────┤ VDD             │
│ GND ────────────┼───────────────┤ VSS             │
└─────────────────┘               │ A0,A1,A2 → HIGH │ (Address 0x27)
                                  │                 │
                                  │ GPA0 ───────────┼──→ Relay IN
                                  │ GPA1 ───────────┼──→ MAX6675 SCK
                                  │ GPA2 ───────────┼──→ MAX6675 CS
                                  │ GPA3 ───────────┼──→ MAX6675 SO
                                  └─────────────────┘
```

---

## 4. Problems Encountered and Solutions

### Problem 1: MCP23017 Not Found

**Symptom:** `ERROR: MCP23017 not found!` at startup

**Diagnosis:** Added I2C bus scanner that tested all addresses 0x00-0x7F at multiple frequencies.

**Root Cause:** MCP23017 module had address pins A0, A1, A2 all pulled HIGH, setting address to 0x27 instead of expected 0x20.

**Solution:** Changed `MCP23017_ADDR` from `0x20` to `0x27` in config.h.

**Lesson:** Always scan the I2C bus first. Don't assume default addresses.

---

### Problem 2: Stack Overflow Crash

**Symptom:** 
```
Stack smashing protect failure!
Backtrace: 0x4008398d:0x3ffc5630...
Rebooting...
```

**Diagnosis:** Crash occurred after successful initialization, during display operations.

**Root Cause:** 
- TFT_eSPI FreeFonts consuming excessive stack memory
- Large local arrays in display functions
- Default Arduino loop stack size (8KB) insufficient

**Solution:**
1. Increased stack size: `-D CONFIG_ARDUINO_LOOP_STACK_SIZE=16384`
2. Removed heavy fonts: LOAD_FONT6, LOAD_FONT7, LOAD_FONT8
3. Disabled smooth fonts: Removed SMOOTH_FONT
4. Changed local buffers to global static
5. Replaced sprintf() with snprintf() using global buffer

**Lesson:** ESP32 with TFT_eSPI needs careful memory management. Avoid large fonts and stack allocations.

---

### Problem 3: Display Blank (No Output)

**Symptom:** Serial monitor showed successful initialization, but display remained black.

**Diagnosis:** Added RGB test sequence (fill red, green, blue, white).

**Root Cause:** 
1. SPI frequency too high (was 55MHz)
2. Backlight not being activated

**Solution:**
1. Reduced SPI_FREQUENCY to 40MHz
2. Added explicit backlight control:
   ```cpp
   pinMode(21, OUTPUT);
   digitalWrite(21, HIGH);
   ```

**Lesson:** Start with conservative SPI frequencies. Always verify backlight control.

---

### Problem 4: Touch Not Responding (2.8" CYD)

**Symptom:** Touch values stuck at maximum (4095, 4095, 4095) or zero.

**Diagnosis:** Raw SPI communication test showed no valid data from XPT2046.

**Root Cause:** Unknown — possibly defective touch controller or incompatible variant.

**Solution:** Switched to 3.5" CYD board with different touch hardware.

**Lesson:** Have backup hardware. Some boards have quality control issues.

---

### Problem 5: Touch Not Responding (3.5" CYD)

**Symptom:** Same as above — touch values maxed out.

**Diagnosis:** Examined board pinout documentation.

**Root Cause:** Touch SPI pins were different from 2.8" board. Touch shares SPI bus with display but documentation was incomplete.

**Actual Touch Pins (3.5" CYD):**
- T_CLK = GPIO14 (shared with display)
- T_MOSI = GPIO13 (shared with display)
- T_MISO = GPIO12 (shared with display)
- T_CS = GPIO33
- T_IRQ = GPIO36

**Solution:**
1. Removed separate XPT2046_Touchscreen library
2. Used TFT_eSPI's built-in touch support
3. Added proper build flags:
   ```
   -D TOUCH_CS=33
   -D TOUCH_IRQ=36
   ```
4. Used `tft.getTouch()` instead of separate library

**Lesson:** Different board variants have different pinouts. Verify against actual board silkscreen or schematic.

---

### Problem 6: Touch Coordinates Wrong

**Symptom:** Touch registered but coordinates didn't match screen positions.

**Solution:** Used TFT_eSPI calibration routine:
```cpp
tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
```

Stored calibration values and applied with `tft.setTouch(calData)`.

**Lesson:** Resistive touch always needs calibration. Save calibration values.

---

## 5. Software Architecture

### File Structure

```
SmartHeaterController/
├── platformio.ini          # Build configuration, libraries, flags
├── include/
│   └── config.h            # Hardware defines, pin mappings, constants
├── src/
│   └── main.cpp            # Main application code
├── lib/                    # Project-specific libraries (empty)
├── test/                   # Unit tests (empty)
├── .gitignore
└── README.md
```

### Code Organization (main.cpp)

```
1. Includes and Global Objects
   - TFT_eSPI, Wire, Adafruit_MCP23X17
   - State variables (temperature, setpoint, heater state)

2. Function Prototypes

3. setup()
   - Serial initialization
   - Backlight control
   - I2C initialization
   - MCP23017 initialization
   - Display initialization
   - Touch calibration

4. loop()
   - Temperature reading (periodic)
   - Thermostat logic
   - Touch handling
   - Display update (periodic)

5. Hardware Functions
   - initMCP23017()
   - readMAX6675()
   - setHeater()

6. Display Functions
   - initDisplay()
   - updateDisplay()
   - drawUI()
   - drawButton()

7. Touch Functions
   - handleTouch()

8. Safety Functions
   - emergencyShutdown()
```

### Thermostat Logic

```
                    Setpoint = 180°F
                    Hysteresis = ±5°F
                    
     175°F                    185°F
       │                        │
       ▼                        ▼
   ────┴────────────────────────┴────
       │      DEADBAND          │
       │    (no change)         │
       │                        │
  HEAT ON                  HEAT OFF
  (temp rising)            (temp falling)
```

The heater turns ON when temperature drops below (setpoint - hysteresis) and turns OFF when temperature rises above (setpoint + hysteresis). This prevents rapid cycling.

---

## 6. PlatformIO Configuration

### platformio.ini (Final)

```ini
[env:cyd35]
platform = espressif32@6.4.0
board = esp32dev
framework = arduino

monitor_speed = 115200
upload_speed = 921600

build_flags = 
    ; Stack size (prevent overflow with TFT_eSPI)
    -D CONFIG_ARDUINO_LOOP_STACK_SIZE=16384
    
    ; Debug level
    -D CORE_DEBUG_LEVEL=3
    
    ; TFT_eSPI Configuration
    -D USER_SETUP_LOADED=1
    -D ST7796_DRIVER=1
    -D TFT_WIDTH=320
    -D TFT_HEIGHT=480
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=21
    
    ; Touch Configuration
    -D TOUCH_CS=33
    -D TOUCH_IRQ=36
    
    ; SPI Frequency (40MHz stable)
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=20000000
    -D SPI_TOUCH_FREQUENCY=2500000
    
    ; Fonts (minimal set to save memory)
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D LOAD_GFXFF=1

lib_deps = 
    bodmer/TFT_eSPI@^2.5.43
    adafruit/Adafruit MCP23017 Arduino Library@^2.3.0
    adafruit/Adafruit BusIO@^1.16.1
```

### Key Configuration Notes

| Setting | Value | Reason |
|---------|-------|--------|
| platform | espressif32@6.4.0 | Stable release, tested |
| CONFIG_ARDUINO_LOOP_STACK_SIZE | 16384 | Prevent stack overflow |
| ST7796_DRIVER | 1 | Correct driver for 3.5" display |
| SPI_FREQUENCY | 40000000 | 40MHz stable (55MHz caused issues) |
| TOUCH_CS | 33 | 3.5" CYD touch chip select |
| TOUCH_IRQ | 36 | 3.5" CYD touch interrupt |
| Fonts | GLCD, 2, 4 only | Reduced memory footprint |

---

## 7. Pin Mappings

### ESP32 3.5" CYD Pin Usage

| GPIO | Function | Notes |
|------|----------|-------|
| 2 | TFT_DC | Display data/command |
| 12 | TFT_MISO / T_MISO | Shared SPI |
| 13 | TFT_MOSI / T_MOSI | Shared SPI |
| 14 | TFT_SCLK / T_CLK | Shared SPI |
| 15 | TFT_CS | Display chip select |
| 21 | TFT_BL | Backlight control |
| 25 | I2C_SCL | To MCP23017 |
| 32 | I2C_SDA | To MCP23017 |
| 33 | TOUCH_CS | Touch chip select |
| 36 | TOUCH_IRQ | Touch interrupt |

### MCP23017 Pin Usage

| Pin | GPIO | Function |
|-----|------|----------|
| GPA0 | 0 | Relay control (active LOW) |
| GPA1 | 1 | MAX6675 SCK |
| GPA2 | 2 | MAX6675 CS |
| GPA3 | 3 | MAX6675 SO (data in) |
| GPA4-7 | 4-7 | Available |
| GPB0-7 | 8-15 | Available |

### MCP23017 I2C Address

Address pins all HIGH → Address = **0x27**

| A2 | A1 | A0 | Address |
|----|----|----|---------|
| H | H | H | 0x27 |

---

## 8. Calibration Data

### Touch Calibration

Calibration values determined using `tft.calibrateTouch()`:

```cpp
uint16_t calData[5] = {XXX, XXX, XXX, XXX, X};  // Your actual values
tft.setTouch(calData);
```

**Note:** Re-run calibration if touch accuracy drifts or after changing display rotation.

### Temperature Calibration

MAX6675 provides 0.25°C resolution. No software calibration applied.

If offset needed:
```cpp
float calibrationOffset = 0.0;  // Adjust if readings are off
float correctedTemp = rawTemp + calibrationOffset;
```

---

## 9. Lessons Learned

### Hardware Selection

1. **Check for known bugs** before committing to a platform. The ESP32-S3 I2C bug cost significant debugging time.

2. **Have backup hardware.** The first CYD board's touch was defective.

3. **Verify actual pinouts** on the physical board. Documentation can be wrong or for different revisions.

4. **I2C addresses aren't always default.** Scan the bus first.

### Software Development

1. **PlatformIO over Arduino IDE** for serious projects. Per-project libraries prevent version conflicts.

2. **TFT_eSPI configuration via build flags** is cleaner than editing User_Setup.h.

3. **Start with diagnostics.** I2C scanner, display test patterns, and raw touch output saved hours.

4. **Watch stack usage** on ESP32 with displays. Heavy fonts and local buffers cause crashes.

5. **Conservative SPI frequencies** first. Increase only after confirming stability.

### Debugging Process

1. **Isolate problems.** Test I2C alone, then display alone, then touch alone.

2. **Add verbose logging** during development. Remove for production.

3. **Use oscilloscope** to verify signals when software debugging hits a wall.

4. **Read error codes carefully.** `ESP_ERR_INVALID_STATE (259)` pointed directly to the I2C driver bug.

---

## 10. Future Enhancements

### Planned Improvements

| Feature | Priority | Notes |
|---------|----------|-------|
| WiFi monitoring | Medium | View temps from phone in pits |
| EEPROM setpoint storage | High | Persist settings across power cycles |
| Multiple zones | Low | Use remaining MCP23017 GPIOs |
| Data logging to SD | Medium | Temperature history |
| OTA updates | Low | Firmware updates without USB |

### Available Resources

- **MCP23017 GPIOs:** 12 unused (GPA4-7, GPB0-7)
- **ESP32 GPIOs:** Several still available
- **Flash:** ~3MB remaining after code

### Waveshare 7" Revisit

A separate project (SmartHeaterController-Waveshare7) exists to attempt getting the 7" display working by:

1. Testing older platform versions (espressif32@5.4.0, @4.4.0)
2. Trying Arduino-ESP32 2.0.17 equivalent
3. Potentially using onboard CH422G instead of external MCP23017

---

## Appendix A: Serial Debug Output (Normal Operation)

```
Smart Oil Heater Controller
============================
Initializing I2C...
I2C initialized on SDA=32, SCL=25
Initializing MCP23017...
MCP23017 initialized ✓
Initializing display...
Display initialized ✓
Initializing touchscreen...
Touchscreen initialized ✓
Relay set to OFF ✓
Initialization complete!
Default setpoint: 180.0°F

Temp: 68.2°F (20.1°C) [Heater: OFF]
Temp: 68.4°F (20.2°C) [Heater: OFF]
...
```

---

## Appendix B: Error Reference

| Error | Meaning | Solution |
|-------|---------|----------|
| MCP23017 not found | I2C communication failed | Check wiring, scan for correct address |
| Stack smashing | Stack overflow | Increase stack size, reduce memory usage |
| Thermocouple not connected | MAX6675 bit 2 set | Check thermocouple wiring |
| NACK on address (2) | Device not present | Wrong address or wiring issue |
| ESP_ERR_INVALID_STATE (259) | I2C driver bug (ESP32-S3) | Use different platform version or board |

---

## Appendix C: Parts List

| Item | Quantity | Notes |
|------|----------|-------|
| ESP32 3.5" CYD (ST7796) | 1 | Main controller |
| MCP23017 module | 1 | Address 0x27 |
| MAX6675 module | 1 | Thermocouple interface |
| K-type thermocouple | 1 | Temperature sensor |
| Relay module (3.3V logic) | 1 | Heater control |
| Dupont wires | ~10 | Connections |
| USB cable | 1 | Power and programming |
| 4.7kΩ resistors | 2 | I2C pull-ups (if not on module) |

---

*Document created December 31, 2025*
*For questions or updates, reference the GitHub repository or project files.*

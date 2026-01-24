# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Workspace Overview

This workspace contains **two separate heater controller implementations**:

1. **SmartHeaterController/** - Single ESP32 with integrated 3.5" touchscreen (all-in-one design)
2. **oil-heater-system/** - Dual-board system with ESP32 controller and ESP32-S3 display (ESP-NOW wireless architecture)

Both systems control oil heaters for race car applications but use different hardware architectures and communication methods.

## Build and Upload Commands

All projects use PlatformIO. Use `pio` if PlatformIO Core CLI is in PATH, or `python -m platformio` otherwise.

### SmartHeaterController (Single ESP32)

```bash
cd SmartHeaterController

# Build the project
pio run

# Upload to ESP32 (auto-detects port)
pio run --target upload

# Upload to specific port (Windows example)
pio run --target upload --upload-port COM3

# Upload and monitor serial output (115200 baud)
pio run --target upload && pio device monitor

# Clean build files
pio run --target clean

# Monitor serial output only
pio device monitor
```

### oil-heater-system (Dual Board ESP-NOW)

This system has TWO separate firmware projects that must be built and uploaded independently:

```bash
# Build and upload CONTROLLER (main logic + thermocouple)
cd oil-heater-system/controller
pio run --target upload
pio device monitor

# Build and upload UI (7" touchscreen interface)
cd oil-heater-system/ui
pio run --target upload
pio device monitor
```

**CRITICAL SETUP**: After flashing the controller, copy the MAC address from serial output and update `CONTROLLER_MAC` in `ui/src/main.cpp` (line ~26) before flashing the UI board. Both ESP32 boards communicate wirelessly via ESP-NOW (~250Hz update rate).

## Project Architectures

### SmartHeaterController (All-in-One)

**Hardware**:
- ESP32-2432S028R (3.5" CYD - Cheap Yellow Display)
- ST7796 480x320 TFT with XPT2046 resistive touch
- MCP23017 I/O expander (I2C 0x27) - handles relay + MAX6675 SPI
- MAX6675 thermocouple interface (via MCP23017 bit-bang SPI)
- Single relay module (active HIGH)

**Key Design Choice**: Uses MCP23017 to free up ESP32 GPIOs. The MAX6675 is connected to MCP23017 pins and uses bit-banged SPI instead of native ESP32 SPI.

**Pin Configuration**:
- I2C Bus: SDA=GPIO32, SCL=GPIO25 (4.7kΩ pull-ups recommended)
- Display SPI: Defined in platformio.ini build_flags (TFT_MISO=12, TFT_MOSI=13, TFT_SCLK=14, TFT_CS=15)
- Display DC: GPIO2, TFT_RST=-1 (not used), Backlight: GPIO27
- Touch: TOUCH_CS=GPIO33, TOUCH_IRQ=GPIO36

**Software Architecture**:
- Single main.cpp with loop-based control
- Direct UI rendering with TFT_eSPI
- Three periodic tasks: temp read (1s), thermostat logic, display update (500ms)
- Touch input with debouncing (200ms)
- BLE support: broadcasts temp/setpoint/status as "Heater_Controller"

**Configuration**: All settings in `include/config.h` including:
- DEFAULT_SETPOINT_F: 180°F
- TEMP_HYSTERESIS_F: 1°F (±1°F band)
- RELAY_MIN_CYCLE_TIME: 10s
- SAFETY_MAX_TEMP_F: 300°F

**Touch Calibration**: Send 'c' or 'C' via serial to calibrate. Calibration data stored in main.cpp line ~51.

**Critical Implementation**: `bitBangSPI()` function reads MAX6675 through MCP23017 I2C. This is slower than native SPI but adequate for 1-second temperature reads.

### oil-heater-system (Dual Board Wireless System)

**Architecture**: Two ESP32 boards connected wirelessly via ESP-NOW, separating control logic from UI. No physical wiring between boards required.

#### Controller Board (oil-heater-system/controller/)

**Hardware**:
- ESP32 DevKit v1 (original ESP32, NOT S3)
- MAX6675 thermocouple (direct GPIO software SPI)
- Single relay output (direct GPIO, no I/O expander)
- ESP-NOW for wireless communication to UI board

**Pin Configuration** (hardcoded in main.cpp):
- Thermocouple: SCK=GPIO18, CS=GPIO5, SO=GPIO19
- Relay: GPIO23 (active HIGH by default, configurable via RELAY_ACTIVE_HIGH constant)

**Software Components**:
- Single main.cpp file (~250 lines)
- Adafruit MAX6675 library for thermocouple reading
- ESP-NOW wireless protocol for status broadcast (250ms interval)
- Bang-bang thermostat with hysteresis (2°C deadband)

**Control Parameters** (in main.cpp):
- DEFAULT_SETPOINT_C: 110°C (~230°F)
- HYSTERESIS_C: 2°C
- MAX_SAFE_TEMP_C: 160°C (~320°F)
- CMD_TIMEOUT_MS: 5000ms (safety watchdog)

**Key Design Choice**: Uses original ESP32 (not S3) to avoid I2C driver issues. MAX6675 connected directly to ESP32 GPIOs using Adafruit library (not bit-banged). No protocol library needed - ESP-NOW data structures defined in main.cpp.

#### Display Board (oil-heater-system/ui/)

**Hardware**:
- Waveshare ESP32-S3-Touch-LCD-7 (7" 800x480 RGB display)
- ST7262 RGB parallel panel with GT911 capacitive touch
- PSRAM required for LVGL frame buffers
- ESP-NOW for wireless communication to controller board

**Pin Configuration**:
- RGB display: 16 data pins + sync/clock (defined in main.cpp)
- Touch I2C: GT911 on I2C bus (address 0x5D or 0x14)
- No UART pins needed (wireless communication)

**Software Components**:
- Single main.cpp file (~600 lines)
- Direct LVGL UI implementation (no separate ui.h/cpp files)
- ESP-NOW receiver for controller status updates
- esp_lcd_panel_rgb for direct RGB panel control (no LovyanGFX)

**Key Libraries**:
- LVGL 8.3.11: UI framework (not 9.x as stated in README.md)
- No LovyanGFX or Waveshare library - uses ESP-IDF esp_lcd APIs directly

**Critical Setup**: Must define CONTROLLER_MAC in main.cpp before flashing. Controller broadcasts its MAC on first boot.

### Communication Protocol (oil-heater-system only)

The controller and display communicate via **ESP-NOW wireless protocol** at ~250Hz using simple C structs (not framed serial protocol).

**Data Structures** (defined in each main.cpp):

**Controller → Display**:
```cpp
struct StatusData {
    float currentTemp;
    float targetTemp;
    bool heaterState;
    uint8_t faultCode;
    uint32_t timestamp;
};
```

**Display → Controller**:
```cpp
struct CommandData {
    uint8_t cmd;       // CMD_SET_TEMP, CMD_ENABLE, CMD_DISABLE
    float value;
    uint32_t timestamp;
};
```

**Protocol Implementation**:
- No checksums or framing - ESP-NOW handles data integrity
- Broadcast-based (no ACK required for status updates)
- Peer pairing required on first boot
- 250ms status broadcast interval from controller

## Key Dependencies

### SmartHeaterController
- `bodmer/TFT_eSPI@^2.5.43`: Display driver (configured via build_flags, not User_Setup.h)
- `adafruit/Adafruit MCP23017 Arduino Library@^2.3.0`: I/O expander
- `adafruit/Adafruit BusIO@^1.16.1`: I2C/SPI abstraction

### oil-heater-system/controller
- `adafruit/MAX6675 library@^1.1.2`: Thermocouple interface
- ESP-NOW (built into ESP32 Arduino core)

### oil-heater-system/ui
- `lvgl/lvgl@^8.3.11`: UI framework (version 8.x, not 9.x)
- ESP-IDF esp_lcd APIs (built into ESP32-S3 Arduino core)
- ESP-NOW (built into ESP32 Arduino core)

## Choosing Which Project to Modify

**Use SmartHeaterController when**:
- Working with 3.5" CYD hardware (ESP32-2432S028R)
- Need compact all-in-one solution with wired connections
- BLE connectivity is required
- Cost-sensitive application (uses cheaper display)
- Single USB cable for programming and power

**Use oil-heater-system when**:
- Working with Waveshare 7" display and separate ESP32 controller
- Need larger touchscreen for better visibility in bright environments
- Want physical separation of control and UI (wireless, no interconnect wiring)
- Working around ESP32-S3 I2C issues (uses original ESP32 for controller)
- Need independent programming of controller and display

## Safety Features (Both Systems)

Both implementations include:
- Over-temperature shutdown (300°F / 160°C limit)
- Sensor error detection (disconnected thermocouple)
- Relay cycling protection (minimum 10s between state changes in SmartHeaterController, immediate in oil-heater-system)
- User acknowledgment required to reset safety shutdown

oil-heater-system additional safety:
- Watchdog timer: heater turns OFF if no UI command for 5 seconds
- Fault display on screen with visual alerts

## Development Notes

1. **ESP32-S3 I2C Issues**: The oil-heater-system uses original ESP32 for the controller (not S3) due to persistent I2C driver bugs in Arduino-ESP32 3.x with ESP32-S3. SmartHeaterController also uses original ESP32 (non-S3) which doesn't have these issues.

2. **TFT_eSPI Configuration**: SmartHeaterController uses build_flags in platformio.ini instead of editing User_Setup.h. All display and touch pins are defined there.

3. **ESP-NOW vs UART**: The oil-heater-system README.md mentions ESP-NOW wireless communication. There are NO protocol.h/protocol.cpp library files - communication uses simple structs defined in each main.cpp file.

4. **LVGL Version**: oil-heater-system uses LVGL 8.3.11, not 9.x. The API is significantly different between major versions.

5. **Display Driver**: oil-heater-system UI uses esp_lcd_panel_rgb APIs directly, not LovyanGFX or ESP_Panel libraries mentioned in README.md.

6. **Temperature Smoothing**: SmartHeaterController uses moving average (8 samples). oil-heater-system relies on MAX6675 internal averaging.

7. **Relay Logic**: Both use active HIGH by default (HIGH=ON, LOW=OFF). oil-heater-system allows configuration via RELAY_ACTIVE_HIGH constant. Relay state changes are rate-limited in SmartHeaterController to prevent mechanical wear.

## Serial Commands

### SmartHeaterController
- `c` or `C`: Run touchscreen calibration

### oil-heater-system
- No serial commands (all control via touchscreen)
- Controller prints MAC address on boot (needed for UI configuration)

## Common Modifications

### Adjusting Setpoint Range
- SmartHeaterController: Edit `include/config.h` - MIN_SETPOINT_F, MAX_SETPOINT_F
- oil-heater-system: Edit controller/src/main.cpp - MIN_SETPOINT_C, MAX_SETPOINT_C constants

### Changing Hysteresis Band
- SmartHeaterController: `TEMP_HYSTERESIS_F` in include/config.h
- oil-heater-system: `HYSTERESIS_C` constant in controller/src/main.cpp

### Modifying Safety Limits
- SmartHeaterController: `SAFETY_MAX_TEMP_F` in include/config.h
- oil-heater-system: `MAX_SAFE_TEMP_C` in controller/src/main.cpp

### Display Layout Changes
- SmartHeaterController: Modify drawing functions in src/main.cpp (search for "tft.draw" calls)
- oil-heater-system: Modify LVGL UI code in ui/src/main.cpp (single file, search for "lv_obj_create")

### Relay Pin Changes
- SmartHeaterController: Requires changing MCP23017 pin mapping in code (GPA0 default)
- oil-heater-system: Change PIN_RELAY constant in controller/src/main.cpp (GPIO23 default)

## State Machine (SmartHeaterController)

1. **Normal Operation**: Thermostat active, relay cycles based on hysteresis
2. **Safety Shutdown**: Triggered by sensor error or over-temperature (>300°F), relay locked OFF
3. **User Reset**: Adjusting setpoint clears safety shutdown (if conditions allow)

Key state transitions:
- Normal → Safety: `currentTemp > SAFETY_MAX_TEMP_F` OR `sensorError == true`
- Safety → Normal: User adjusts setpoint AND `currentTemp < SAFETY_MAX_TEMP_F` AND `sensorError == false`

Global state variables in main.cpp:
- `currentTemp`: Last temperature reading (float, Fahrenheit)
- `setpointTemp`: User-configured target temperature (float, Fahrenheit)
- `heaterOn`: Current relay state (bool)
- `sensorError`: MAX6675 error flag (bool)
- `safetyShutdown`: Emergency shutdown active (bool)

## State Machine (oil-heater-system)

Implemented in controller/src/main.cpp with simple bang-bang control:
- **Heating**: Relay ON when `temp < (setpoint - hysteresis)`
- **Off**: Relay OFF when `temp > (setpoint + hysteresis)`
- **Fault**: Relay OFF on sensor error or over-temperature

Fault codes (uint8_t in StatusData):
- 0: No fault
- 1: Over-temperature (>160°C)
- 2: Sensor disconnected/error
- 3: Watchdog timeout (no UI command for 5s)

## Troubleshooting

**SmartHeaterController**:
- "Touch not responding": Run calibration with 'c' command via serial
- "Sensor Error 999°F": Check MAX6675 wiring to MCP23017 (GPA1=SCK, GPA2=CS, GPA3=SO), verify I2C connection at 0x27
- "Relay not switching": Verify MCP23017 address (0x27), check GPA0 connection to relay IN pin
- "Display garbled": Check TFT_eSPI build_flags in platformio.ini match your CYD hardware

**oil-heater-system**:
- "No display updates / FAULT: No Controller": Check controller MAC address is correctly copied to ui/src/main.cpp CONTROLLER_MAC array
- "ESP-NOW init failed": Both boards must use same WiFi channel (default works, set explicitly if needed)
- "Display stays black": Verify PSRAM detection in serial output ("PSRAM found: XXXXX bytes"), use separate 5V/2A supply on PWR port
- "Touch not working": GT911 may be at 0x14 instead of 0x5D (check serial output for I2C scan)
- "Sensor Error": Check MAX6675 wiring directly to ESP32 GPIO pins (18, 5, 19)
- "Relay clicking rapidly": Increase HYSTERESIS_C value in controller/src/main.cpp

## File Structure

```
workspace/
├── SmartHeaterController/
│   ├── platformio.ini          # ESP32 + TFT_eSPI config
│   ├── include/
│   │   └── config.h            # All user-configurable parameters
│   └── src/
│       └── main.cpp            # Single-file implementation
│
└── oil-heater-system/
    ├── README.md               # Hardware setup guide
    ├── controller/
    │   ├── platformio.ini      # ESP32 (original, not S3)
    │   └── src/
    │       └── main.cpp        # Controller logic + ESP-NOW TX
    └── ui/
        ├── platformio.ini      # ESP32-S3 + LVGL config
        ├── include/
        │   └── lv_conf.h       # LVGL 8.x configuration
        └── src/
            └── main.cpp        # Display UI + ESP-NOW RX
```

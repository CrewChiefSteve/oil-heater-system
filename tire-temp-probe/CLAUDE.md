# CLAUDE.md - Tire Temperature Probe

This file provides guidance to Claude Code when working with the tire-temp-probe project.

## Project Overview

Wireless thermocouple-based tire and brake temperature measurement system for motorsports applications. Uses 4 MAX31855K thermocouple amplifiers to measure tire temperatures (inside/middle/outside) and brake rotor temperature. Features a small display that guides the user through a sequential corner capture workflow (RF → LF → LR → RR), transmitting completed corner data via BLE to the mobile app.

**Important**: This is the thermocouple probe system in `tire-temp-probe/`, NOT the IR gun in `tire-temp-gun/`.

## User Workflow

```
Power on → Display: "Connect to App"
    ↓
BLE connected → Display: "Start at LF"
    ↓
User places 3 probes on LF tire (outer/middle/inner)
    ↓
Temps stabilize (~1 second) → Auto-capture → Green LED → BLE sends corner data
    ↓
Display: "Go to RF" → User moves to RF
    ↓
Repeat for RF → LR → RR
    ↓
All 4 captured → Display: "Complete ✓" → Session ends
```

**Key Design**: Device controls the capture sequence (LF → RF → LR → RR). App is a passive receiver that logs data to Convex backend.

**v2 Protocol Note**: Only 3 tire temperature probes are used (outer/middle/inner). Brake temperature removed from protocol.

## Hardware Architecture

### Core Components
- **MCU**: ESP32-S3-WROOM-1
- **Thermocouples**: 4x MAX31855K amplifiers on shared SPI bus
  - 3x tire probes (inside, middle, outside of tire contact patch)
  - 1x brake rotor probe
- **Display**: SSD1306 128x64 OLED (I2C) - Shows corner prompts and status
- **Status LED**: WS2812B RGB LED (single pixel) - Capture confirmation
- **Buzzer**: Optional audio feedback on capture
- **Power**: 2000mAh LiPo battery with TP4056 USB-C charging

### Pin Configuration (include/pins.h)

**SPI Bus** (shared by all MAX31855K):
- GPIO18 = SPI_SCK
- GPIO19 = SPI_MISO (no MOSI - read-only devices)

**Chip Select pins**:
- GPIO5 = CS_TIRE_IN (inside tire probe)
- GPIO6 = CS_TIRE_MID (middle tire probe)
- GPIO7 = CS_TIRE_OUT (outside tire probe)
- GPIO15 = CS_BRAKE (brake rotor probe)

**I2C Bus** (display):
- GPIO8 = I2C_SDA
- GPIO9 = I2C_SCL

**Peripherals**:
- GPIO48 = RGB_LED (WS2812B)
- GPIO1 = VBAT_ADC (battery voltage monitoring)
- GPIO2 = CHRG_STAT (TP4056 charge status)
- GPIO47 = BUZZER (optional audio alerts)

## Software Architecture

### Modular Design

```
tire-temp-probe/
├── platformio.ini          # ESP32-S3 board config, dependencies
├── include/
│   ├── pins.h              # GPIO pin assignments
│   ├── config.h            # User-configurable parameters
│   ├── types.h             # Data structures and enums
│   ├── ble_protocol.h      # BLE packet format definitions
│   ├── probes.h            # Thermocouple reading interface
│   ├── ble_service.h       # BLE service interface
│   ├── display.h           # OLED display interface
│   ├── led.h               # Status LED interface
│   └── power.h             # Battery management interface
└── src/
    ├── main.cpp            # Setup, loop, state machine
    ├── probes.cpp          # Thermocouple reading implementation
    ├── ble_service.cpp     # NimBLE service implementation
    ├── display.cpp         # SSD1306 display implementation
    ├── led.cpp             # FastLED status indication
    └── power.cpp           # Battery monitoring implementation
```

### State Machine

```
INITIALIZING
     ↓
WAITING_CONNECTION ←──────────────────┐
     ↓ (BLE connected)                │
CORNER_RF ─→ STABILIZING_RF ─→ CAPTURED_RF
     ↓                                │
CORNER_LF ─→ STABILIZING_LF ─→ CAPTURED_LF
     ↓                                │
CORNER_LR ─→ STABILIZING_LR ─→ CAPTURED_LR
     ↓                                │
CORNER_RR ─→ STABILIZING_RR ─→ CAPTURED_RR
     ↓                                │
SESSION_COMPLETE ─────────────────────┘
     │ (BLE disconnected)
     ↓
WAITING_CONNECTION
```

**State Descriptions**:
- `INITIALIZING` - Startup, init all subsystems
- `WAITING_CONNECTION` - Display "Connect to App", BLE advertising
- `CORNER_XX` - Display "Place probes on XX", waiting for stable temps
- `STABILIZING_XX` - Temps detected, waiting for stability threshold
- `CAPTURED_XX` - Auto-captured, green LED, BLE transmit, brief pause
- `SESSION_COMPLETE` - All 4 corners done, display summary

### Module Responsibilities

#### probes.cpp / probes.h
- Initialize 4x MAX31855K thermocouple amplifiers
- Read temperatures from all probes
- Validate temperature readings (range check, NaN detection)
- Smooth temperatures using moving average
- **Detect temperature stability** (key for auto-capture trigger)
- Calculate tire average from 3 probes

**Key Functions**:
- `probesInit()` - Initialize thermocouples
- `probesUpdate(MeasurementData&)` - Read all probes
- `probesAreStable()` - Returns true when all 4 probes stable for threshold duration
- `probesGetStabilityProgress()` - Returns 0.0-1.0 for UI feedback

**Stability Detection**:
- All 4 probes must be within `TEMP_STABLE_THRESHOLD` (0.5°C) for `STABILITY_DURATION_MS` (1000ms)
- Resets if any probe changes more than threshold
- Progress can be shown on display (optional)

#### display.cpp / display.h
- Initialize SSD1306 OLED display
- Show corner prompts ("Place probes on RF")
- Show stability progress (optional progress bar)
- Show capture confirmation
- Show session complete summary
- Show battery/connection status

**Key Functions**:
- `displayInit()` - Initialize I2C and SSD1306
- `displayShowCornerPrompt(corner)` - "Place probes on RF"
- `displayShowStabilizing(progress)` - Optional progress indicator
- `displayShowCaptured(corner, temps)` - Brief capture confirmation
- `displayShowComplete()` - Session complete screen
- `displayShowWaitingConnection()` - "Connect to App"
- `displayShowError(message)` - Error state

**Display Layouts**:

*Waiting Connection*:
```
┌────────────────────┐
│   TIRE PROBE       │
│                    │
│  Connect to App    │
│                    │
│  [BLE icon] ...    │
└────────────────────┘
```

*Corner Prompt*:
```
┌────────────────────┐
│   TIRE PROBE       │
│                    │
│  Place probes on   │
│       RF           │  ← Large, centered
│                    │
│  [====    ] 40%    │  ← Optional stability bar
└────────────────────┘
```

*Captured*:
```
┌────────────────────┐
│      ✓ RF          │
│                    │
│  In: 185°F         │
│  Mid: 192°F        │
│  Out: 188°F        │
│  Brake: 340°F      │
└────────────────────┘
```

*Session Complete*:
```
┌────────────────────┐
│   SESSION COMPLETE │
│                    │
│  RF ✓  LF ✓        │
│  RR ✓  LR ✓        │
│                    │
│  Ready for next    │
└────────────────────┘
```

#### ble_service.cpp / ble_service.h
- Initialize NimBLE stack
- Create BLE service with characteristics
- Handle client connections/disconnections
- **Transmit corner reading on capture** (not continuous)
- Transmit system status periodically

**Key Functions**:
- `bleInit(deviceName)` - Initialize BLE stack and service
- `bleStartAdvertising()` - Start advertising
- `bleIsConnected()` - Check connection status
- `bleTransmitCornerReading(corner, tireIn, tireMid, tireOut, brakeTemp)` - Send captured corner
- `bleTransmitSystemStatus()` - Send battery/state (periodic)

#### led.cpp / led.h
- Initialize WS2812B RGB LED
- **Green flash on capture** - Primary feedback
- Status indication for other states

**LED States**:
- Off: Idle/measuring
- Blue breathing: Waiting for connection
- Yellow pulse: Stabilizing (temps detected)
- **Green solid (1 sec)**: Capture successful - GO signal
- Orange blink: Low battery
- Red: Error

#### power.cpp / power.h
- Read battery voltage via ADC
- Calculate battery percentage
- Detect charging status

### Data Structures (types.h)

**DeviceState** enum:
```cpp
enum DeviceState {
  STATE_INITIALIZING,
  STATE_WAITING_CONNECTION,
  STATE_CORNER_RF,
  STATE_STABILIZING_RF,
  STATE_CAPTURED_RF,
  STATE_CORNER_LF,
  STATE_STABILIZING_LF,
  STATE_CAPTURED_LF,
  STATE_CORNER_LR,
  STATE_STABILIZING_LR,
  STATE_CAPTURED_LR,
  STATE_CORNER_RR,
  STATE_STABILIZING_RR,
  STATE_CAPTURED_RR,
  STATE_SESSION_COMPLETE,
  STATE_ERROR
};
```

**Corner** enum (v2 - explicit UInt8 values):
```cpp
enum Corner {
  CORNER_LF = 0,  // Left Front (v2: start here, matches BLE CORNER_ID=0)
  CORNER_RF = 1,  // Right Front (v2: matches BLE CORNER_ID=1)
  CORNER_LR = 2,  // Left Rear (v2: matches BLE CORNER_ID=2)
  CORNER_RR = 3   // Right Rear (v2: matches BLE CORNER_ID=3)
};
```

**Corner Sequence (v2)**: LF → RF → LR → RR (updated for v2 protocol)

**CornerReading** struct (v2 - brake temp still in struct but not transmitted):
```cpp
struct CornerReading {
  Corner corner;          // Which corner (LF=0, RF=1, LR=2, RR=3)
  float tireInside;       // Inner tire temp (Celsius) → temp3 in JSON
  float tireMiddle;       // Middle tire temp (Celsius) → temp2 in JSON
  float tireOutside;      // Outer tire temp (Celsius) → temp1 in JSON
  float brakeTemp;        // Brake rotor temp (Celsius) - NOT transmitted in v2
  float tireAverage;      // Calculated average of 3 tire temps
  float tireSpread;       // max - min of 3 tire temps
  uint32_t timestamp;     // millis() at capture
};
```

**v2 Note**: Only tireInside/Middle/Outside are transmitted (as temp3/temp2/temp1). Brake temp removed from protocol.

**SessionData** struct:
```cpp
struct SessionData {
  CornerReading corners[4];  // RF, LF, LR, RR
  uint8_t capturedCount;     // 0-4
  bool isComplete;
};
```

### BLE Protocol v2 (ble_protocol.h)

**Service UUID**: `4fafc201-0004-459e-8fcc-c5c9c331914b` (unchanged)

**Characteristics (v2 - 3 total)**:

| UUID | Name | Properties | Format | Description |
|------|------|------------|--------|-------------|
| `beb5483e-36e1-4688-b7f5-ea07361b26a8` | CORNER_READING | READ, NOTIFY | JSON | Corner temp data on capture |
| `beb5483e-36e1-4688-b7f5-ea07361b26aa` | STATUS | READ, NOTIFY | JSON | System status (periodic) |
| `beb5483e-36e1-4688-b7f5-ea07361b26af` | CORNER_ID | READ, WRITE, NOTIFY | UInt8 | Corner assignment (0-3) |

**v2 Changes**:
- CORNER_READING moved from 26ac → 26a8 (primary data characteristic)
- STATUS changed from binary → JSON format
- Added CORNER_ID characteristic (26af) for corner assignment
- All NOTIFY characteristics include BLE2902 descriptors (iOS compatibility)
- Removed brake temperature from data (3 tire temps only)

**CORNER_READING Packet (JSON)**:
```json
{
  "corner": "LF",
  "temp1": 185.5,  // Outer tire temp (°F or °C)
  "temp2": 190.2,  // Middle tire temp
  "temp3": 188.0,  // Inner tire temp
  "unit": "F"      // "F" or "C"
}
```

**STATUS Packet (JSON)**:
```json
{
  "batteryLow": false,
  "sensorError": false,
  "probeConnected": true
}
```

**CORNER_ID Format (UInt8, 1 byte)**:
- 0 = LF (Left Front)
- 1 = RF (Right Front)
- 2 = LR (Left Rear)
- 3 = RR (Right Rear)

Stored in NVS, used for dynamic device naming (TireProbe_LF, TireProbe_RF, etc.)

## Configuration (include/config.h)

```cpp
// Stability detection
#define TEMP_STABLE_THRESHOLD     0.5f    // °C variance allowed
#define STABILITY_DURATION_MS     1000    // Must be stable for 1 second
#define TEMP_READ_INTERVAL_MS     100     // Read probes every 100ms

// Capture feedback
#define CAPTURE_DISPLAY_MS        1500    // Show capture confirmation
#define CAPTURE_LED_MS            1000    // Green LED duration

// Temperature limits
#define MAX_TEMP_C                400.0f  // Validity check
#define MIN_TEMP_C                -10.0f  // Validity check

// Battery
#define BATTERY_READ_INTERVAL_MS  5000
#define BATTERY_LOW_THRESHOLD     10      // Percent

// BLE
#define BLE_DEVICE_NAME           "TireProbe"
#define STATUS_TX_INTERVAL_MS     2000    // System status broadcast
```

## Dependencies (platformio.ini)

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

lib_deps =
    h2zero/NimBLE-Arduino@^1.4.1
    adafruit/Adafruit MAX31855 library@^1.4.1
    fastled/FastLED@^3.6.0
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/Adafruit GFX Library@^1.11.5
    bblanchon/ArduinoJson@^6.21.0
```

## Build Commands

```bash
cd tire-temp-probe

# Build
pio run

# Upload to ESP32-S3
pio run --target upload

# Upload and monitor
pio run --target upload && pio device monitor

# Clean
pio run --target clean
```

## Main Loop Logic (main.cpp)

```cpp
void loop() {
  probesUpdate();
  powerUpdate();
  
  switch (currentState) {
    case STATE_WAITING_CONNECTION:
      displayShowWaitingConnection();
      ledBreathing(BLUE);
      if (bleIsConnected()) {
        transitionTo(STATE_CORNER_RF);
      }
      break;
      
    case STATE_CORNER_RF:
    case STATE_CORNER_LF:
    case STATE_CORNER_LR:
    case STATE_CORNER_RR:
      displayShowCornerPrompt(getCurrentCorner());
      ledOff();
      if (probesDetectContact()) {
        transitionTo(getStabilizingState());
      }
      break;
      
    case STATE_STABILIZING_RF:
    case STATE_STABILIZING_LF:
    case STATE_STABILIZING_LR:
    case STATE_STABILIZING_RR:
      displayShowStabilizing(probesGetStabilityProgress());
      ledPulse(YELLOW);
      if (probesAreStable()) {
        captureAndTransmit();
        transitionTo(getCapturedState());
      }
      break;
      
    case STATE_CAPTURED_RF:
    case STATE_CAPTURED_LF:
    case STATE_CAPTURED_LR:
    case STATE_CAPTURED_RR:
      displayShowCaptured(getCurrentCorner(), lastReading);
      ledSolid(GREEN);
      if (millis() - captureTime > CAPTURE_DISPLAY_MS) {
        transitionToNextCorner();
      }
      break;
      
    case STATE_SESSION_COMPLETE:
      displayShowComplete();
      ledOff();
      // Wait for disconnect or timeout to reset
      break;
  }
  
  // Periodic status broadcast
  if (bleIsConnected() && statusTimer.elapsed()) {
    bleTransmitSystemStatus();
  }
  
  delay(10); // Watchdog
}
```

## Mobile App Integration

The mobile app (`apps/mobile-tire-probe/`) expects:

1. **Single BLE connection** to "TireProbe" device
2. **Subscribe to CORNER_READING** characteristic
3. **Receive 4 JSON packets** (one per corner) during session
4. **Session auto-completes** when 4 corners received

**App does NOT**:
- Control which corner is being captured
- Tell device when to capture
- Need to send any commands to device

Device is fully autonomous once connected.

## Serial Debug Output

```
[INIT] Tire Probe v1.0
[INIT] Probes: OK
[INIT] Display: OK
[INIT] BLE: Advertising as "TireProbe"
[BLE] Client connected
[STATE] → CORNER_RF
[PROBE] Contact detected, stabilizing...
[PROBE] Stability: 45%
[PROBE] Stability: 78%
[PROBE] Stability: 100% - CAPTURED
[BLE] TX Corner: RF | In:85.2 Mid:88.1 Out:82.5 Brake:176.3
[STATE] → CORNER_LF
...
[STATE] → SESSION_COMPLETE
[BLE] Client disconnected
[STATE] → WAITING_CONNECTION
```

## Common Modifications

### Change Corner Sequence
Edit `getNextCorner()` in `main.cpp`. Default (v2): LF → RF → LR → RR

### Adjust Stability Threshold
Edit `TEMP_STABLE_THRESHOLD` in `config.h` (default 0.5°C)

### Adjust Stability Duration
Edit `STABILITY_DURATION_MS` in `config.h` (default 1000ms)

### Add Audio Feedback
Uncomment buzzer code in `main.cpp`, wire buzzer to GPIO47

### Change Display Content
Edit functions in `display.cpp`

## Troubleshooting

**Temps never stabilize**:
- Increase `TEMP_STABLE_THRESHOLD` (try 1.0°C)
- Decrease `STABILITY_DURATION_MS` (try 500ms)
- Check for drafts or ambient temp changes
- Verify good probe contact

**Display not working**:
- Check I2C wiring (SDA to GPIO8, SCL to GPIO9)
- Verify 3.3V power to display
- Check I2C address (usually 0x3C or 0x3D)

**BLE not connecting**:
- Check serial output for BLE init
- Verify app is scanning for "TireProbe"
- Try power cycling device

**Capture not triggering**:
- Watch serial for stability percentage
- Verify all 4 probes are making contact
- Check for sensor errors in serial output

## Safety Considerations

- Maximum temperature limit: 400°C
- Low battery warning at 10%
- Sensor error detection with visual feedback
- Device will not capture if any probe is invalid
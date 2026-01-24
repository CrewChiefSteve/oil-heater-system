# CLAUDE.md - Tire Temperature Probe

This file provides guidance to Claude Code when working with the tire-temp-probe project.

## Project Overview

Wireless thermocouple-based tire and brake temperature measurement system for motorsports applications. Uses 4 MAX31855K thermocouple amplifiers to measure tire temperatures (inside/middle/outside) and brake rotor temperature, transmitting data via BLE to a receiver/display unit.

**Important**: This is the thermocouple probe system in `tire-temp-probe/`, NOT the IR gun in `tire-temp-gun/`.

## Hardware Architecture

### Core Components
- **MCU**: ESP32-S3-WROOM-1
- **Thermocouples**: 4x MAX31855K amplifiers on shared SPI bus
  - 3x tire probes (inside, middle, outside of tire contact patch)
  - 1x brake rotor probe
- **Status LED**: WS2812B RGB LED (single pixel)
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

**Peripherals**:
- GPIO48 = RGB_LED (WS2812B)
- GPIO1 = VBAT_ADC (battery voltage monitoring)
- GPIO2 = CHRG_STAT (TP4056 charge status)
- GPIO47 = BUZZER (optional audio alerts)

## Software Architecture

### Modular Design

The project is organized into separate functional modules:

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
│   ├── led.h               # Status LED interface
│   └── power.h             # Battery management interface
└── src/
    ├── main.cpp            # Setup, loop, state machine
    ├── probes.cpp          # Thermocouple reading implementation
    ├── ble_service.cpp     # NimBLE service implementation
    ├── led.cpp             # FastLED status indication
    └── power.cpp           # Battery monitoring implementation
```

### Module Responsibilities

#### probes.cpp / probes.h
- Initialize 4x MAX31855K thermocouple amplifiers
- Read temperatures from all probes
- Validate temperature readings (range check, NaN detection)
- Smooth temperatures using moving average
- Detect temperature stability
- Calculate tire average from 3 probes
- Update ProbeData, TireChannel, BrakeChannel structures

**Key Functions**:
- `probesInit()` - Initialize thermocouples
- `probesUpdate(MeasurementData&)` - Read all probes
- `readProbe(csPin, ProbeData&)` - Read single probe
- `calculateTireAverage(TireChannel&)` - Average 3 tire probes

#### ble_service.cpp / ble_service.h
- Initialize NimBLE stack
- Create BLE service with 4 characteristics
- Handle client connections/disconnections
- Transmit tire, brake, and system status packets
- Read/write corner assignment configuration
- Manage BLE advertising

**Key Functions**:
- `bleInit(deviceName)` - Initialize BLE stack and service
- `bleStartAdvertising()` - Start advertising
- `bleTransmitTireData()` - Send tire temps packet
- `bleTransmitBrakeData()` - Send brake temp packet
- `bleTransmitSystemStatus()` - Send battery/state packet
- `bleGetCorner()` - Get corner assignment (FL/FR/RL/RR)

#### led.cpp / led.h
- Initialize WS2812B RGB LED
- Update LED color/pattern based on device state
- Non-blocking blink patterns
- Visual feedback for connection status

**Key Functions**:
- `ledInit()` - Initialize FastLED
- `ledUpdate(state, connected)` - Update based on state
- `ledSetColor(r, g, b)` - Set solid color
- `ledBlink(r, g, b, interval)` - Blink pattern

**LED States**:
- Yellow fast blink: Initializing
- Green slow blink: Idle, not connected
- Blue solid: Connected, idle
- Green solid: Connected, measuring
- Cyan very fast: Transmitting
- Orange blink: Low battery
- Red fast blink: Error

#### power.cpp / power.h
- Read battery voltage via ADC
- Calculate battery percentage
- Detect charging status (TP4056 CHRG pin)
- Update SystemStatus structure
- Trigger low battery state

**Key Functions**:
- `powerInit()` - Initialize ADC and GPIO
- `powerUpdate(SystemStatus&)` - Update battery status
- `powerReadVoltage()` - Read battery voltage
- `powerCalculatePercent(voltage)` - Convert to percentage
- `powerIsCharging()` - Check charge status

#### main.cpp
- Initialize all subsystems
- State machine implementation
- Periodic temperature reading (250ms)
- BLE transmission (500ms when connected)
- Serial output for debugging
- Loop coordination

### Data Structures (types.h)

**DeviceState** enum:
- STATE_INITIALIZING - Startup
- STATE_IDLE - No active measurement
- STATE_MEASURING - Actively reading
- STATE_TRANSMITTING - Sending BLE data
- STATE_LOW_BATTERY - Battery <10%
- STATE_ERROR - Sensor fault

**Corner** enum:
- CORNER_FL (Front Left)
- CORNER_FR (Front Right)
- CORNER_RL (Rear Left)
- CORNER_RR (Rear Right)

**ProbeData** struct:
- temperature (float, Celsius)
- avgTemperature (smoothed average)
- isValid (bool)
- isStable (bool)
- errorCount (uint8_t)
- lastReadTime (uint32_t)

**TireChannel** struct:
- inside (ProbeData)
- middle (ProbeData)
- outside (ProbeData)
- averageTemp (calculated from 3 probes)

**BrakeChannel** struct:
- rotor (ProbeData)

**MeasurementData** struct:
- tire (TireChannel)
- brake (BrakeChannel)
- corner (Corner assignment)
- timestamp (uint32_t)

**SystemStatus** struct:
- state (DeviceState)
- batteryPercent (uint8_t)
- batteryVoltage (float)
- charging (bool)
- uptimeMs (uint32_t)

### BLE Protocol (ble_protocol.h)

**Service UUID**: `4fafc201-0004-459e-8fcc-c5c9c331914b`

**Characteristics**:
- `beb5483e-36e1-4688-b7f5-ea07361b26a8`: Tire temps (notify) - 18 bytes
- `beb5483e-36e1-4688-b7f5-ea07361b26a9`: Brake temp (notify) - 13 bytes
- `beb5483e-36e1-4688-b7f5-ea07361b26aa`: System status (notify) - 11 bytes
- `beb5483e-36e1-4688-b7f5-ea07361b26ab`: Device config (read/write) - 2 bytes

**Packet Formats**:

*Tire Data (18 bytes)*:
```
[Type:1] [Inside:4f] [Middle:4f] [Outside:4f] [Timestamp:4u] [Corner:1]
```

*Brake Data (13 bytes)*:
```
[Type:1] [Rotor:4f] [Timestamp:4u] [Corner:1]
```

*System Status (11 bytes)*:
```
[Type:1] [State:1] [BatPct:1] [Voltage:4f] [Charging:1] [Uptime:4u]
```

All temperatures are float32 in Celsius. Timestamps are uint32_t milliseconds since boot.

## Configuration (include/config.h)

**Key Parameters**:
- `TEMP_READ_INTERVAL_MS` = 250ms (probe read rate)
- `BLE_TX_INTERVAL_MS` = 500ms (transmission rate)
- `TEMP_STABLE_THRESHOLD` = 0.5°C (stability detection)
- `TEMP_SMOOTHING_SAMPLES` = 8 (moving average window)
- `BATTERY_READ_INTERVAL_MS` = 5000ms
- `BATTERY_LOW_THRESHOLD` = 10% (warning level)
- `MAX_TEMP_C` = 400°C (validity check)
- `MIN_TEMP_C` = -10°C (validity check)

## Dependencies (platformio.ini)

- **h2zero/NimBLE-Arduino** ^1.4.1 - BLE stack
- **adafruit/Adafruit MAX31855 library** ^1.4.1 - Thermocouple interface
- **fastled/FastLED** ^3.6.0 - RGB LED control

## Build Commands

```bash
cd tire-temp-probe

# Build the project
pio run

# Upload to ESP32-S3
pio run --target upload

# Upload and monitor serial (115200 baud)
pio run --target upload && pio device monitor

# Clean build
pio run --target clean
```

## State Machine

```
INITIALIZING → IDLE
     ↓
  MEASURING ←→ ERROR (sensor faults)
     ↓
TRANSMITTING (when BLE connected)
     ↓
LOW_BATTERY (battery <10%, not charging)
```

**State Transitions**:
- INITIALIZING → IDLE: After successful init
- IDLE → MEASURING: Valid probe readings
- MEASURING → ERROR: Any probe returns invalid data
- ERROR → MEASURING: All probes return valid data
- MEASURING → TRANSMITTING: BLE connected, TX interval elapsed
- TRANSMITTING → MEASURING: Transmission complete
- Any state → LOW_BATTERY: Battery <10% and not charging

## Serial Output

Outputs temperature data at 250ms intervals:
```
Tire [IN/MID/OUT]: 85.2 / 87.5 / 84.8 C  |  Brake: 215.3 C  |  Bat: 78% (CHG)
```

## Common Modifications

### Adjust Read Rate
Edit `TEMP_READ_INTERVAL_MS` in `include/config.h` (default 250ms)

### Change BLE Transmission Rate
Edit `BLE_TX_INTERVAL_MS` in `include/config.h` (default 500ms)

### Modify Temperature Smoothing
Edit `TEMP_SMOOTHING_SAMPLES` in `include/config.h` (default 8 samples)

### Change Battery Thresholds
Edit `BATTERY_MIN_VOLTAGE`, `BATTERY_MAX_VOLTAGE`, `BATTERY_LOW_THRESHOLD` in `include/config.h`

### Add New BLE Characteristic
1. Add UUID to `include/ble_protocol.h`
2. Create characteristic in `bleInit()` in `src/ble_service.cpp`
3. Add transmission function in `src/ble_service.cpp`
4. Call from `loop()` in `src/main.cpp`

### Change Pin Assignments
Edit `include/pins.h` (requires hardware rework)

## Troubleshooting

**"Sensor error" messages**:
- Check MAX31855K wiring (SCK, CS, MISO connections)
- Verify thermocouple polarity (+ and -)
- Check for shorts or open circuits in thermocouples
- Ensure CS pins are not shared with other peripherals

**BLE not advertising**:
- Check serial output for BLE init messages
- Verify NimBLE library version (^1.4.1)
- ESP32-S3 should support BLE natively

**LED not working**:
- Verify WS2812B data pin (GPIO48)
- Check LED power supply (needs 5V)
- Verify FastLED library installed

**Battery percentage incorrect**:
- Calibrate `BATTERY_VOLTAGE_DIVIDER` in `include/config.h`
- Check ADC pin connection (GPIO1)
- Verify voltage divider resistor values if used

**Temperature readings unstable**:
- Increase `TEMP_SMOOTHING_SAMPLES` (default 8)
- Adjust `TEMP_STABLE_THRESHOLD` (default 0.5°C)
- Check for electromagnetic interference near SPI bus

## Development Notes

1. **Modular Architecture**: Each subsystem (probes, BLE, LED, power) is isolated in its own module for maintainability.

2. **Non-Blocking Design**: All timing uses `millis()` checks, no `delay()` calls in main loop (except 10ms watchdog protection).

3. **Error Handling**: Each probe tracks its own error count, system transitions to ERROR state only if probes fail repeatedly.

4. **BLE Packet Format**: Simple binary packets, no framing or checksums (BLE handles integrity).

5. **Corner Assignment**: Can be configured via BLE write to config characteristic, allowing receiver to assign probe to specific wheel.

6. **Battery Management**: Uses voltage-to-percentage lookup with configurable thresholds. TP4056 CHRG pin indicates charging status.

7. **LED Feedback**: Different blink patterns for each state, color-coded for quick visual status check.

## Safety Considerations

- Maximum temperature limit: 400°C (config.h)
- Low battery warning at 10%
- Sensor error detection with retry count
- Watchdog delay in main loop prevents crashes

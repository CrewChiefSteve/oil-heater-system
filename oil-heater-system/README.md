# Smart Oil Heater Controller

ESP32-based thermostat controller for race car oil heating applications.

**Note**: This repository contains the **controller firmware only**. The display interface is maintained in a separate repository: [oil-heater-display](https://github.com/CrewChiefSteve/oil-heater-display) (Arduino-based).

## Architecture

```
┌─────────────────────────┐    UART Serial (115200)  ┌─────────────────────────┐
│    Display Board        │◄────────────────────────►│   Controller Board      │
│  (Arduino-based)        │      GPIO 16/17 TX/RX    │   ESP32 DevKit v1       │
│  Separate Repository    │                          │                         │
│                         │                          │   - MAX6675 Thermocouple│
│  - Touchscreen Display  │                          │   - Relay Module        │
│  - Setpoint Adjustment  │                          │   - Safety Watchdog     │
│  - Status Visualization │                          │   - Bang-bang Control   │
└─────────────────────────┘                          │   - Temp Smoothing      │
                                                     └─────────────────────────┘
```

## Features

- **Temperature Smoothing**: 5-sample moving average filter reduces noise from MAX6675 readings
- **Smart State Change Detection**: Display updates only when temperature, relay state, fault code, setpoint, or enable status changes
- **Safety Watchdog**: Heater turns OFF if no display command received for 5 seconds
- **Bang-bang Control**: Simple hysteresis-based thermostat (±2°C deadband)
- **Over-temperature Protection**: Hard shutdown at 160°C (320°F)
- **Sensor Fault Detection**: Automatic heater disable on thermocouple disconnect
- **UART Communication**: Reliable serial protocol with magic number validation

## Hardware Setup

### Controller Board (ESP32 DevKit v1)

| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| MAX6675 SCK | GPIO 18 | Clock |
| MAX6675 CS | GPIO 5 | Chip Select |
| MAX6675 SO | GPIO 19 | Data Out (MISO) |
| Relay IN | GPIO 23 | Active HIGH (configurable via RELAY_ACTIVE_HIGH) |
| Display UART RX | GPIO 16 | Serial2 RX (from display TX) |
| Display UART TX | GPIO 17 | Serial2 TX (to display RX) |
| MAX6675 VCC | 3.3V | |
| MAX6675 GND | GND | |

**Display Connection**: Connect to the display board via UART (115200 baud). See the [oil-heater-display](https://github.com/CrewChiefSteve/oil-heater-display) repository for display wiring and firmware.

## Build and Upload

### Using PlatformIO CLI

```bash
cd controller
pio run -t upload -t monitor
```

Or if PlatformIO is not in your PATH:

```bash
cd controller
python -m platformio run -t upload -t monitor
```

### Expected Serial Output

After successful upload, you should see:
```
========================================
  Smart Oil Heater - Controller Board
========================================

[OK] Relay initialized (OFF)
[OK] Display UART initialized
     RX: GPIO 16, TX: GPIO 17, Baud: 115200

Waiting for display connection...
```

Once the display is connected via UART, you'll see:
```
[OK] Display connected via UART!
T=25.0C  Set=110.0C  En=0  Relay=OFF  Fault=0
```

## Operation

1. **Connect controller and display via UART** (TX↔RX, RX↔TX, GND↔GND)
2. **Power both boards**
3. Controller waits for display connection (sends status at 250ms intervals)
4. **Display shows status** once UART communication established
5. **Use display controls** to adjust setpoint (50-150°C range)
6. **Heater cycles on/off** around setpoint with 2°C hysteresis

## Safety Features

- **Watchdog**: Heater turns OFF if no UI command for 5 seconds
- **Overtemp Cutoff**: Hard shutdown at 160°C (320°F)
- **Sensor Fault**: Heater disabled if thermocouple disconnected
- **Status Display**: Real-time fault indication on screen

## CRITICAL: Hardware Thermal Cutoff

**Do NOT rely solely on software for safety!**

Wire a snap-disc thermostat (normally closed) in series with your relay. Set it 10-15°C above your maximum operating temperature. This provides hardware-level protection if the ESP32 crashes with the relay ON.

## BLE Protocol (v2)

The controller board broadcasts status data via BLE for mobile app integration.

**Device Name**: `Heater_XXXX` (where XXXX = last 4 hex digits of WiFi MAC address)

**Service UUID**: `4fafc201-0001-459e-8fcc-c5c9c331914b`

**Characteristics**:

| Characteristic | UUID | Properties | Data Format | Description |
|----------------|------|------------|-------------|-------------|
| TEMPERATURE | `beb5483e-36e1-4688-b7f5-ea07361b26a8` | READ, NOTIFY | Float32LE (4 bytes) | Current temperature in Celsius |
| TARGET | `beb5483e-36e1-4688-b7f5-ea07361b26a9` | READ, WRITE, NOTIFY | Float32LE (4 bytes) | Target setpoint in Celsius |
| STATUS | `beb5483e-36e1-4688-b7f5-ea07361b26aa` | READ, NOTIFY | JSON string | System status (see below) |

**STATUS Characteristic JSON Format**:
```json
{
  "heater": true,
  "fault": 0,
  "overheat": false
}
```

Fields:
- `heater` (boolean): Relay state (true = ON, false = OFF)
- `fault` (number): Fault code (0 = no fault, 1 = sensor error, 2 = over-temperature)
- `overheat` (boolean): Over-temperature shutdown active

**Notes**:
- All NOTIFY characteristics include BLE2902 descriptors for iOS compatibility
- Temperature values are IEEE 754 single-precision floats in little-endian byte order
- Status updates broadcast at ~250ms intervals over ESP-NOW between boards

## Repository Structure

```
oil-heater-system/
├── controller/
│   ├── platformio.ini          # ESP32 build configuration
│   └── src/
│       └── main.cpp            # Controller firmware (~380 lines)
└── README.md
```

**Display Firmware**: See [oil-heater-display](https://github.com/CrewChiefSteve/oil-heater-display) repository.

## Configuration

Key parameters in `controller/src/main.cpp`:

```cpp
// Control parameters
static constexpr float DEFAULT_SETPOINT_C = 110.0f;   // Default target temp
static constexpr float HYSTERESIS_C       = 2.0f;     // ±2°C deadband
static constexpr float MAX_SAFE_TEMP_C    = 160.0f;   // Emergency shutoff
static constexpr float MIN_SETPOINT_C     = 50.0f;    // Minimum allowed setpoint
static constexpr float MAX_SETPOINT_C     = 150.0f;   // Maximum allowed setpoint

// Timing
static constexpr uint32_t CMD_TIMEOUT_MS  = 5000;     // Watchdog timeout
static constexpr uint32_t TEMP_READ_MS    = 250;      // Temperature read interval
static constexpr uint32_t STATUS_SEND_MS  = 250;      // Status broadcast interval

// Temperature smoothing
static constexpr int NUM_SAMPLES          = 15;       // Moving average window

// Hardware
static constexpr bool RELAY_ACTIVE_HIGH   = true;     // Set false for active-LOW relay
```

## Thermocouple Calibration

The controller supports three calibration modes for improved temperature accuracy.

### Mode 1: No Calibration (CAL_NONE) - Default

Uses raw MAX6675 readings. Typical accuracy: ±3-5°C.

```cpp
static constexpr CalibrationMode CAL_MODE = CAL_NONE;
```

**When to use**: For initial testing or when precise temperature control isn't critical.

### Mode 2: Single-Point Calibration (CAL_SINGLE)

**Best for**: Quick calibration using a reference thermometer at operating temperature.

**Procedure:**
1. Heat oil to operating temperature (~200°F / 93°C)
2. Compare MAX6675 reading to a trusted reference thermometer
3. Calculate offset: `reference_reading - MAX6675_reading`
4. Update code in `controller/src/main.cpp`:

```cpp
static constexpr CalibrationMode CAL_MODE = CAL_SINGLE;
static constexpr float CAL_SINGLE_OFFSET_C = -1.7f;  // Your calculated offset
```

**Example:**
- Reference thermometer: 93.3°C (200°F)
- MAX6675 serial output: 95.0°C
- Offset calculation: 93.3 - 95.0 = **-1.7°C**

**Advantages**: Quick, simple, one measurement needed
**Limitations**: Only corrects offset, assumes sensor scale is accurate

### Mode 3: Two-Point Calibration (CAL_TWO_POINT)

**Best for**: Maximum accuracy across full temperature range (0-150°C).

**Procedure:**

**Step 1: Ice Bath Test (0°C / 32°F)**
1. Fill container with crushed ice and water (slush consistency)
2. Stir well and let stabilize for 2 minutes
3. Submerge thermocouple tip (don't touch container sides)
4. Wait 2 minutes for stabilization
5. Record the raw reading from serial monitor
6. Typical reading: 0-2°C

**Step 2: Boiling Water Test (100°C / 212°F)**
1. Bring water to rolling boil
2. Submerge thermocouple (don't touch pot bottom/sides)
3. Wait 1 minute for stabilization
4. Record the raw reading
5. **Altitude adjustment**: Mooresville, NC (~700ft) ≈ 99°C
   - Use [elevation boiling point calculator](https://www.omnicalculator.com/chemistry/boiling-point-altitude) if needed

**Step 3: Update Configuration**

Edit `controller/src/main.cpp`:

```cpp
static constexpr CalibrationMode CAL_MODE = CAL_TWO_POINT;
static constexpr float CAL_RAW_ICE_C  = 1.2f;   // Your ice bath reading
static constexpr float CAL_RAW_BOIL_C = 98.5f;  // Your boiling water reading
static constexpr float CAL_REF_BOIL_C = 99.0f;  // Adjust for altitude (100°C at sea level)
```

**Advantages**: Corrects both offset and scale errors, accurate across full range
**Limitations**: Requires more time and equipment (ice bath + boiling water)

### Calibration Verification

After configuration, upload firmware and check serial output at startup:

```
========================================
  Smart Oil Heater - Controller Board
========================================

[OK] Relay initialized (OFF)
[OK] Display UART initialized
     RX: GPIO 16, TX: GPIO 17, Baud: 115200

[CAL] Thermocouple calibration:
      Mode: TWO-POINT
      Ice reading:  1.20 C (ref: 0.00 C)
      Boil reading: 98.50 C (ref: 99.00 C)
      Calculated scale:  1.0154
      Calculated offset: -1.22 C
      Status: CALIBRATED
```

### Calibration Debug Mode

To see raw vs calibrated temperatures in real-time, edit `controller/src/main.cpp`:

```cpp
#define CAL_DEBUG_RAW true
```

Serial output will show:
```
[CAL] Raw: 95.00 C -> Calibrated: 93.30 C
T=93.3C  Set=110.0C  En=1  Relay=ON  Fault=0
```

**Remember to set back to `false` after calibration testing.**

### Tips for Accurate Calibration

- **Single-point**: Calibrate at your typical operating temperature for best accuracy
- **Two-point**: Use distilled water for boiling test (tap water minerals affect boiling point)
- **Thermocouple placement**: Keep tip in center of liquid, away from container walls
- **Wait for stability**: Allow 1-2 minutes for readings to stabilize
- **Multiple readings**: Take 3-5 readings and average them
- **Document results**: Note your calibration values in case firmware needs to be reflashed

## Troubleshooting

### Controller Issues

**Sensor reads 0°C or NAN**
- Check MAX6675 wiring (SCK, CS, SO pins)
- Verify thermocouple is properly connected to MAX6675
- MAX6675 requires 3.3V power
- Check serial output for sensor error messages

**Relay not switching**
- Verify relay module wiring to GPIO 23
- Check if relay requires active LOW (set `RELAY_ACTIVE_HIGH = false` in code)
- Test relay with manual GPIO control to verify hardware
- Some relay modules need separate 5V power supply

**Relay clicking rapidly**
- Increase `HYSTERESIS_C` value in code (default is 2°C)
- Check thermocouple placement for stable readings
- Temperature smoothing (5-sample moving average) should already help reduce noise

**"FAULT_COMM_TIMEOUT" displayed**
- Display not sending commands within 5 seconds
- Check UART wiring (TX↔RX, RX↔TX, GND↔GND)
- Verify both boards at 115200 baud
- Monitor serial output on both boards for communication packets

**Display shows no updates**
- Verify UART TX pin (GPIO 17) is connected to display RX
- Check baud rate matches on both sides (115200)
- Look for "[OK] Display connected via UART!" message in serial output

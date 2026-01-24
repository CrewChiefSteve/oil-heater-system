# Laser Ride Height Sensor

ESP32-C3 based dual Time-of-Flight (ToF) sensor system for race car ride height measurement with BLE wireless interface.

## Hardware Overview

- **MCU**: ESP32-C3 Mini (esp32-c3-devkitm-1)
- **Sensors**: 2x VL53L1X Time-of-Flight sensors (4m range, ±3mm accuracy)
- **Interface**: BLE 5.0 for wireless data transmission
- **Input**: Physical button for manual readings
- **Output**: Status LED
- **Power**: Battery monitoring via ADC

## Features

- **Dual Sensor Configuration**: Two independent ToF sensors on shared I2C bus
- **Automatic Address Assignment**: Uses XSHUT sequencing to assign unique I2C addresses
- **Outlier Rejection**: Automatically detects and filters measurement discrepancies
- **Continuous Mode**: Stream readings at 10Hz via BLE
- **Zero Calibration**: Store offset for relative measurements
- **Low Power BLE**: NimBLE stack for efficient wireless communication
- **Battery Monitoring**: Real-time voltage reporting

## Wiring Diagram

```
ESP32-C3 Mini Pinout:
┌─────────────────────────────────────┐
│  ESP32-C3-DevKitM-1                 │
├─────────────────────────────────────┤
│                                     │
│  [GPIO 4] ────────┬─────────────────┼──> I2C SDA (both sensors)
│                   │                 │
│  [GPIO 5] ────────┼────┬────────────┼──> I2C SCL (both sensors)
│                   │    │            │
│  [GPIO 6] ────────┼────┼────────────┼──> Sensor 1 XSHUT
│                   │    │            │
│  [GPIO 7] ────────┼────┼────┬───────┼──> Sensor 2 XSHUT
│                   │    │    │       │
│  [GPIO 8] ────────┼────┼────┼───────┼──> Status LED (+ 220Ω to GND)
│                   │    │    │       │
│  [GPIO 9] ────────┼────┼────┼───────┼──> Button (to GND, INPUT_PULLUP)
│                   │    │    │       │
│  [GPIO 2] ────────┼────┼────┼───────┼──> Battery ADC (voltage divider)
│                   │    │    │       │
│  [GND]    ────────┼────┼────┼───────┼──> Common Ground
│                   │    │    │       │
│  [3V3]    ────────┼────┼────┼───────┼──> 3.3V Power Rail
│                   │    │    │       │
└───────────────────┼────┼────┼───────┘
                    │    │    │
        ┌───────────┘    │    └──────────┐
        │                │               │
        ▼                ▼               ▼
   ┌─────────┐      ┌─────────┐    ┌─────────┐
   │ VL53L1X │      │ VL53L1X │    │ Status  │
   │ Sensor1 │      │ Sensor2 │    │   LED   │
   ├─────────┤      ├─────────┤    └─────────┘
   │ VIN  ◄──┼──3V3 │ VIN  ◄──┼──3V3
   │ GND  ◄──┼──GND │ GND  ◄──┼──GND
   │ SDA  ◄──┼──┬───│ SDA  ◄──┼──── GPIO 4 (I2C SDA)
   │ SCL  ◄──┼──┼───│ SCL  ◄──┼──── GPIO 5 (I2C SCL)
   │ XSHUT◄──┼──┼───│ XSHUT◄──┼──┬─ GPIO 6 / GPIO 7
   └─────────┘  │   └─────────┘  │
                │                 │
              4.7kΩ             4.7kΩ
               Pull-up          Pull-up
                │                 │
               3V3               3V3
```

### Battery Voltage Divider (if monitoring > 3.3V)

```
Battery (+) ─────┬───── ADC Input (GPIO 2)
                 │
               10kΩ
                 │
                GND
               10kΩ
                 │
              ESP GND

This creates a 2:1 voltage divider. Adjust VOLTAGE_DIVIDER_RATIO in config.h.
For 5V max battery: 5V → 2.5V at ADC (safe for ESP32-C3)
```

## Pin Configuration

| Function        | GPIO | Description                          |
|-----------------|------|--------------------------------------|
| I2C SDA         | 4    | Shared data line for both sensors    |
| I2C SCL         | 5    | Shared clock line for both sensors   |
| Sensor1 XSHUT   | 6    | Sensor 1 shutdown/reset (address 0x30)|
| Sensor2 XSHUT   | 7    | Sensor 2 shutdown/reset (address 0x31)|
| Status LED      | 8    | Active HIGH, 220Ω current limiting   |
| Button          | 9    | Active LOW with INPUT_PULLUP         |
| Battery ADC     | 2    | Voltage divider input (0-3.3V max)   |

**Important**: Both sensors share the same I2C bus (SDA/SCL). Address assignment is handled via XSHUT sequencing during initialization.

## I2C Pull-up Resistors

VL53L1X modules typically include on-board pull-ups (10kΩ). For long wire runs or multiple devices, add external 4.7kΩ pull-ups to 3.3V on SDA and SCL lines.

## BLE Protocol

### Device Information

- **Device Name**: `RH-Sensor`
- **Service UUID**: `4fafc201-0003-459e-8fcc-c5c9c331914b`

### Characteristics

#### Height Data (READ/NOTIFY)
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Properties**: READ, NOTIFY
- **Format**: ASCII string
- **Example**: `"S1:123.4,S2:125.1,AVG:124.2,BAT:4.89"`

**Data Format Breakdown**:
- `S1`: Sensor 1 distance (mm)
- `S2`: Sensor 2 distance (mm)
- `AVG`: Averaged/filtered distance (mm, after outlier rejection and zero calibration)
- `BAT`: Battery voltage (V)

#### Command (WRITE)
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a9`
- **Properties**: WRITE
- **Format**: Single ASCII character

**Supported Commands**:
| Command | Character | Description                              |
|---------|-----------|------------------------------------------|
| Single  | `R`       | Take single reading and transmit         |
| Start   | `C`       | Start continuous mode (10Hz updates)     |
| Stop    | `S`       | Stop continuous mode                     |
| Zero    | `Z`       | Zero calibration (store current reading as offset) |

### BLE Connection Flow

1. **Discovery**: Scan for device named `RH-Sensor`
2. **Connect**: Establish BLE connection
3. **Subscribe**: Enable notifications on Height characteristic
4. **Command**: Send command character to Command characteristic
5. **Receive**: Data arrives via notification or read request

### Example BLE Commands (Python with bleak)

```python
import asyncio
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "4fafc201-0003-459e-8fcc-c5c9c331914b"
CHAR_HEIGHT = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHAR_COMMAND = "beb5483e-36e1-4688-b7f5-ea07361b26a9"

async def main():
    # Find device
    device = await BleakScanner.find_device_by_name("RH-Sensor")

    async with BleakClient(device) as client:
        # Subscribe to notifications
        def callback(sender, data):
            print(f"Data: {data.decode()}")

        await client.start_notify(CHAR_HEIGHT, callback)

        # Send command to start continuous mode
        await client.write_gatt_char(CHAR_COMMAND, b'C')

        # Wait for data...
        await asyncio.sleep(10)

        # Stop continuous mode
        await client.write_gatt_char(CHAR_COMMAND, b'S')

asyncio.run(main())
```

## Sensor Configuration

### VL53L1X Settings

- **Distance Mode**: Long (4m range, configurable in `config.h`)
- **Timing Budget**: 33ms per sensor (~30Hz max)
- **I2C Speed**: 400kHz (Fast Mode)
- **Measurement Mode**: Continuous ranging

### Outlier Rejection Algorithm

When both sensors are operational:
1. Calculate absolute difference: `delta = |S1 - S2|`
2. If `delta > 10mm`: Use the **lower** reading (closer object is more reliable)
3. If `delta ≤ 10mm`: Use **average** of both readings

This approach filters out spurious reflections and sensor noise.

### Zero Calibration

Zero calibration allows relative height measurements:
1. Position sensors at reference height (e.g., ride height at full droop)
2. Send `Z` command via BLE
3. System stores current reading as offset
4. All future readings are adjusted: `reported_distance = measured_distance - offset`

Maximum allowed offset: 500mm (configurable in `config.h`)

## Build and Upload

### Prerequisites

- [PlatformIO Core](https://platformio.org/install) or PlatformIO IDE extension
- ESP32-C3 board definition (auto-installed by PlatformIO)

### Build Commands

```bash
cd ride-height-sensor

# Build project
pio run

# Upload to ESP32-C3 (auto-detect port)
pio run --target upload

# Upload to specific port (Windows)
pio run --target upload --upload-port COM3

# Upload to specific port (Linux/Mac)
pio run --target upload --upload-port /dev/ttyUSB0

# Monitor serial output
pio device monitor

# Upload and monitor
pio run --target upload && pio device monitor

# Clean build
pio run --target clean
```

### First Boot

On first boot, the system will:
1. Initialize both VL53L1X sensors with address assignment
2. Start BLE advertising as `RH-Sensor`
3. Wait for button press or BLE command

**Serial output will show**:
```
================================================
  Laser Ride Height Sensor - ESP32-C3
  Dual VL53L1X ToF + BLE Interface
================================================

=== Initializing VL53L1X Sensors ===
Initializing Sensor 1...
Sensor 1 address set to 0x30
Initializing Sensor 2...
Sensor 2 address set to 0x31
Distance mode: LONG (4m range)
Timing budget: 33 ms (~30 Hz)
=== Sensor initialization complete ===

=== Initializing BLE ===
BLE Device: RH-Sensor
Service UUID: 4fafc201-0003-459e-8fcc-c5c9c331914b
BLE advertising started
=== BLE initialization complete ===

=== System Ready ===
```

## Usage

### Manual Reading (Button)

1. Press button on GPIO9
2. LED blinks briefly during measurement
3. Data transmitted via BLE (if connected) and printed to serial

### Continuous Mode (BLE)

1. Connect to `RH-Sensor` via BLE
2. Subscribe to Height characteristic notifications
3. Send `C` command to start continuous readings (10Hz)
4. Send `S` command to stop

### Zero Calibration

1. Position sensors at desired reference point
2. Send `Z` command via BLE
3. LED blinks 3 times to confirm calibration
4. All future readings are relative to this point

## LED Status Indicators

| Pattern                  | Meaning                                    |
|--------------------------|--------------------------------------------|
| Off                      | Idle, no BLE connection                    |
| Slow blink (1s period)   | BLE connected, idle                        |
| Quick blink (50ms)       | Taking measurement                         |
| 3x quick blinks          | Zero calibration successful                |
| Rapid blink (200ms)      | Sensor initialization error                |

## Configuration

All user-configurable parameters are in `include/config.h`:

### Pin Assignments
- Change GPIO pins for sensors, button, LED, ADC

### Sensor Parameters
- `TIMING_BUDGET_MS`: Measurement time per sensor (default: 33ms)
- `DISTANCE_MODE_LONG`: true = 4m range, false = 1.3m range
- `OUTLIER_THRESHOLD_MM`: Delta threshold for outlier rejection (default: 10mm)

### BLE Settings
- `BLE_DEVICE_NAME`: Change broadcast name
- `SERVICE_UUID` / `CHAR_*_UUID`: Modify UUIDs if needed

### Timing
- `CONTINUOUS_UPDATE_INTERVAL_MS`: Update rate in continuous mode (default: 100ms = 10Hz)
- `BUTTON_DEBOUNCE_MS`: Button debounce time (default: 50ms)

### Battery Monitoring
- `VOLTAGE_DIVIDER_RATIO`: Adjust for your voltage divider circuit (default: 2.0)

## Troubleshooting

### Sensors Not Initializing

**Symptom**: LED blinks rapidly, serial shows "Failed to initialize Sensor X"

**Solutions**:
1. Check I2C wiring (SDA=GPIO4, SCL=GPIO5)
2. Verify 3.3V power to both sensors
3. Ensure pull-up resistors on SDA/SCL (4.7kΩ to 3.3V)
4. Check XSHUT connections (GPIO6, GPIO7)
5. Try slower I2C speed: Change `Wire.setClock(400000)` to `Wire.setClock(100000)` in main.cpp

### BLE Not Advertising

**Symptom**: Cannot find `RH-Sensor` in BLE scan

**Solutions**:
1. Check serial output for "BLE advertising started" message
2. Ensure ESP32-C3 is powered properly (USB or external 5V)
3. Try resetting the device
4. Some phones/computers cache BLE devices - restart Bluetooth on client device

### Readings Show -1.0 or 999.9

**Symptom**: Sensor reads invalid distance

**Solutions**:
1. Check sensor is not in timeout (serial shows "Sensor X timeout")
2. Ensure target is within range (4m for long mode, 1.3m for short mode)
3. Verify target has sufficient reflectivity (avoid transparent/absorbing surfaces)
4. Check for ambient light interference (VL53L1X uses 940nm IR)

### Large Discrepancy Between Sensors

**Symptom**: S1 and S2 readings differ by >10mm consistently

**Solutions**:
1. Check sensor alignment - both should point at same surface
2. Verify both sensors are securely mounted
3. Increase `OUTLIER_THRESHOLD_MM` if genuine reading difference
4. Check for obstructions in sensor field of view (60° cone)

### Battery Voltage Reads 0.00V

**Symptom**: BAT field shows 0.00 or incorrect value

**Solutions**:
1. Check ADC wiring on GPIO2
2. Verify voltage divider circuit (must not exceed 3.3V at ADC pin!)
3. Adjust `VOLTAGE_DIVIDER_RATIO` in config.h to match your hardware
4. ESP32-C3 ADC reference is 3.3V - exceeding this will damage the chip

## Safety Notes

1. **ADC Input Voltage**: GPIO2 ADC input MUST NOT exceed 3.3V. Use proper voltage divider for batteries >3.3V.
2. **I2C Pull-ups**: Excessive pull-up current can damage ESP32-C3. Use 4.7kΩ or higher.
3. **Laser Safety**: VL53L1X emits Class 1 laser (940nm IR). Safe under normal use, but do not stare into beam with optical instruments.
4. **Mounting**: Ensure sensors are rigidly mounted to avoid vibration-induced measurement errors.

## Performance Specifications

- **Range**: 40mm - 4000mm (long mode), 40mm - 1300mm (short mode)
- **Accuracy**: ±3mm typical, ±5mm worst case
- **Update Rate**: 30Hz per sensor (33ms timing budget), 10Hz in continuous BLE mode
- **Field of View**: 27° (full width at half max)
- **Ambient Light Rejection**: Up to 40,000 lux
- **Power Consumption**:
  - Active measurement: ~20mA per sensor
  - BLE advertising: ~10mA
  - Idle (sensors stopped): <5mA

## License

This project is provided as-is for educational and racing applications. Modify freely for your use case.

## Credits

- **VL53L1X Library**: Pololu (https://github.com/pololu/vl53l1x-arduino)
- **NimBLE Stack**: h2zero (https://github.com/h2zero/NimBLE-Arduino)
- **ESP32-C3 Support**: Espressif Systems

## Support

For issues or questions:
1. Check serial output for diagnostic messages
2. Review wiring against diagram above
3. Verify configuration in `include/config.h`
4. Check GitHub issues for similar problems

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Laser ride height sensor for race car setup work. Uses dual VL53L1X Time-of-Flight sensors for redundant measurements with BLE connectivity to mobile app.

## Hardware

- **MCU:** ESP32-C3 Mini (esp32-c3-devkitm-1)
- **Sensors:** 2× VL53L1X ToF sensors (4m range, ±3mm accuracy)
- **Power:** 500mAh LiPo with TP4056 USB-C charger
- **UI:** Single LED status indicator, tactile button trigger

## Build and Upload Commands

This project uses PlatformIO. Use `pio` if PlatformIO Core CLI is in PATH, or `python -m platformio` otherwise.

```bash
cd ride-height-sensor

# Build the project
pio run

# Upload to ESP32-C3 (auto-detects port)
pio run --target upload

# Upload to specific port
pio run --target upload --upload-port COM3      # Windows
pio run --target upload --upload-port /dev/ttyUSB0  # Linux

# Upload and monitor serial output
pio run --target upload && pio device monitor

# Monitor serial output only (115200 baud)
pio device monitor

# Clean build files
pio run --target clean
```

## Pin Configuration

All pins defined in `include/config.h`:

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | 4 | Shared by both VL53L1X sensors |
| I2C SCL | 5 | 400kHz clock |
| XSHUT Sensor 1 | 6 | Address assignment control |
| XSHUT Sensor 2 | 7 | Address assignment control |
| LED | 8 | Status indicator |
| Button | 9 | INPUT_PULLUP, triggers reading |
| Battery ADC | 2 | Voltage divider input |

### I2C Address Assignment

Both VL53L1X sensors share the I2C bus. At boot, the firmware:
1. Holds both XSHUT pins LOW (sensors in reset)
2. Releases XSHUT1 HIGH, initializes sensor 1, sets address to 0x30
3. Releases XSHUT2 HIGH, initializes sensor 2, sets address to 0x31

This sequence runs in `initSensors()` in main.cpp.

## BLE Protocol

### Device Identification
- **Name:** "RH-Sensor" (or "RH-Sensor_XXXX" with MAC suffix for production)
- **Service UUID:** `4fafc201-0003-459e-8fcc-c5c9c331914b`

### Characteristics

| UUID | Properties | Description |
|------|------------|-------------|
| `beb5483e-36e1-4688-b7f5-ea07361b26a8` | READ, NOTIFY | Height readings |
| `beb5483e-36e1-4688-b7f5-ea07361b26a9` | WRITE | Commands |

### Commands (write to CMD characteristic)

| Char | Action |
|------|--------|
| `R` | Request single reading |
| `C` | Start continuous mode (~10Hz) |
| `S` | Stop continuous mode |
| `Z` | Zero/tare calibration |

### Data Format (HEIGHT characteristic notifications)

```
S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85
```

| Field | Description | Unit |
|-------|-------------|------|
| S1 | Sensor 1 reading | mm |
| S2 | Sensor 2 reading | mm |
| AVG | Averaged reading | mm |
| IN | Averaged reading | inches |
| BAT | Battery voltage | volts |

**Parsing example (mobile app):**
```typescript
const parts = data.split(',');
const values: Record<string, number> = {};
for (const part of parts) {
  const [key, val] = part.split(':');
  values[key] = parseFloat(val);
}
// values = { S1: 123.4, S2: 125.1, AVG: 124.2, IN: 4.89, BAT: 3.85 }
```

## Sensor Configuration

Defined in `include/config.h`:

| Parameter | Value | Notes |
|-----------|-------|-------|
| TIMING_BUDGET_MS | 33 | ~30Hz update rate |
| DISTANCE_MODE | Long | 4m range (vs 1.3m Short) |
| OUTLIER_THRESHOLD | 10 | mm difference to flag |

### Outlier Rejection Logic

In `takeReading()`:
- If `|S1 - S2| > OUTLIER_THRESHOLD`: Uses lower value (closer = more reliable reflection)
- Otherwise: Averages both sensors
- Zero offset applied after averaging

## LED States

| State | Pattern | Meaning |
|-------|---------|---------|
| Solid | Continuous ON | Ready, waiting for connection |
| Fast blink | 100ms toggle | Taking measurement |
| Slow blink | 500ms toggle | Connected, idle |
| Off | - | Error or no power |

LED behavior controlled by `setLed()` function and `LedState` enum.

## Button Behavior

- **Single press:** Triggers single reading (same as 'R' command)
- **Debounce:** 50ms in ISR
- **Long press:** Reserved for future (continuous mode toggle)

Button handled by `buttonISR()` with `IRAM_ATTR` for interrupt safety.

## Battery Monitoring

- **ADC Pin:** GPIO2
- **Voltage Divider:** 100kΩ / 100kΩ (2:1 ratio)
- **Range:** 3.0V (0%) to 4.2V (100%)
- **Check Interval:** Every 30 seconds

Calculation in `readBatteryVoltage()`:
```cpp
float voltage = (raw / 4095.0) * 2.5 * 2.0;  // ADC range × divider ratio
```

## Key Dependencies

Defined in `platformio.ini`:

| Library | Version | Purpose |
|---------|---------|---------|
| pololu/VL53L1X | ^1.3.1 | ToF sensor driver |
| h2zero/NimBLE-Arduino | ^1.4.1 | Lightweight BLE stack |

**Why NimBLE over Bluedroid:**
- 50% smaller flash footprint
- Lower RAM usage (critical for ESP32-C3)
- Faster connection times
- Sufficient for peripheral role

## File Structure

```
ride-height-sensor/
├── platformio.ini          # Build config, dependencies
├── include/
│   └── config.h            # All pin defs, UUIDs, constants
├── src/
│   └── main.cpp            # Single-file implementation
├── README.md               # User documentation
└── CLAUDE.md               # This file
```

## Common Modifications

### Changing Sensor Timing (Accuracy vs Speed)

In `config.h`:
```cpp
#define TIMING_BUDGET_MS  33   // Current: ~30Hz
// Options:
//   20  = Fast, less accurate
//   33  = Balanced (default)
//   50  = Slower, more accurate
//   100 = Slow, most accurate
```

### Adjusting Outlier Threshold

In `config.h`:
```cpp
#define OUTLIER_THRESHOLD 10  // mm
// Increase if sensors legitimately read different (angled surface)
// Decrease for stricter matching requirement
```

### Adding MAC Suffix to Device Name

In `main.cpp` `initBLE()`:
```cpp
// Current:
NimBLEDevice::init(BLE_DEVICE_NAME);

// For unique names:
uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);
char name[20];
snprintf(name, sizeof(name), "RH-Sensor_%02X%02X", mac[4], mac[5]);
NimBLEDevice::init(name);
```

### Changing Continuous Mode Rate

In `main.cpp` loop():
```cpp
// Current: TIMING_BUDGET_MS + 10 (~43ms = ~23Hz actual)
if (continuousMode && (millis() - lastReading > TIMING_BUDGET_MS + 10))

// For 10Hz:
if (continuousMode && (millis() - lastReading > 100))
```

## Troubleshooting

### "Sensor 1/2 init failed"
- Check I2C wiring (SDA to GPIO4, SCL to GPIO5)
- Verify 3.3V power to sensors
- Check XSHUT connections (GPIO6, GPIO7)
- Try slower I2C: `Wire.setClock(100000);`

### "Sensor timeout"
- Sensor blocked or too far from target
- Target surface too dark/absorptive
- Check for I2C bus conflicts

### "BLE not advertising"
- NimBLE init failed (check serial output)
- Flash may be full - check build size
- Try `pio run --target erase` then re-upload

### Battery reads 0V or wrong
- Check voltage divider wiring
- Verify ADC pin (GPIO2)
- ESP32-C3 ADC is 0-2.5V range, not 3.3V

### Readings inconsistent
- Increase TIMING_BUDGET_MS for more accuracy
- Check sensor mounting (must be parallel to ground)
- Textured surfaces cause scatter - sensors should agree within 5mm on flat floor

### Mobile app not receiving data
- Verify data format matches: `S1:x,S2:x,AVG:x,IN:x,BAT:x`
- Check characteristic UUID matches app config
- Ensure NOTIFY is enabled (app must subscribe)

## Serial Debug Output

At 115200 baud, firmware outputs:
```
=== Ride Height Sensor ===
CrewChiefSteve Technologies
==========================

Init sensor 1...
Sensor 1 at 0x30
Init sensor 2...
Sensor 2 at 0x31
Both sensors initialized OK
Init BLE...
BLE advertising started

Ready! Press button or send BLE command.

Height: 4.89" (124.2mm)
Battery: 3.85V (71%)
BLE: Client connected
CMD: Single reading requested
Height: 5.12" (130.1mm)
```

## Mobile App Integration

The companion mobile app is at `apps/mobile-ride-height/` in the CrewChiefSteve monorepo.

**Data flow:**
```
ESP32-C3 (this firmware)
  ↓ BLE NOTIFY (on button press or continuous)
Mobile App (useRideHeightDevice hook)
  ↓ Parses "S1:x,S2:x,AVG:x,IN:x,BAT:x"
UI Display
  ↓ mmToFractionalInches() → "5-3/16""
Reading History (last 20)
```

**Testing without mobile app:**
Use nRF Connect or LightBlue app to:
1. Scan for "RH-Sensor"
2. Connect and discover services
3. Subscribe to HEIGHT characteristic (enable notifications)
4. Write "R" to CMD characteristic
5. Observe notification with reading data
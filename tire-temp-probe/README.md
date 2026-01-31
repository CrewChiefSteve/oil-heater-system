# Tire Temperature Probe

Wireless thermocouple-based tire and brake temperature measurement system for motorsports applications.

## Features

- **4-Channel Thermocouple System**
  - 3 tire temperature probes (inside, middle, outside)
  - 1 brake rotor temperature probe
  - K-type thermocouples with MAX31855K amplifiers

- **Wireless BLE Transmission**
  - Real-time temperature data broadcast
  - System status and battery monitoring
  - Configurable corner assignment (FL/FR/RL/RR)

- **Battery Powered**
  - 2000mAh LiPo battery
  - USB-C charging with TP4056
  - Battery level monitoring
  - Low battery warning

- **Visual Status Indication**
  - WS2812B RGB LED
  - Color-coded status indication
  - Connection and measurement feedback

## Hardware Requirements

### Components
- ESP32-S3-WROOM-1 development board
- 4x MAX31855K thermocouple amplifier modules
- 4x K-type thermocouples
- 1x WS2812B RGB LED (single pixel)
- 1x TP4056 USB-C charging module
- 1x 2000mAh LiPo battery (3.7V)
- Optional: Buzzer for audio alerts

### Wiring

**SPI Bus (Shared)**:
- ESP32 GPIO18 → MAX31855 SCK (all 4 modules)
- ESP32 GPIO19 → MAX31855 SO/MISO (all 4 modules)
- ESP32 3.3V → MAX31855 VCC (all 4 modules)
- ESP32 GND → MAX31855 GND (all 4 modules)

**Chip Select Pins**:
- ESP32 GPIO5 → Tire Inside CS
- ESP32 GPIO6 → Tire Middle CS
- ESP32 GPIO7 → Tire Outside CS
- ESP32 GPIO15 → Brake Rotor CS

**LED**:
- ESP32 GPIO48 → WS2812B Data In
- 5V → WS2812B VCC
- GND → WS2812B GND

**Power Management**:
- ESP32 GPIO1 → Battery voltage divider (ADC)
- ESP32 GPIO2 → TP4056 CHRG pin

**Thermocouples**:
- K-type thermocouples to MAX31855 screw terminals (observe polarity)

## Software Setup

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install)
2. Install USB-to-Serial drivers for ESP32-S3 (if needed)

### Building and Uploading

```bash
# Navigate to project directory
cd tire-temp-probe

# Build the firmware
pio run

# Upload to ESP32-S3
pio run --target upload

# Upload and monitor serial output (115200 baud)
pio run --target upload && pio device monitor

# Clean build files
pio run --target clean
```

### Dependencies

All dependencies are automatically installed by PlatformIO:
- NimBLE-Arduino (BLE stack)
- Adafruit MAX31855 library (thermocouple interface)
- FastLED (RGB LED control)

## Configuration

Edit `include/config.h` to customize:

```cpp
// Temperature reading interval (ms)
#define TEMP_READ_INTERVAL_MS   250

// BLE transmission interval (ms)
#define BLE_TX_INTERVAL_MS      500

// Temperature smoothing samples
#define TEMP_SMOOTHING_SAMPLES  8

// Battery thresholds
#define BATTERY_LOW_THRESHOLD   10  // Percentage

// Default corner assignment
#define DEFAULT_CORNER  CORNER_FL  // FL, FR, RL, or RR
```

## Usage

### Startup

1. Power on the device
2. LED blinks yellow during initialization
3. LED turns green (slow blink) when ready
4. Device advertises as "TireProbe_XX" via BLE (where XX = corner ID: LF, RF, LR, RR)

### Operation

**LED Status Codes**:
- **Yellow fast blink**: Initializing
- **Green slow blink**: Idle, waiting for connection
- **Blue solid**: Connected, idle
- **Green solid**: Connected, measuring
- **Cyan very fast**: Transmitting data
- **Orange blink**: Low battery (<10%)
- **Red fast blink**: Sensor error

### Serial Monitor Output

Connect at 115200 baud to view real-time data:

```
Tire [IN/MID/OUT]: 85.2 / 87.5 / 84.8 C  |  Brake: 215.3 C  |  Bat: 78% (CHG)
```

### BLE Protocol (v2)

**Device Name**: `TireProbe_XX` (where XX = corner ID: LF, RF, LR, RR)

**Service UUID**: `4fafc201-0004-459e-8fcc-c5c9c331914b`

**Note**: This UUID MUST match `SERVICE_UUIDS.TIRE_TEMP_PROBE` in `@crewchiefsteve/ble` package. See `packages/ble/src/constants/uuids.ts` for all device UUIDs.

#### Characteristics

| Characteristic | UUID | Properties | Data Format | Description |
|----------------|------|------------|-------------|-------------|
| **CORNER_READING** | `beb5483e-36e1-4688-b7f5-ea07361b26ac` | NOTIFY | JSON string | Corner temperature data on capture |
| **SYSTEM_STATUS** | `beb5483e-36e1-4688-b7f5-ea07361b26aa` | NOTIFY | Binary (8 bytes) | Battery, state, capture count |

#### CORNER_READING Format (JSON)
Sent when a corner is captured during sequential workflow:
```json
{
  "corner": "RF",
  "tireInside": 85.0,
  "tireMiddle": 88.5,
  "tireOutside": 82.3,
  "brakeTemp": 176.5
}
```
All temperatures in Celsius.

#### SYSTEM_STATUS Format (Binary, 8 bytes)
```
[State:1] [BatteryPct:1] [BatteryV:4f] [Charging:1] [CapturedCount:1]
```

**Notes**:
- All NOTIFY characteristics include BLE2902 descriptors for iOS compatibility
- Device controls capture sequence (RF → LF → LR → RR)
- Mobile app is a passive receiver that logs data

## Temperature Measurement

### Tire Temperatures

Three probes measure across the tire contact patch:
- **Inside**: Inner edge temperature
- **Middle**: Center temperature
- **Outside**: Outer edge temperature

Uneven temperatures indicate camber or tire pressure issues.

### Brake Temperature

Single probe measures brake rotor surface temperature. Useful for monitoring brake thermal load during track sessions.

## Troubleshooting

### Sensor Errors

**Symptom**: Red LED blinking, "Sensor Error" in serial output

**Solutions**:
- Check thermocouple wiring and polarity
- Verify MAX31855 module connections (SCK, CS, MISO)
- Ensure thermocouples are K-type
- Check for shorts or damaged thermocouple wires

### BLE Not Connecting

**Solutions**:
- Verify device is advertising (check serial output)
- Ensure receiver is scanning on correct service UUID
- Power cycle both devices
- Check ESP32-S3 has BLE antenna connected

### Incorrect Battery Readings

**Solutions**:
- Calibrate `BATTERY_VOLTAGE_DIVIDER` in `include/config.h`
- Verify voltage divider resistor values
- Check ADC pin connection (GPIO1)

### Unstable Temperature Readings

**Solutions**:
- Increase `TEMP_SMOOTHING_SAMPLES` (default 8)
- Ensure good thermocouple contact with measurement surface
- Shield SPI wiring from electromagnetic interference
- Check for loose connections

## Project Structure

```
tire-temp-probe/
├── platformio.ini          # Build configuration
├── include/
│   ├── pins.h              # GPIO pin assignments
│   ├── config.h            # User-configurable settings
│   ├── types.h             # Data structures
│   ├── ble_protocol.h      # BLE packet formats
│   ├── probes.h            # Thermocouple interface
│   ├── ble_service.h       # BLE service interface
│   ├── led.h               # LED control interface
│   └── power.h             # Battery management interface
└── src/
    ├── main.cpp            # Main application
    ├── probes.cpp          # Thermocouple reading
    ├── ble_service.cpp     # BLE implementation
    ├── led.cpp             # LED status indication
    └── power.cpp           # Battery monitoring
```

## Safety Notes

- Maximum temperature limit: 400°C
- Do not exceed thermocouple temperature ratings
- Ensure proper thermocouple attachment to prevent damage
- Monitor battery temperature during charging
- Use appropriate wire gauge for battery connections

## License

See repository LICENSE file.

## Contributing

Contributions welcome! Please follow existing code style and test thoroughly before submitting pull requests.

## Support

For issues and questions, see CLAUDE.md for detailed architecture documentation.

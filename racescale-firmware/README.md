# RaceScale V4.0 Firmware

ESP32-S3 based precision race car scale with BLE connectivity and configurable corner identity.

## Features

- **High-Speed Weight Measurement**
  - 80 Hz HX711 sampling with 2-sample averaging
  - Adaptive filtering (fast mode 0.7α during changes, slow mode 0.2α when stable)
  - Zero deadband (0.3 lbs) prevents wandering at zero
  - 40 Hz OLED display updates
  - 4 Hz BLE notifications

- **Temperature Compensation**
  - DS18B20 temperature sensor (±0.5°C accuracy)
  - Automatic calibration factor adjustment (0.0002°C coefficient)
  - Non-blocking async temperature readings every 5s

- **Configurable Corner Identity**
  - Store corner assignment in NVS (LF, RF, LR, RR, or custom like 01, 02, etc.)
  - BLE device name: `RaceScale_XX` (where XX is corner ID)
  - Configure via serial command (`corner LF`) or BLE characteristic
  - Defaults to `RaceScale_01` if not configured

- **BLE Connectivity (CrewChiefSteve Standard)**
  - Service UUID: `4fafc201-0002-459e-8fcc-c5c9c331914b`
  - Compatible with @crewchiefsteve/ble package
  - Real-time weight notifications
  - Remote tare and calibration
  - Corner ID read/write support

- **Robust Button Interface**
  - Short press: Precision tare (10-sample average)
  - Long press (3s): Enter calibration mode
  - Debounced with 200ms timing

- **Serial Calibration Interface**
  - Interactive command-line calibration
  - Live debugging output with stability indicators
  - NVS persistence (survives power cycles)

## Hardware Requirements

### Core Components
- ESP32-S3-DevKitC-1 (or compatible)
- HX711 load cell amplifier
- Load cell (tested with 200 kg cells)
- DS18B20 temperature sensor
- 0.96" SSD1306 OLED display (128x64)
- Momentary push button (tare)

### Pin Configuration (ESP32-S3)

| Function | GPIO | Description |
|----------|------|-------------|
| HX711 DOUT | 42 | Data output |
| HX711 CLK | 41 | Clock input |
| DS18B20 | 6 | OneWire data |
| Button | 5 | Tare button (active LOW) |
| I2C SDA | 8 | OLED data |
| I2C SCL | 9 | OLED clock |

**Note**: GPIO 41 and 42 are JTAG pins on ESP32-S3. The firmware releases these pins on startup using `gpio_reset_pin()`.

## Software Setup

### Prerequisites

1. Install [PlatformIO](https://platformio.org/install) (via VSCode extension or CLI)
2. Install USB-to-Serial drivers for ESP32-S3 (if needed)

### Building and Uploading

```bash
# Navigate to project directory
cd racescale-firmware

# Build the firmware
pio run

# Upload to ESP32-S3
pio run --target upload

# Upload and monitor serial output (115200 baud)
pio run --target upload && pio device monitor

# Clean build files
pio run --target clean
```

### First-Time Setup

1. **Flash firmware** to ESP32-S3
2. **Connect to serial** monitor (115200 baud)
3. **Set corner identity**:
   ```
   corner LF
   ```
   Or via BLE (write to Corner characteristic)
4. **Calibrate** with known weight:
   ```
   cal 25
   ```
   (Place 25 lb weight on scale before running command)
5. **Restart** to apply corner ID to BLE device name

## Serial Commands

Open serial monitor at 115200 baud and type commands:

| Command | Description |
|---------|-------------|
| `cal 25` | Calibrate to 25 lbs (place known weight first) |
| `tare` | Zero the scale (precision 10-sample tare) |
| `corner LF` | Set corner identity (LF, RF, LR, RR, 01-99, etc.) |
| `info` | Display current settings and status |
| `raw` | Show raw 10-sample reading |
| `reset` | Clear NVS, restore defaults |
| `help` | Show command help |

## BLE Protocol

### Service UUID
`4fafc201-0002-459e-8fcc-c5c9c331914b`

**Note**: This UUID MUST match `SERVICE_UUIDS.RACESCALE` in `@crewchiefsteve/ble` package. See `packages/ble/src/constants/uuids.ts` for all device UUIDs.

### Characteristics

| Characteristic | UUID | Properties | Data Format | Description |
|----------------|------|------------|-------------|-------------|
| **WEIGHT** | `beb5483e-36e1-4688-b7f5-ea07361b26a8` | READ, NOTIFY | Float32LE (4 bytes) | Current weight in kg |
| **ZERO** | `beb5483e-36e1-4688-b7f5-ea07361b26a9` | WRITE | String | Zero/tare command (send "ZERO") |
| **TEMPERATURE** | `beb5483e-36e1-4688-b7f5-ea07361b26ab` | READ, NOTIFY | Float32LE (4 bytes) | Load cell temperature in Celsius |
| **CALIBRATION** | `beb5483e-36e1-4688-b7f5-ea07361b26ac` | READ, WRITE, NOTIFY | Float32LE (4 bytes) | Calibration factor |
| **STATUS** | `beb5483e-36e1-4688-b7f5-ea07361b26aa` | READ, NOTIFY | String | Status flags (zero/tare state, battery, etc.) |
| **CORNER_ID** | `beb5483e-36e1-4688-b7f5-ea07361b26af` | READ, WRITE, NOTIFY | UInt8 (1 byte) | Corner assignment: 0=LF, 1=RF, 2=LR, 3=RR |

**Notes**:
- All NOTIFY characteristics include BLE2902 descriptors for iOS compatibility
- Float32LE values are IEEE 754 single-precision floats in little-endian byte order
- CORNER_ID uses numeric values: 0=LF, 1=RF, 2=LR, 3=RR (not strings)

### BLE Connection Example (Web Bluetooth)

```javascript
// 1. Connect to device
const device = await navigator.bluetooth.requestDevice({
  filters: [{ namePrefix: 'RaceScale_' }],
  optionalServices: ['4fafc201-0002-459e-8fcc-c5c9c331914b']
});

const server = await device.gatt.connect();
const service = await server.getPrimaryService('4fafc201-0002-459e-8fcc-c5c9c331914b');

// 2. Subscribe to weight notifications
const weightChar = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26a8');
await weightChar.startNotifications();
weightChar.addEventListener('characteristicvaluechanged', (event) => {
  const weight = new TextDecoder().decode(event.target.value);
  console.log(`Weight: ${weight} lbs`);
});

// 3. Tare the scale
const tareChar = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26a9');
await tareChar.writeValue(new TextEncoder().encode('1'));

// 4. Read corner ID
const cornerChar = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26ad');
const cornerValue = await cornerChar.readValue();
const cornerID = new TextDecoder().decode(cornerValue);
console.log(`Corner: ${cornerID}`);

// 5. Set corner ID (requires restart to update device name)
await cornerChar.writeValue(new TextEncoder().encode('LF'));
```

## Configuration

Edit `include/config.h` to customize:

### Filter Tuning
```cpp
static constexpr float FAST_FILTER_ALPHA = 0.7f;      // Fast response (0-1)
static constexpr float SLOW_FILTER_ALPHA = 0.20f;     // Stable filtering
static constexpr float ZERO_DEADBAND = 0.3f;          // Snap-to-zero threshold (lbs)
```

### Update Rates
```cpp
static constexpr uint32_t UPDATE_RATE_MS = 25;        // Display update (40Hz)
static constexpr uint32_t BLE_UPDATE_MS = 250;        // BLE update (4Hz)
static constexpr uint32_t TEMP_UPDATE_MS = 5000;      // Temp sensor (0.2Hz)
```

### Calibration Defaults
```cpp
#define DEFAULT_CALIBRATION 2843.0f  // HX711 calibration factor
#define DEFAULT_CORNER "01"           // Default corner if not set
```

## Project Structure

```
racescale-firmware/
├── platformio.ini          # ESP32-S3 board config, dependencies
├── src/
│   └── main.cpp            # Main application code
├── include/
│   ├── config.h            # Pin definitions, constants, tuning
│   ├── ble_protocol.h      # BLE UUIDs (CrewChiefSteve standard)
│   ├── adaptive_filter.h   # Adaptive filtering class
│   └── button_handler.h    # Button debouncing class
├── lib/                    # Local libraries (if needed)
└── README.md               # This file
```

## Calibration Procedure

### Method 1: Serial (Recommended)

1. Connect to serial monitor (115200 baud)
2. Remove all weight from scale
3. Type `tare` and press Enter
4. Place known weight (e.g., 25 lbs)
5. Wait for reading to stabilize (watch for "✅ STABLE")
6. Type `cal 25` and press Enter
7. Calibration saved to NVS automatically

### Method 2: BLE

1. Connect to scale via BLE
2. Tare: Write "1" to Tare characteristic
3. Place known weight (e.g., 25 lbs)
4. Write "25" to Calibration characteristic
5. Calibration saved automatically

### Method 3: Button (Emergency)

1. Press and hold button for 3 seconds
2. Display shows "CALIBRATION MODE"
3. Place known weight
4. Use serial or BLE to complete calibration

## Temperature Compensation

The scale automatically compensates for temperature changes using the formula:

```
compensated_cal = base_cal × (1 + 0.0002 × (temp - 70°F))
```

This accounts for thermal expansion of the load cell and mounting hardware. Temperature is read every 5 seconds from the DS18B20 sensor.

## Troubleshooting

### "OLED init failed"
- Check I2C wiring (SDA=GPIO8, SCL=GPIO9)
- Verify OLED address is 0x3C (some are 0x3D)
- Add 4.7kΩ pull-up resistors to SDA/SCL if using long wires

### "DS18B20 error - retrying..."
- Check DS18B20 wiring (Data=GPIO6, needs 4.7kΩ pull-up to 3.3V)
- Verify sensor polarity (GND, Data, VCC)
- Temperature errors won't prevent weight measurement

### Weight reading unstable
- Ensure load cell is securely mounted
- Check for mechanical binding or friction
- Increase `SLOW_FILTER_ALPHA` (e.g., to 0.25) in config.h
- Adjust `STABILITY_RANGE` (try 0.20 instead of 0.15)

### Scale reads negative or incorrect values
- Swap HX711 E+ and E- wires (reverses polarity)
- Recalibrate with known weight
- Check `BASE_CALIBRATION` value (should be ~2800-3000 for typical cells)

### BLE won't connect
- Check device name in serial output
- Ensure corner ID is set (default: RaceScale_01)
- Restart ESP32-S3 after setting corner ID
- BLE automatically restarts advertising after disconnect

### Button not responding
- Check button wiring (GPIO5 to GND when pressed)
- Verify internal pull-up is enabled (pinMode INPUT_PULLUP)
- Adjust `BUTTON_DEBOUNCE_MS` if needed (default 200ms)

### NVS errors / "reset" doesn't work
- Flash partition table may be corrupted
- Re-flash with `--erase-flash` option:
  ```bash
  pio run --target erase
  pio run --target upload
  ```

## Advanced Features

### Filter Behavior

The adaptive filter automatically switches between two modes:

- **Fast Mode (α=0.7)**: Activated when weight change > 0.3 lbs
  - Responds quickly to new weights
  - Active during loading/unloading

- **Slow Mode (α=0.2)**: Activated after 1.5s of stability
  - Heavy filtering for clean, locked display
  - Reduces noise when weight is stable

### Zero Deadband

Readings under 0.3 lbs are snapped to exactly 0.00. This prevents:
- Display wandering between -0.1 and +0.1
- False weight readings from thermal drift
- Noise when scale is empty

### NVS Storage

Settings are stored in ESP32 non-volatile storage:
- `cal_factor`: HX711 calibration (float)
- `corner_id`: Corner identity string (max 15 chars)

NVS namespace: `racescale_v3`

## Version History

### V4.0 (Current)
- Added configurable corner identity (BLE + NVS)
- Updated to CrewChiefSteve BLE UUID standard (0002)
- Modular code structure (separate header files)
- ESP32-S3 target with custom pins

### V3.1 (Production)
- Tuned filter parameters (0.7/0.20 alpha)
- Added zero deadband (0.3 lbs)
- Serial calibration commands
- Improved temperature handling
- ESP32-S3 JTAG pin release

### V2.0 (Optimized)
- Adaptive filtering implementation
- Async temperature reading
- NVS calibration persistence
- BLE auto-reconnect

## License

MIT License - See LICENSE file for details

## Credits

- Original RaceScale V3_LF by CrewChiefSteve
- HX711 library by bogde
- Adafruit GFX & SSD1306 libraries
- ESP32 Arduino core by Espressif

## Support

For issues or questions:
- GitHub Issues: (your repository URL)
- Serial debug output: Type `info` for current settings
- BLE UUID reference: packages/ble/src/constants/uuids.ts

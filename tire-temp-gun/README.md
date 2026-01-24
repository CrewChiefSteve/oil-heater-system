# Tire Temperature Gun

ESP32-based handheld infrared thermometer for race car tire temperature measurement with BLE connectivity.

## Features

- **Non-contact temperature measurement** using MLX90614 IR sensor
- **Four measurement modes**: Instant, Hold, Max, Min
- **Red laser pointer** for precise aiming (activates on trigger press)
- **128x64 OLED display** for clear readings
- **BLE connectivity** for mobile app integration (JSON data streaming)
- **Fahrenheit/Celsius toggle** with Hold button
- **Max/Min tracking** with long-press reset
- **Battery level monitoring** via voltage divider
- **Audible feedback** on button presses
- **Optimized for rubber tires** (0.95 emissivity)

## Hardware

### Parts List

| Component | Specification | Quantity | Notes |
|-----------|--------------|----------|-------|
| ESP32-WROOM-32 | DevKit v1 | 1 | Main controller |
| MLX90614 | IR temperature sensor | 1 | I2C address 0x5A |
| SSD1306 | 128x64 OLED display | 1 | I2C address 0x3C |
| Red laser module | 5mW, 3-5V | 1 | Aiming pointer |
| Tactile buttons | 6x6mm | 3 | Trigger, Mode, Hold |
| Piezo buzzer | 5V active | 1 | Audible feedback |
| Resistors | 10kΩ | 2 | Voltage divider (R1, R2) |
| Li-ion battery | 18650, 3.7V | 1 | Power source |
| Battery holder | 18650 | 1 | With built-in protection |
| Slide switch | SPST | 1 | Power on/off |
| Enclosure | Handheld pistol grip | 1 | Custom or off-the-shelf |
| Prototype board | - | 1 | For assembly |
| Wire | 22-24 AWG | - | Various colors |

### Pin Connections

#### I2C Devices (shared bus)

| Device | VCC | GND | SDA | SCL | Address |
|--------|-----|-----|-----|-----|---------|
| MLX90614 | 3.3V | GND | GPIO21 | GPIO22 | 0x5A |
| SSD1306 | 3.3V | GND | GPIO21 | GPIO22 | 0x3C |

**Note**: Add 4.7kΩ pull-up resistors on SDA and SCL lines if not already present on breakout boards.

#### Digital I/O

| Component | ESP32 Pin | Configuration | Notes |
|-----------|-----------|---------------|-------|
| Trigger Button | GPIO13 | INPUT_PULLUP | Active LOW |
| Mode Button | GPIO12 | INPUT_PULLUP | Active LOW |
| Hold Button | GPIO14 | INPUT_PULLUP | Active LOW |
| Laser Module | GPIO27 | OUTPUT | Active HIGH |
| Piezo Buzzer | GPIO25 | OUTPUT | PWM capable |
| Battery Sense | GPIO34 | INPUT (ADC) | Via voltage divider |

#### Battery Voltage Divider

```
Battery+ ----[R1: 10kΩ]---- GPIO34 (ADC) ----[R2: 10kΩ]---- GND
```

This creates a 2:1 divider for 4.2V Li-ion max (2.1V at ADC, within ESP32's 3.3V limit).

#### Power Distribution

- **Battery+** → Slide switch → **VIN** pin on ESP32 DevKit
- **GND** → Common ground for all components
- **3.3V** → From ESP32 regulator to MLX90614 and SSD1306
- **5V** (from USB or boost converter) → Laser module (if using 5V laser)

## Wiring Diagram

```
                    ┌─────────────┐
                    │   ESP32     │
                    │  DevKit v1  │
                    └─────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
   ┌────▼────┐       ┌───▼────┐      ┌─────▼─────┐
   │ MLX90614│       │SSD1306 │      │  Buttons  │
   │ (0x5A)  │       │ (0x3C) │      │ GPIO12-14 │
   └─────────┘       └────────┘      └───────────┘
        │                 │                 │
   I2C (21,22)       I2C (21,22)     INPUT_PULLUP
        │                 │                 │
        └─────────────────┴─────────────────┘
                          │
                   ┌──────▼──────┐
                   │   Laser     │
                   │   GPIO27    │
                   └─────────────┘
```

## Software Setup

### Install PlatformIO

```bash
# Using PlatformIO Core CLI
pio --version

# Or use Python
python -m platformio --version
```

### Build and Upload

```bash
cd tire-temp-gun

# Build the project
pio run

# Upload to ESP32 (auto-detect port)
pio run --target upload

# Upload to specific port (Windows)
pio run --target upload --upload-port COM3

# Upload to specific port (Linux/Mac)
pio run --target upload --upload-port /dev/ttyUSB0

# Upload and monitor serial output
pio run --target upload && pio device monitor

# Monitor serial output only (115200 baud)
pio device monitor
```

### Configuration

Edit `include/config.h` to customize:

- **Pin assignments** (if using different GPIOs)
- **Battery voltage divider ratio** (if using different resistor values)
- **Temperature reading interval** (default 100ms)
- **BLE notify interval** (default 250ms)
- **Emissivity value** (default 0.95 for rubber)
- **Buzzer frequencies** for different events

## Usage

### Physical Controls

| Button | Short Press | Long Press (2s) |
|--------|-------------|-----------------|
| **Trigger** | Activate laser pointer | - |
| **Mode** | Cycle modes: Instant → Hold → Max → Min | - |
| **Hold** | Toggle Fahrenheit/Celsius | Reset Max/Min values |

### Measurement Modes

1. **INSTANT** (default)
   - Live temperature reading updates continuously
   - Tracks max/min in background

2. **HOLD**
   - Freezes the current reading
   - Useful for recording temperatures without looking at the display

3. **MAX**
   - Displays the maximum temperature recorded
   - Updates automatically as higher temps are measured

4. **MIN**
   - Displays the minimum temperature recorded
   - Updates automatically as lower temps are measured

### Display Layout

```
┌──────────────────────────┐
│ MODE        BAT: 85%     │
│ BLE                      │
│                          │
│   185.5°F                │
│                          │
│                          │
│ Ambient: 72.3°F          │
└──────────────────────────┘
```

- **Top left**: Current mode (INSTANT/HOLD/MAX/MIN)
- **Top right**: Battery percentage
- **Line 2**: "BLE" indicator when connected
- **Center**: Large temperature reading
- **Bottom**: Ambient temperature

### BLE Mobile App Integration

**Device Name**: `TireTempGun`

**Service UUID**: `4fafc201-0005-459e-8fcc-c5c9c331914b`

#### Temperature Characteristic (Notify)
**UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`

JSON payload format (sent every 250ms when connected):
```json
{
  "temp": 185.5,   // Current/held/max/min based on mode (°F or °C)
  "amb": 72.3,     // Ambient temperature
  "max": 195.2,    // Maximum recorded
  "min": 175.8,    // Minimum recorded
  "bat": 85,       // Battery percentage
  "mode": 0,       // 0=Instant, 1=Hold, 2=Max, 3=Min
  "unit": "F"      // "F" or "C"
}
```

#### Command Characteristic (Write)
**UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a9`

Send JSON commands from mobile app:
```json
{"reset": true}           // Reset max/min values
{"mode": 2}               // Set mode (0-3)
{"unit": "C"}             // Set unit ("F" or "C")
```

### Typical Workflow

1. Power on device with slide switch
2. Wait for startup beeps (two short beeps)
3. Point laser at tire surface (press trigger)
4. Read temperature on OLED display
5. Press Mode to switch to HOLD to freeze reading
6. Release trigger (laser turns off)
7. Record temperature
8. Press Mode again to return to INSTANT
9. Repeat for all four tires

### Tips for Accurate Readings

- **Aim perpendicular** to the tire surface (not at an angle)
- **Keep distance consistent** (6-12 inches recommended)
- **Measure same location** on each tire (e.g., center of contact patch)
- **Wait for sensor stabilization** (~1 second per reading)
- **Avoid shiny surfaces** (use matte areas of tire)
- **Check ambient temperature** (displayed at bottom of screen)
- **Calibrate emissivity** if measuring non-rubber surfaces

## Troubleshooting

### Display shows "ERROR: MLX90614 not found!"
- Check I2C wiring (SDA to GPIO21, SCL to GPIO22)
- Verify MLX90614 has 3.3V power and common ground
- Check for 4.7kΩ pull-up resistors on SDA/SCL
- Try lower I2C speed (change `Wire.setClock(100000)` in main.cpp)

### Display shows "ERROR: SSD1306 not found!"
- Verify SSD1306 I2C address (0x3C or 0x3D, check with I2C scanner)
- Update `SSD1306_ADDR` in config.h if needed
- Check power and ground connections

### Laser won't turn on
- Verify laser module has correct voltage (3.3V or 5V depending on module)
- Check GPIO27 connection
- Test with multimeter (should be ~3.3V when trigger pressed)

### Temperature readings seem incorrect
- Emissivity may need adjustment (default 0.95 for rubber)
- Ensure clean line of sight to target (no obstructions)
- Verify sensor is not too close or too far from target
- Check ambient temperature compensation

### BLE not connecting
- Ensure Bluetooth is enabled on mobile device
- Look for device name "TireTempGun" in BLE scanner app
- Check serial monitor for "BLE advertising started" message
- Try power cycling the device

### Battery drains quickly
- Check for continuous laser activation (should only be on when trigger pressed)
- Verify no short circuits on power rails
- Consider using sleep mode when idle (requires code modification)
- Display brightness is always at max (OLED limitation with this driver)

### Buttons not responding
- Verify INPUT_PULLUP configuration in code
- Check button wiring (one side to GPIO, other to GND)
- Test with multimeter (should show ~3.3V when released, 0V when pressed)
- Increase `DEBOUNCE_MS` in config.h if getting false triggers

## Calibration

### Emissivity Adjustment

Different materials require different emissivity settings:

| Material | Emissivity | Notes |
|----------|------------|-------|
| Rubber (tire) | 0.95 | Default setting |
| Asphalt | 0.93 | Track surface |
| Aluminum | 0.10-0.30 | Polished/anodized |
| Paint (matte) | 0.90-0.95 | Body panels |
| Carbon fiber | 0.80-0.90 | Matte finish |

To change emissivity, uncomment this line in `setup()` in main.cpp:
```cpp
mlx.writeEmissivity(DEFAULT_EMISSIVITY);
```

**WARNING**: This writes to the MLX90614's EEPROM (limited write cycles). Only use when necessary.

### Battery Calibration

If battery percentage is inaccurate:

1. Measure actual battery voltage with multimeter
2. Measure voltage at GPIO34 with multimeter
3. Calculate actual divider ratio: `ratio = batteryVoltage / gpio34Voltage`
4. Update `BAT_VOLTAGE_DIVIDER_RATIO` in config.h
5. Adjust `BAT_MAX_VOLTAGE` and `BAT_MIN_VOLTAGE` for your specific battery

## Future Enhancements

- [ ] Data logging to SD card
- [ ] Statistical analysis (average, standard deviation)
- [ ] Temperature trends graph on display
- [ ] WiFi connectivity for live telemetry
- [ ] Configurable emissivity via BLE
- [ ] Auto power-off after inactivity
- [ ] Multi-point measurement averaging
- [ ] External thermocouple input for validation
- [ ] Custom mobile app (iOS/Android)

## Safety Notes

- **Laser Safety**: 5mW red lasers are Class IIIa. Do not point at eyes or reflective surfaces.
- **Battery Safety**: Use protected 18650 cells. Never short circuit or exceed charge voltage.
- **Temperature Limits**: MLX90614 rated for -70°C to +382.2°C object temperature.
- **ESD Protection**: Handle MLX90614 carefully (sensitive to static discharge).

## License

This project is released under the MIT License. See LICENSE file for details.

## References

- [MLX90614 Datasheet](https://www.melexis.com/en/product/MLX90614/Digital-Plug-Play-Infrared-Thermometer-TO-Can)
- [ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
- [NimBLE Arduino Documentation](https://h2zero.github.io/NimBLE-Arduino/)
- [OLED Display Tutorial](https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/)

## Support

For issues or questions, please open an issue on the project repository.

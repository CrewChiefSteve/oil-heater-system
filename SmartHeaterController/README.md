# Smart Oil Heater Controller

ESP32-based thermostat for race car oil heating applications. Maintains engine oil at operating temperature before races.

## Hardware

- **ESP32-2432S028R** (3.5" CYD - Cheap Yellow Display) - ST7796 480x320 TFT with resistive touch
- **MCP23017** - I2C 16-bit I/O expander (address 0x27)
- **MAX6675** - K-type thermocouple interface
- **Relay Module** - Active HIGH, for heater control
- **K-Type Thermocouple** - Temperature sensor

## Wiring Diagram

```
┌──────────────────┐
│   CYD 3.5" ESP32 │
│                  │
│   GPIO32 (SDA) ──┼──── 4.7kΩ ─┬─ 3.3V
│                  │            │
│   GPIO25 (SCL) ──┼──── 4.7kΩ ─┤
│                  │            │
│   3.3V ──────────┼────────────┼───────┐
│   GND ───────────┼────────────┼───────┼───────┐
└──────────────────┘            │       │       │
                                │       │       │
        ┌───────────────────────┴───────┴───────┴────────┐
        │           MCP23017                              │
        │                                                 │
        │  SDA (pin 13) ◄── GPIO32                        │
        │  SCL (pin 12) ◄── GPIO25                        │
        │  VDD (pin 9)  ◄── 3.3V                          │
        │  VSS (pin 10) ◄── GND                           │
        │  RESET (pin 18) ── 3.3V                         │
        │  A0, A1 ── GND                                  │
        │  A2 ── 3.3V  (address = 0x27)                   │
        │                                                 │
        │  GPA0 ───────────► Relay IN                     │
        │  GPA1 ───────────► MAX6675 SCK                  │
        │  GPA2 ───────────► MAX6675 CS                   │
        │  GPA3 ◄────────── MAX6675 SO                    │
        └─────────────────────────────────────────────────┘
```

## Pin Reference Tables

### CYD 3.5" to MCP23017

| CYD GPIO | MCP23017 Pin | Notes |
|----------|--------------|-------|
| GPIO32   | SDA (pin 13) | Add 4.7kΩ pull-up to 3.3V |
| GPIO25   | SCL (pin 12) | Add 4.7kΩ pull-up to 3.3V |
| 3.3V     | VDD (pin 9)  | Power |
| GND      | VSS (pin 10) | Ground |

### MCP23017 Address Configuration

| A2  | A1  | A0  | I2C Address |
|-----|-----|-----|-------------|
| GND | GND | GND | 0x20        |
| 3.3V| GND | GND | 0x24        |
| 3.3V| 3.3V| 3.3V| **0x27** ✓  |

This project uses **0x27** (A2=3.3V, A1=3.3V, A0=3.3V)

### MCP23017 to Peripherals

| MCP Pin | Connected To | Function |
|---------|--------------|----------|
| GPA0    | Relay IN     | Heater control (active HIGH) |
| GPA1    | MAX6675 SCK  | Bit-bang SPI clock |
| GPA2    | MAX6675 CS   | Chip select (active LOW) |
| GPA3    | MAX6675 SO   | Data input (MISO) |

### MAX6675 Module

| MAX6675 Pin | Connected To |
|-------------|--------------|
| VCC         | 3.3V         |
| GND         | GND          |
| SCK         | MCP23017 GPA1|
| CS          | MCP23017 GPA2|
| SO          | MCP23017 GPA3|
| T+/T-       | K-Type Thermocouple |

### Relay Module

| Relay Pin | Connected To |
|-----------|--------------|
| VCC       | 5V (or 3.3V) |
| GND       | GND          |
| IN        | MCP23017 GPA0|
| COM       | AC Line      |
| NO        | Heater       |

## Features

- Touchscreen setpoint adjustment (UP/DOWN buttons)
- Temperature range: 50°F - 280°F setpoint
- ±1°F hysteresis (prevents relay cycling)
- 10-second minimum relay cycle time
- 300°F safety shutoff
- Sensor error detection
- Bluetooth Low Energy (BLE) support - broadcasts temperature, setpoint, and status
- Real-time temperature monitoring with MAX6675 thermocouple
- Visual feedback with color-coded display (red=heating/error, green=off, cyan=normal)

## Bluetooth

The device broadcasts as **"Heater_Controller"** via BLE and provides three characteristics:

1. **Temperature** - Current temperature reading
2. **Setpoint** - Target temperature
3. **Status** - JSON format: `{"heater":"ON/OFF","safety":"OK/SHUTDOWN","sensor":"OK/ERROR"}`

Connect with any BLE scanner app (nRF Connect, LightBlue, etc.) to monitor the heater remotely.

## Building

This is a PlatformIO project. To build:

```bash
pio run
```

To upload and monitor:

```bash
pio run -t upload -t monitor
```

Or using Python:

```bash
python -m platformio run -t upload -t monitor
```

## Parts List

| Item | Qty | Notes |
|------|-----|-------|
| ESP32 3.5" CYD (ST7796) | 1 | ESP32-2432S028R |
| MCP23017 module | 1 | Set address to 0x27 |
| MAX6675 module | 1 | Thermocouple interface |
| K-type thermocouple | 1 | High-temp rated for oil |
| Relay module (3.3V logic) | 1 | Active HIGH |
| 4.7kΩ resistors | 2 | I2C pull-ups (if not on module) |
| Dupont wires | ~15 | Connections |

## Configuration

Key settings in `include/config.h`:

- `DEFAULT_SETPOINT_F`: Default target temperature (180°F)
- `TEMP_HYSTERESIS_F`: Temperature control band (±1°F)
- `RELAY_MIN_CYCLE_TIME`: Minimum time between relay changes (10s)
- `SAFETY_MAX_TEMP_F`: Emergency shutoff temperature (300°F)

Touch calibration data is stored in `src/main.cpp` (line 14). Send 'c' via serial to recalibrate.

## Serial Commands

- `c` or `C` - Run touchscreen calibration

## Development Notes

This project went through several hardware iterations due to ESP32-S3 I2C driver bugs in Arduino-ESP32 3.x. The original ESP32 (non-S3) in the CYD boards works reliably.

The MAX6675 uses bit-banged SPI through the MCP23017 because all direct ESP32 GPIO pins are used by the display and touch controller.

## License

MIT License - See LICENSE file for details

## Author

CrewChiefSteve Technologies

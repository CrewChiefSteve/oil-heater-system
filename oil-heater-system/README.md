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
5. **Use display controls** to adjust setpoint (50-150°C range) and enable/disable heating
6. **Heater cycles on/off** around setpoint with 2°C hysteresis

## Safety Features

- **Watchdog**: Heater turns OFF if no UI command for 5 seconds
- **Overtemp Cutoff**: Hard shutdown at 160°C (320°F)
- **Sensor Fault**: Heater disabled if thermocouple disconnected
- **Status Display**: Real-time fault indication on screen

## CRITICAL: Hardware Thermal Cutoff

**Do NOT rely solely on software for safety!**

Wire a snap-disc thermostat (normally closed) in series with your relay. Set it 10-15°C above your maximum operating temperature. This provides hardware-level protection if the ESP32 crashes with the relay ON.

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
static constexpr int NUM_SAMPLES          = 5;        // Moving average window
static constexpr float TEMP_THRESHOLD     = 0.3f;     // Min change to trigger update

// Hardware
static constexpr bool RELAY_ACTIVE_HIGH   = true;     // Set false for active-LOW relay
```

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

# BLE Mock Firmware Suite - Mobile App Testing

Test firmware for all race car telemetry devices. Allows testing mobile apps without physical sensors.

## Overview

This suite contains 5 mock firmware projects that broadcast realistic data over BLE:

| Project | Device Type | ESP32s Needed | Purpose |
|---------|-------------|---------------|---------|
| **racescale-mock** | Corner weight scale | 4 (one per corner) | Test 4-scale simultaneous connection |
| **oil-heater-mock** | Oil heater controller | 1 | Test temperature control UI |
| **ride-height-mock** | Ultrasonic ride height | 1 | Test continuous measurement mode |
| **tire-temp-probe-mock** | 3-point tire probe | 1 | Test sequential corner workflow |
| **tire-temp-gun-mock** | IR temperature gun | 1 | Test spot/continuous modes |

## Quick Start

### 1. Install PlatformIO

```bash
# Install PlatformIO Core CLI
pip install platformio

# Or use existing installation
pio --version
```

### 2. Flash Any Mock Firmware

```bash
# Example: RaceScale
cd mock-firmware/racescale-mock
pio run --target upload
pio device monitor

# Example: Oil Heater
cd mock-firmware/oil-heater-mock
pio run --target upload
pio device monitor
```

### 3. Test with Mobile App

1. Open mobile app
2. Scan for BLE devices
3. Connect to mock device
4. Verify data appears correctly
5. Test write commands (tare, setpoint, etc.)

## UUID Reference

All UUIDs match EXACTLY what mobile apps expect after BLE protocol alignment.

### Service UUIDs
```cpp
Oil Heater:   4fafc201-1fb5-459e-8fcc-c5c9c331914b
RaceScale:    4fafc201-1fb5-459e-8fcc-c5c9c331914b  (same as heater)
Ride Height:  4fafc201-0003-459e-8fcc-c5c9c331914b
Tire Probe:   4fafc201-0004-459e-8fcc-c5c9c331914b
Tire Temp Gun: 4fafc201-0005-459e-8fcc-c5c9c331914b
```

### Characteristic UUIDs (RaceScale Example)
```cpp
Weight:       beb5483e-36e1-4688-b7f5-ea07361b26a8  // Float32LE
Tare:         beb5483e-36e1-4688-b7f5-ea07361b26ad  // Write UInt8
Battery:      beb5483e-36e1-4688-b7f5-ea07361b26ae  // UInt8
Corner ID:    beb5483e-36e1-4688-b7f5-ea07361b26af  // String
```

See individual project READMEs for complete UUID lists.

## Critical Data Format Notes

### RaceScale - Binary Floats!
Weight MUST be Float32LE binary (NOT ASCII string):
```cpp
float weight = 285.5;
pWeightChar->setValue((uint8_t*)&weight, sizeof(float));  // Correct
pWeightChar->setValue("285.5");  // WRONG!
```

### Oil Heater - ASCII Strings
Temperature sent as ASCII string:
```cpp
char tempStr[16];
snprintf(tempStr, sizeof(tempStr), "%.1f", 185.5);
pTempChar->setValue(tempStr);  // "185.5"
```

### Tire Probe - 12-byte Binary
3 tire temps as contiguous Float32LE (12 bytes total):
```cpp
float tireData[3] = {185.5, 192.3, 188.1};
pTireChar->setValue((uint8_t*)tireData, 12);
```

## Hardware Requirements

### Minimum Setup (All Projects)
- ESP32 dev board (any model: DevKit V1, NodeMCU-32S, etc.)
- USB cable for programming/power
- No sensors needed (all data is simulated)

### Multi-Scale Setup (RaceScale Only)
- **4x ESP32 dev boards** (one per corner: LF, RF, LR, RR)
- 4x USB cables or USB power bank with 4 ports
- Program each with same firmware, configure different corner IDs

## Button Functions

All projects use **BOOT button (GPIO 0)** on ESP32 dev board:

- **racescale-mock**: Tare (zero weight)
- **oil-heater-mock**: Toggle safety shutdown
- **ride-height-mock**: Single reading
- **tire-temp-probe-mock**: Trigger corner reading
- **tire-temp-gun-mock**: Take new temperature reading

## Serial Commands

All projects support serial commands at **115200 baud**:

### Common Commands
- `STATUS` - Show current state (all projects)

### Project-Specific Commands
See individual project READMEs for complete command lists.

## Testing Workflow

### Per-Device Testing
1. Flash ESP32 with mock firmware
2. Open serial monitor (115200 baud)
3. Verify device boots and shows BLE advertising
4. Open mobile app
5. Scan for BLE devices
6. Connect to mock device
7. Verify data displays correctly
8. Test button triggers
9. Test serial commands
10. Test write characteristics (if applicable)
11. Disconnect/reconnect

### Multi-Device Testing (RaceScale)
1. Flash 4 ESP32s with racescale-mock
2. Configure each via serial: `LF`, `RF`, `LR`, `RR`
3. Restart all 4 ESP32s
4. Power all 4 simultaneously
5. Open mobile app
6. Scan - should find: Scale-LF, Scale-RF, Scale-LR, Scale-RR
7. Connect to all 4 (app supports multi-connection)
8. Verify weight from each corner
9. Test tare on individual corners
10. Verify simultaneous updates

## Troubleshooting

### Device Not Found in BLE Scan
- Check serial monitor shows "BLE started: [DeviceName]"
- Verify BLE is enabled on mobile device
- Try restarting ESP32
- Move closer to ESP32 (BLE range ~10m)

### Can't Connect
- Check only one mobile app connects at a time
- Restart ESP32 if stuck in connected state
- Check serial shows "Client connected" on connection

### Wrong Data Format
- Verify mobile app expects format shown in README
- Check serial output shows data being transmitted
- Review characteristic UUIDs match exactly
- Check binary vs. ASCII string format

### Updates Not Received
- Verify characteristic has NOTIFY property
- Check connection state in serial monitor
- Confirm update interval (varies by project)

## Data Simulation Details

All projects generate realistic mock data:

### RaceScale
- Weight: 250-400 lbs per corner
- Variance: ±0.5 lbs (settling) → ±0.1 lbs (stable)
- Battery: 85% (drains slowly)
- Temperature: ~72°F (slight drift)

### Oil Heater
- Heating rate: ~2°F/second
- Cooling rate: ~0.5°F/second
- Setpoint range: 100-250°F
- Hysteresis: ±5°F

### Ride Height
- Sensor range: 100-150mm
- Variance: ±0.5mm
- Sensor delta: <5mm (realistic)
- Conversion: mm to inches

### Tire Probe
- Tire temps: 150-220°F (inside hottest)
- Brake temps: 300-600°F
- Sequential workflow: RF→LF→LR→RR
- Battery: 85% (drains slowly)

### Tire Temp Gun
- Spot readings: 150-220°F
- Ambient: 65-85°F
- Emissivity: 0.95 (configurable 0.1-1.0)
- Modes: Spot (2s) or Continuous (1s)

## Development Notes

### Libraries Used
- **NimBLE-Arduino** (v1.4.1): Lightweight BLE stack
- **ArduinoJson** (v7.0.0): JSON serialization

### Why NimBLE vs ESP32 BLE?
- Lower memory footprint
- Better multi-connection support (RaceScale needs 4 simultaneous)
- More stable on ESP32

### Adding New Mock Devices
1. Copy an existing project as template
2. Update UUIDs to match mobile app
3. Implement data simulation logic
4. Update notification intervals
5. Add serial commands for testing
6. Create comprehensive README
7. Test with mobile app

## Project Structure

```
mock-firmware/
├── README.md                    # This file
├── racescale-mock/
│   ├── platformio.ini
│   ├── src/main.cpp            # 4-corner scale (configurable)
│   └── README.md
├── oil-heater-mock/
│   ├── platformio.ini
│   ├── src/main.cpp            # Heater controller
│   └── README.md
├── ride-height-mock/
│   ├── platformio.ini
│   ├── src/main.cpp            # Dual ultrasonic sensors
│   └── README.md
├── tire-temp-probe-mock/
│   ├── platformio.ini
│   ├── src/main.cpp            # Sequential corner workflow
│   └── README.md
└── tire-temp-gun-mock/
    ├── platformio.ini
    ├── src/main.cpp            # IR temperature gun
    └── README.md
```

## Benefits of Mock Firmware

1. **No Hardware Needed**: Test apps without building physical devices
2. **Repeatable Testing**: Consistent data for regression testing
3. **Edge Case Testing**: Simulate faults, low battery, sensor errors
4. **Multi-Device Testing**: Test 4-scale setup without 4 load cells
5. **Development Speed**: Flash ESP32 in seconds vs. building hardware
6. **Protocol Validation**: Verify UUIDs and data formats match exactly

## Next Steps

1. **Flash First Mock**: Start with oil-heater-mock (simplest)
2. **Test Mobile App**: Verify connection and data display
3. **Try RaceScale**: Test multi-device setup with 4 ESP32s
4. **Explore Commands**: Use serial to trigger different states
5. **Test Edge Cases**: Simulate faults, low battery, etc.
6. **Validate Protocol**: Ensure all UUIDs and formats match

## Support

Each project has a detailed README with:
- UUIDs and data formats
- Serial commands
- Button functions
- Testing checklist
- Troubleshooting guide
- Expected behavior timeline

Check project READMEs for specific details.

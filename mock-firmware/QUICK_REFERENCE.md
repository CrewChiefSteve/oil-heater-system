# Quick Reference Card - BLE Mock Firmware

One-page cheat sheet for all 5 mock firmware projects.

## Flash & Monitor (Any Project)

```bash
cd mock-firmware/<project-name>
pio run -t upload && pio device monitor
```

## Device Names & Update Rates

| Project | BLE Name | Update Rate | Button Action |
|---------|----------|-------------|---------------|
| **racescale-mock** | Scale-LF/RF/LR/RR | 500ms | Tare weight |
| **oil-heater-mock** | OilHeater | 1000ms | Toggle fault |
| **ride-height-mock** | RideHeight | On command / 500ms | Single read |
| **tire-temp-probe-mock** | TireProbe | On trigger | Corner read |
| **tire-temp-gun-mock** | TireTempGun | 2s / 1s | New reading |

## Serial Commands (115200 baud)

### RaceScale
- `LF` / `RF` / `LR` / `RR` - Set corner (restart ESP32 after)
- `TARE` - Zero weight
- `STATUS` - Show state

### Oil Heater
- `SET <temp>` - Set setpoint (100-250°F)
- `FAULT` - Toggle safety shutdown
- `SENSOR` - Toggle sensor error
- `STATUS` - Show state

### Ride Height
- `R` - Single reading
- `C` - Continuous mode (500ms)
- `S` - Stop continuous
- `Z` - Zero sensors
- `STATUS` - Show state

### Tire Probe
- `READ` / `R` - Trigger corner reading
- `RESET` - Reset to RF corner
- `STATUS` - Show state

### Tire Gun
- `READ` / `R` - New reading
- `RESET` - Reset max/min
- `EMIT <val>` - Set emissivity (0.1-1.0)
- `F` / `C` - Set unit
- `SPOT` / `CONT` - Set mode
- `STATUS` - Show state

## Service UUIDs

```
Oil Heater:  4fafc201-1fb5-459e-8fcc-c5c9c331914b
RaceScale:   4fafc201-1fb5-459e-8fcc-c5c9c331914b (same!)
Ride Height: 4fafc201-0003-459e-8fcc-c5c9c331914b
Tire Probe:  4fafc201-0004-459e-8fcc-c5c9c331914b
Tire Gun:    4fafc201-0005-459e-8fcc-c5c9c331914b
```

## Data Formats (CRITICAL!)

| Project | Characteristic | Format | Example |
|---------|---------------|---------|---------|
| **RaceScale** | Weight | Float32LE (4 bytes) | `{0x00,0x00,0x8E,0x43}` = 285.5 |
| **RaceScale** | Battery | UInt8 (1 byte) | `0x55` = 85% |
| **RaceScale** | Corner | String | `"LF"` |
| **Oil Heater** | Temp | ASCII String | `"185.5"` |
| **Oil Heater** | Setpoint | ASCII String | `"180.0"` |
| **Oil Heater** | Status | JSON | `{"heater":true,...}` |
| **Ride Height** | Data | CSV String | `"S1:123.4,S2:125.1..."` |
| **Tire Probe** | Tire | 3× Float32LE (12 bytes) | `{I,M,O}` temps |
| **Tire Probe** | Brake | Float32LE (4 bytes) | Brake temp |
| **Tire Gun** | Temp | JSON | `{"temp":185.5,...}` |

## Typical Mock Data Ranges

```
RaceScale:    250-400 lbs (per corner)
Oil Heater:   70-250°F (heating simulation)
Ride Height:  100-150mm (typical ride height)
Tire Probe:   Tire: 150-220°F, Brake: 300-600°F
Tire Gun:     150-220°F (spot readings)
Battery:      85% (all devices, drains slowly)
```

## Troubleshooting

### Device Not Found
1. Check serial: `BLE started: [DeviceName]`
2. Restart ESP32
3. Move phone closer

### Can't Connect
1. Restart ESP32
2. Disable/enable Bluetooth on phone
3. Check only one app connecting

### Wrong Data
1. **RaceScale**: Must be Float32LE binary, NOT string
2. **Oil Heater**: Must be ASCII string, NOT binary
3. Check characteristic UUID matches

### Commands Don't Work
1. Verify 115200 baud
2. Check newline ending (NL or CR+LF)
3. Verify correct command syntax

## Multi-Scale Setup (RaceScale Only)

**One-time configuration per ESP32**:
```
1. Flash ESP32 #1 → Serial: "LF" → Restart
2. Flash ESP32 #2 → Serial: "RF" → Restart
3. Flash ESP32 #3 → Serial: "LR" → Restart
4. Flash ESP32 #4 → Serial: "RR" → Restart
```

**Testing**:
```
1. Power all 4 ESP32s
2. App scan finds: Scale-LF, Scale-RF, Scale-LR, Scale-RR
3. Connect to all 4 simultaneously
4. Verify weights: LF=285, RF=292, LR=278, RR=295
```

## Testing Order (Recommended)

1. **oil-heater-mock** ⭐ Start here (simplest)
2. **tire-temp-gun-mock** (JSON only)
3. **ride-height-mock** (CSV format)
4. **tire-temp-probe-mock** (sequential)
5. **racescale-mock** (most complex)

## Hardware Needed

**Minimum**: 1× ESP32 dev board + 1× USB cable
**Full suite**: 5× ESP32 + 5× USB cables
**Multi-scale**: 4× ESP32 + USB hub/power bank

## Key Files

- **README.md** - Overview & quick start
- **TESTING_GUIDE.md** - Step-by-step testing
- **SUMMARY.md** - Project summary
- **ARCHITECTURE.md** - System diagrams
- **QUICK_REFERENCE.md** - This file
- **Per-project README.md** - Device-specific docs

## Common PlatformIO Commands

```bash
# Build only
pio run

# Upload firmware
pio run -t upload

# Serial monitor
pio device monitor

# Upload + monitor
pio run -t upload && pio device monitor

# Clean build
pio run -t clean

# List connected devices
pio device list

# Update libraries
pio pkg update
```

## Expected Serial Output (First Boot)

### RaceScale
```
=== RaceScale BLE Mock Firmware ===
Loaded corner: LF, base weight: 285.5 lbs
BLE started: Scale-LF
Waiting for mobile app connection...
```

### Oil Heater
```
=== Oil Heater BLE Mock Firmware ===
BLE started: OilHeater
Waiting for mobile app connection...
Temp: 70.0°F | Setpoint: 180.0°F | Heater: ON | Safety: OK
```

### Ride Height
```
=== Ride Height BLE Mock Firmware ===
BLE started: RideHeight
Waiting for mobile app connection...
```

### Tire Probe
```
=== Tire Temperature Probe BLE Mock Firmware ===
Sequential corner workflow: RF -> LF -> LR -> RR
BLE started: TireProbe
Ready for corner: RF
```

### Tire Gun
```
=== Tire Temperature Gun BLE Mock Firmware ===
BLE started: TireTempGun
Waiting for mobile app connection...
Temp: 185.5°F | Amb: 72.3°F | Max: 185.5°F | Min: 185.5°F
```

## Button Reference (All use BOOT = GPIO 0)

```
┌─────────────────┬──────────────────────┐
│ Project         │ BOOT Button Action   │
├─────────────────┼──────────────────────┤
│ RaceScale       │ Tare weight to 0     │
│ Oil Heater      │ Toggle safety fault  │
│ Ride Height     │ Single reading       │
│ Tire Probe      │ Corner read+advance  │
│ Tire Gun        │ New temperature read │
└─────────────────┴──────────────────────┘
```

## Memory Requirements

- **Flash**: ~600-900 KB (15-20% of 4MB)
- **RAM**: ~100-160 KB (20-30% of 520KB)
- **Heap**: Stable, no leaks
- **Libraries**: NimBLE + ArduinoJson

## Success Indicators

✅ Serial shows BLE started
✅ Device appears in app scan
✅ Connection succeeds in <2s
✅ Data updates at expected rate
✅ Button triggers work
✅ Serial commands work
✅ Disconnect/reconnect works
✅ No crashes or reboots

## Quick Test (5 Minutes)

```bash
# 1. Flash
cd mock-firmware/oil-heater-mock
pio run -t upload

# 2. Monitor
pio device monitor

# 3. Mobile app
- Open app → Scan → Connect to "OilHeater"
- Verify temp rises from 70°F → 180°F
- Press BOOT button → Safety shutdown
- Serial: "STATUS" → Check state

# 4. Pass criteria
✅ Temp visible and updating
✅ Heater cycles properly
✅ Button toggles fault
✅ Serial commands work
```

---

**Print this page** for quick reference during testing!

**Need more detail?** See project-specific README files.

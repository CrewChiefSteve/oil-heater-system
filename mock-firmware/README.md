# CrewChiefSteve Mock Firmware — BLE Protocol v2

ESP32-C3 firmware that simulates all 5 CrewChiefSteve BLE devices for mobile app development and testing without real hardware.

## Quick Start

```bash
# Open in PlatformIO, build, and flash to ESP32-C3
pio run -t upload

# Open serial monitor
pio device monitor --baud 115200
```

Then use serial commands to switch between devices:

```
device 0   → Oil Heater (Heater_MOCK)
device 1   → RaceScale (RaceScale_LF)
device 2   → Ride Height (RH-Sensor_LF)
device 3   → Tire Probe (TireProbe_LF)
device 4   → Tire Temp Gun (TireTempGun)
corner 1   → Switch to RF (then restart: device 1)
status     → Print current simulation state
help       → Show all commands
```

## What It Does

Runs a single BLE device at a time (selectable via serial) that generates realistic fake sensor data. Your React Native mobile apps connect to it just like real hardware — same service UUIDs, characteristic UUIDs, data formats, and update rates.

**Key behaviors:**
- Oil heater temperature ramps toward setpoint with PID-like cycling
- Scale weights use damped oscillation to simulate a car being placed on the pad
- Ride height sensors have dual-sensor jitter with configurable delta
- Tire probe temps drift realistically per corner with camber effects
- Temp gun simulates scanning across tire surface
- Batteries slowly drain over time
- All WRITE characteristics are fully functional (setpoint, tare, commands, etc.)

## Device Protocol Reference

### Oil Heater (0001)

| Characteristic | UUID (26xx) | Properties | Format | Example |
|---|---|---|---|---|
| TEMPERATURE | 26a8 | READ, NOTIFY | String | `"180.5"` |
| SETPOINT | 26a9 | READ, WRITE, NOTIFY | String | `"180.0"` |
| STATUS | 26aa | READ, NOTIFY | JSON | `{"heater":true,"safetyShutdown":false,"sensorError":false}` |

**Service UUID:** `4fafc201-0001-459e-8fcc-c5c9c331914b`
**BLE Name:** `Heater_MOCK`

**Simulation:** Starts at ambient (72°F), heats toward setpoint at ~5°F/sec. Cycles on/off near setpoint. Safety shutdown above 260°F. Write to SETPOINT to change target (100–250°F range).

### RaceScale (0002)

| Characteristic | UUID (26xx) | Properties | Format | Example |
|---|---|---|---|---|
| WEIGHT | 26a8 | READ, NOTIFY | Float32LE | `350.5` (4 bytes) |
| CALIBRATION | 26aa | WRITE | Float32LE | Known weight |
| TEMPERATURE | 26ab | READ, NOTIFY | Float32LE | `75.2` (load cell °F) |
| STATUS | 26ac | READ, NOTIFY | JSON | `{"zeroed":true,"calibrated":true,"error":""}` |
| TARE | 26ad | WRITE | UInt8 | `0x01` to zero |
| BATTERY | 26ae | READ, NOTIFY | UInt8 | `85` (percent) |
| CORNER_ID | 26af | READ, WRITE, NOTIFY | UInt8 | `0`–`3` |

**Service UUID:** `4fafc201-0002-459e-8fcc-c5c9c331914b`
**BLE Name:** `RaceScale_XX` (XX = LF/RF/LR/RR)

**Simulation:** Starts empty (0 lbs), then after 5 seconds simulates placing a car — weight overshoots and settles via damped oscillation. Typical weights: LF=548, RF=532, LR=572, RR=558 lbs. Write 0x01 to TARE to zero.

### Ride Height Sensor (0003)

| Characteristic | UUID (26xx) | Properties | Format | Example |
|---|---|---|---|---|
| HEIGHT | 26a8 | READ, NOTIFY | CSV String | `"S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"` |
| CMD | 26a9 | WRITE | String | `R`, `C`, `S`, `Z` |
| STATUS | 26aa | READ, NOTIFY | JSON | `{"zeroed":false,"batteryLow":false,"sensorError":false}` |
| CORNER_ID | 26af | READ, WRITE, NOTIFY | UInt8 | `0`–`3` |

**Service UUID:** `4fafc201-0003-459e-8fcc-c5c9c331914b`
**BLE Name:** `RH-Sensor_XX` (XX = LF/RF/LR/RR)

**Commands:** `R` = single reading, `C` = continuous mode, `S` = stop, `Z` = zero calibration

**Simulation:** Dual sensors with ~1.7mm offset and ±0.2mm jitter. Starts in continuous mode (2 Hz). Write `Z` to zero. Height varies slightly per corner.

### Tire Temperature Probe (0004) — v2 JSON-Only

| Characteristic | UUID (26xx) | Properties | Format | Example |
|---|---|---|---|---|
| CORNER_READING | 26a8 | NOTIFY | JSON | `{"corner":0,"tireInside":185.2,"tireMiddle":188.5,"tireOutside":182.3,"brakeTemp":425.7}` |
| STATUS | 26aa | READ, NOTIFY | JSON | `{"battery":85,"isCharging":false,"firmware":"2.0.0"}` |
| CORNER_ID | 26af | READ, WRITE | UInt8 | `0`–`3` |

**Service UUID:** `4fafc201-0004-459e-8fcc-c5c9c331914b`
**BLE Name:** `TireProbe_XX` (XX = LF/RF/LR/RR)

**v2 Note:** Binary TIRE_DATA/BRAKE_DATA characteristics removed. Only JSON CORNER_READING is used.

**Simulation:** Three tire zones with camber-effect spread (inner hotter). Front corners run slightly hotter. Brake temps: fronts ~450°F, rears ~400°F. All temps drift slowly with ±1.5°F noise.

### Tire Temperature Gun (0005)

| Characteristic | UUID (26xx) | Properties | Format | Example |
|---|---|---|---|---|
| TEMPERATURE | 26a8 | NOTIFY | JSON | `{"temp":185.5,"amb":72.3,"max":195.2,"min":175.8,"bat":85,"mode":0}` |
| COMMAND | 26a9 | WRITE | String | `EMIT:0.95`, `UNIT:F`, `UNIT:C`, `RESET`, `LASER:ON`, `LASER:OFF` |

**Service UUID:** `4fafc201-0005-459e-8fcc-c5c9c331914b`
**BLE Name:** `TireTempGun`

**Note:** No STATUS characteristic. Battery is in the TEMPERATURE JSON.

**Simulation:** Continuous mode at 4 Hz. Temp drifts and periodically jumps to simulate scanning different tire areas. Tracks session min/max. Write `RESET` to clear min/max.

## Common UUID Reference

All characteristics use the pattern: `beb5483e-36e1-4688-b7f5-ea07361b26XX`

| XX | Typical Purpose |
|----|-----------------|
| a8 | Primary data (temp, weight, height, reading) |
| a9 | Secondary / command |
| aa | Status or calibration |
| ab | Temperature (scale) or config |
| ac | Status (scale) or extra |
| ad | Tare (scale) |
| ae | Battery (scale) |
| af | Corner ID |

## Corner IDs

| Value | Corner |
|-------|--------|
| 0 | LF (Left Front) |
| 1 | RF (Right Front) |
| 2 | LR (Left Rear) |
| 3 | RR (Right Rear) |

## Architecture

```
src/
├── main.cpp              Entry point, serial UI, BLE lifecycle
├── config.h              All UUIDs, constants, simulation defaults
├── simulator.h           SimValue, SimBattery, DampedOscillator, TempDrifter
├── mock_oil_heater.h     Oil heater simulation + BLE service
├── mock_racescale.h      RaceScale simulation + BLE service
├── mock_ride_height.h    Ride height simulation + BLE service
├── mock_tire_probe.h     Tire probe simulation + BLE service (JSON-only)
└── mock_tire_temp_gun.h  Tire temp gun simulation + BLE service
```

**Design decisions:**
- **NimBLE** instead of Bluedroid — lighter weight, better for C3's limited RAM
- **Single device at a time** — the C3 can run one device cleanly; switching tears down and rebuilds BLE
- **Header-only device modules** — simple dependency chain, no linker issues
- **Realistic simulation** — values drift smoothly, not random noise; batteries drain; weights settle with oscillation
- **Full WRITE support** — all writable characteristics respond correctly

## ESP32-C3 Notes

- Single-core RISC-V — no FreeRTOS task pinning needed
- ~280KB free heap after NimBLE + one device service
- NimBLE automatically adds BLE2902 descriptors for NOTIFY characteristics (required for iOS)
- Central/Observer roles disabled in build flags to save ~30KB RAM

## Build Configuration

Override defaults via `platformio.ini` build flags:

```ini
build_flags = 
    -D MOCK_DEFAULT_DEVICE=1    ; Start as RaceScale
    -D MOCK_DEFAULT_CORNER=2    ; Start as LR corner
```

Or switch at runtime via serial commands.

## Troubleshooting

**Mobile app can't find mock device:**
- Check serial output for "Advertising started"
- Verify the mobile app scans for the correct service UUID
- Ensure you're simulating the right device type
- Try `reset` in serial to restart advertising

**App connects but no data:**
- Make sure the app subscribes to NOTIFY characteristics
- Check serial for "Client connected" message
- NimBLE requires the app to enable notifications via CCCD descriptor

**Heap too low / crash:**
- Run `heap` in serial to check free memory
- The C3 should have ~280KB free — if much lower, something is leaking
- `reset` will teardown and rebuild cleanly

**Wrong data format:**
- This firmware follows BLE_PROTOCOL_REFERENCE.md exactly
- If the mobile app expects different formats, update the app to match the spec

## Protocol Compliance

This mock firmware is built against `BLE_PROTOCOL_REFERENCE.md` (2026-01-27) as the single source of truth. Key v2 rules enforced:

- ✅ Characteristic UUID pattern: `beb5483e-36e1-4688-b7f5-ea07361b26XX`
- ✅ BLE2902 descriptors on all NOTIFY characteristics (NimBLE auto-adds)
- ✅ STATUS is JSON on all devices that have it
- ✅ CORNER_ID is UInt8 (0=LF, 1=RF, 2=LR, 3=RR)
- ✅ No ENABLE characteristic on Oil Heater
- ✅ Tire Probe uses JSON-only (no binary TIRE_DATA/BRAKE_DATA)
- ✅ No legacy UUID (`4fafc201-1fb5-...`)

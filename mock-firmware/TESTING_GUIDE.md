# Mock Firmware Testing Guide

Quick reference for testing all BLE mock firmware projects.

## Pre-Flight Checklist

- [ ] PlatformIO installed (`pio --version`)
- [ ] ESP32 dev board connected via USB
- [ ] Mobile app installed on phone/tablet
- [ ] Bluetooth enabled on mobile device
- [ ] Serial monitor ready (115200 baud)

## Project Testing Order

Test in this order (simple → complex):

1. **oil-heater-mock** - Simplest, ASCII strings, good first test
2. **tire-temp-gun-mock** - JSON only, spot/continuous modes
3. **ride-height-mock** - CSV format, command-driven
4. **tire-temp-probe-mock** - Binary floats, sequential workflow
5. **racescale-mock** - Most complex, requires 4 ESP32s for full test

## Quick Flash Commands

```bash
# Oil Heater
cd mock-firmware/oil-heater-mock && pio run -t upload && pio device monitor

# Tire Temp Gun
cd mock-firmware/tire-temp-gun-mock && pio run -t upload && pio device monitor

# Ride Height
cd mock-firmware/ride-height-mock && pio run -t upload && pio device monitor

# Tire Probe
cd mock-firmware/tire-temp-probe-mock && pio run -t upload && pio device monitor

# RaceScale (repeat for 4 ESP32s)
cd mock-firmware/racescale-mock && pio run -t upload && pio device monitor
```

## Test Scenarios by Project

### 1. Oil Heater Mock

**Goal**: Verify temperature control UI works

**Steps**:
1. Flash ESP32
2. Open mobile app → Scan → Connect to "OilHeater"
3. Verify temp starts at ~70°F, setpoint 180°F
4. Watch temp rise (~2°F/sec)
5. Observe heater cycle around setpoint (180±5°F)
6. Change setpoint in app (e.g., 200°F)
7. Verify temp adjusts to new target
8. Press BOOT button → Safety shutdown activates
9. Verify heater turns OFF
10. Adjust setpoint → Safety clears

**Expected Serial Output**:
```
Temp: 70.0°F | Setpoint: 180.0°F | Heater: ON | Safety: OK
Temp: 72.0°F | Setpoint: 180.0°F | Heater: ON | Safety: OK
...
Temp: 182.0°F | Setpoint: 180.0°F | Heater: OFF | Safety: OK
```

**Pass Criteria**:
- Temperature rises when heater ON
- Heater cycles properly
- Setpoint change works
- Safety shutdown via button works

---

### 2. Tire Temp Gun Mock

**Goal**: Verify IR gun spot/continuous modes

**Steps**:
1. Flash ESP32
2. Connect to "TireTempGun" in app
3. Verify temp appears (150-220°F range)
4. Press BOOT button → New reading
5. Verify max/min track properly
6. Send "RESET" command via app → Max/min reset
7. Send "EMIT:0.50" → Temp should halve
8. Send "UNIT:C" → Temp converts to Celsius
9. Send "MODE:CONTINUOUS" → Updates increase to 1/sec
10. Send "MODE:SPOT" → Updates return to 2/sec

**Expected Serial Output**:
```
Temp: 185.5°F | Amb: 72.3°F | Max: 185.5°F | Min: 185.5°F | Bat: 85% | Mode: SPOT
BUTTON: New reading
Temp: 192.3°F | Amb: 72.5°F | Max: 192.3°F | Min: 185.5°F | Bat: 85% | Mode: SPOT
```

**Pass Criteria**:
- Spot readings trigger on button
- Continuous mode streams data
- Emissivity affects readings
- Unit conversion works
- Max/min tracking works

---

### 3. Ride Height Mock

**Goal**: Verify continuous sensor mode

**Steps**:
1. Flash ESP32
2. Connect to "RideHeight" in app
3. Send 'R' command → Single reading appears
4. Verify CSV format: `S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85`
5. Send 'C' command → Continuous mode starts
6. Verify readings every 500ms
7. Verify S1 and S2 within 5mm of each other
8. Send 'S' command → Continuous stops
9. Send 'Z' command → Sensors zero
10. Send 'R' → Readings resume from 0mm

**Expected Serial Output**:
```
Command received: 'R'
Sent: S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85
Command received: 'C'
Continuous mode STARTED
Sent: S1:123.6,S2:125.3,AVG:124.4,IN:4.90,BAT:3.85
```

**Pass Criteria**:
- Single readings work
- Continuous mode streams properly
- Sensors stay within 5mm delta
- Zero function works
- CSV format correct

---

### 4. Tire Temp Probe Mock

**Goal**: Verify sequential corner workflow

**Steps**:
1. Flash ESP32
2. Connect to "TireProbe" in app
3. Verify starts at RF corner
4. Press BOOT button → RF reading appears
5. Verify tire temps (I/M/O) and brake temp
6. Wait 3 seconds → Auto-advance to LF
7. Press BOOT → LF reading
8. Continue for LR, RR
9. After RR → Should loop back to RF
10. Verify battery drains slowly

**Expected Serial Output**:
```
Ready for corner: RF

BUTTON: Corner reading triggered
[RF] Tire: I=185.5°F M=192.3°F O=188.1°F | Brake: 378.5°F
Next corner in 3 seconds: LF

[LF] Tire: I=180.2°F M=187.4°F O=183.6°F | Brake: 402.1°F
Next corner in 3 seconds: LR
...
=== Session complete! Starting over at RF ===
```

**Pass Criteria**:
- Sequential workflow: RF→LF→LR→RR
- Tire temps realistic (inside hottest)
- Brake temps in range (300-600°F)
- Auto-advance works
- Session loops back to RF

---

### 5. RaceScale Mock (4-Scale Setup)

**Goal**: Verify multi-device simultaneous connection

**Setup** (One-time per ESP32):
1. Flash ESP32 #1 → Serial: `LF` → Restart
2. Flash ESP32 #2 → Serial: `RF` → Restart
3. Flash ESP32 #3 → Serial: `LR` → Restart
4. Flash ESP32 #4 → Serial: `RR` → Restart

**Testing**:
1. Power all 4 ESP32s simultaneously
2. Open app → Scan
3. Should find: Scale-LF, Scale-RF, Scale-LR, Scale-RR
4. Connect to all 4 (app must support multi-connection)
5. Verify weight from each corner:
   - LF: ~285 lbs
   - RF: ~292 lbs
   - LR: ~278 lbs
   - RR: ~295 lbs
6. Press BOOT on LF scale → Weight zeros
7. Verify only LF affected, others unchanged
8. Verify weights update every 500ms
9. Check battery, temperature for each scale
10. Disconnect one → Others stay connected

**Expected Serial Output (per ESP32)**:
```
Loaded corner: LF, base weight: 285.5 lbs
BLE started: Scale-LF
[LF] Weight: 285.23 lbs | Temp: 72.1°F | Bat: 85% | Stable: YES
[LF] Weight: 285.67 lbs | Temp: 72.1°F | Bat: 85% | Stable: YES
BUTTON TARE
[LF] Weight: 0.12 lbs | Temp: 72.1°F | Bat: 85% | Stable: NO
```

**Pass Criteria**:
- All 4 scales advertise with unique names
- App connects to all 4 simultaneously
- Weights update independently
- Tare affects only one scale
- Binary float format works (NOT strings)
- Settling animation after tare
- Each corner has realistic weight

---

## Common Issues & Solutions

### Issue: "Device not found in scan"
**Solution**:
- Check serial shows "BLE started: [DeviceName]"
- Restart ESP32
- Move phone closer to ESP32

### Issue: "Can't connect to device"
**Solution**:
- Only one app can connect at a time
- Restart ESP32 if stuck
- Check mobile Bluetooth is enabled

### Issue: "Data not updating"
**Solution**:
- Check serial shows "Client connected"
- Verify characteristic has NOTIFY property
- Check update interval for that project

### Issue: "Wrong data format in app"
**Solution**:
- **RaceScale/TireProbe**: Must be Float32LE binary, NOT strings
- **OilHeater**: Must be ASCII strings, NOT binary
- **TireTempGun**: JSON format required
- Check serial output matches README format

### Issue: "RaceScale - All corners show same weight"
**Solution**:
- Each ESP32 needs unique corner ID via serial
- Send `LF`, `RF`, `LR`, or `RR` to each
- Restart ESP32 after changing
- Verify serial shows correct base weight

### Issue: "Commands not working"
**Solution**:
- Check characteristic has WRITE property
- Verify command format (char vs string)
- Check serial monitor for "Command received" message

---

## Testing Checklist (Per Project)

Use this checklist for each mock firmware:

- [ ] PlatformIO build succeeds (`pio run`)
- [ ] Upload succeeds (`pio run -t upload`)
- [ ] Serial monitor shows boot message
- [ ] BLE advertising starts
- [ ] Device appears in app scan
- [ ] Connection succeeds
- [ ] Initial data appears in app
- [ ] Data updates at expected interval
- [ ] BOOT button triggers expected action
- [ ] Serial commands work
- [ ] Write commands from app work (if applicable)
- [ ] Disconnect/reconnect works
- [ ] Data format matches specification
- [ ] UUIDs match mobile app exactly

---

## Performance Benchmarks

Expected behavior:

| Project | Update Rate | Connection Time | Current Draw |
|---------|-------------|-----------------|--------------|
| RaceScale | 500ms | <2s | ~80mA |
| Oil Heater | 1000ms | <2s | ~80mA |
| Ride Height | On command or 500ms | <2s | ~80mA |
| Tire Probe | On trigger + status 5s | <2s | ~80mA |
| Tire Gun | 2s (spot) / 1s (cont) | <2s | ~80mA |

All projects should:
- Advertise within 1 second of boot
- Connect in under 2 seconds
- Send first notification within update interval
- Reconnect after disconnect in under 3 seconds

---

## Advanced Testing

### Stress Testing
1. Rapid connect/disconnect cycles (10x)
2. Leave connected for 1 hour
3. Send 100+ commands rapidly
4. Multi-device interference (all 5 mocks running simultaneously)

### Edge Cases
1. **Battery Drain**: Run for extended period, verify battery decreases
2. **Fault Conditions**: Trigger safety shutdowns, sensor errors
3. **Extreme Values**: Zero weights, max temps, low battery
4. **Command Errors**: Send invalid commands, verify rejection
5. **Data Bounds**: Verify temps/weights stay in realistic ranges

### Protocol Validation
1. Use BLE sniffer (nRF Connect app) to verify:
   - Correct UUIDs advertised
   - Data format matches (binary vs. string)
   - Notification intervals correct
   - MTU size appropriate

---

## Sign-Off Checklist (Complete Suite)

Before considering testing complete:

- [ ] All 5 projects build without errors
- [ ] All 5 projects upload successfully
- [ ] All 5 devices advertise correctly
- [ ] Mobile app connects to all 5 device types
- [ ] All button functions work
- [ ] All serial commands work
- [ ] All write characteristics work
- [ ] Binary formats correct (RaceScale, TireProbe)
- [ ] ASCII formats correct (OilHeater, RideHeight)
- [ ] JSON formats correct (all status characteristics)
- [ ] UUIDs verified against mobile app source
- [ ] Multi-device test passes (4x RaceScale)
- [ ] No crashes or watchdog resets
- [ ] Memory usage acceptable (<80% heap)
- [ ] All README examples verified

---

## Next Steps After Testing

1. **Document Issues**: Note any mobile app bugs found
2. **Update UUIDs**: If any mismatches found, update mock firmware
3. **Enhance Mocks**: Add more realistic edge cases
4. **Create Test Suite**: Automated tests for regression testing
5. **Production Firmware**: Use mock as reference for real sensor integration

---

## Quick Reference: Serial Commands

| Project | Commands |
|---------|----------|
| **RaceScale** | `LF/RF/LR/RR`, `TARE`, `STATUS` |
| **Oil Heater** | `SET <temp>`, `FAULT`, `SENSOR`, `STATUS` |
| **Ride Height** | `R`, `C`, `S`, `Z`, `STATUS` |
| **Tire Probe** | `READ/R`, `RESET`, `STATUS` |
| **Tire Gun** | `READ/R`, `RESET`, `EMIT <val>`, `F/C`, `SPOT/CONT`, `STATUS` |

All commands at **115200 baud**.

---

## Quick Reference: Button Functions

All projects use **BOOT button (GPIO 0)**:

- **RaceScale**: Tare weight to 0
- **Oil Heater**: Toggle safety shutdown
- **Ride Height**: Single reading
- **Tire Probe**: Trigger corner reading + advance
- **Tire Gun**: New temperature reading

---

## Support

For issues or questions:
1. Check individual project README
2. Review serial monitor output
3. Verify UUIDs match mobile app
4. Check data format (binary vs. ASCII)
5. Test with nRF Connect app first

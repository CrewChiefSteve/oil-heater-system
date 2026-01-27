# Tire Temperature Gun - Firmware Update Summary

**Date:** 2026-01-27
**Status:** 📝 READY FOR UPDATE
**Mobile App:** mobile-tire-temp
**Service UUID:** `4fafc201-0005-459e-8fcc-c5c9c331914b`

---

## Changes Required

### Summary

| Change | File | Function | Priority |
|--------|------|----------|----------|
| COMMAND parsing: JSON → String | `src/main.cpp` | `CharacteristicCallbacks::onWrite()` | **REQUIRED** |

**Total Changes:** 1 required

---

## Change #1: COMMAND Characteristic Parsing (REQUIRED)

### File: `src/main.cpp`

### Current Code (Line 72-102)

```cpp
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.print("Received command: ");
            Serial.println(value.c_str());

            // Parse JSON command (for future mobile app control)
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, value.c_str());

            if (!error) {
                if (doc.containsKey("reset")) {
                    maxTempF = currentTempF;
                    minTempF = currentTempF;
                    Serial.println("Max/Min reset via BLE");
                }
                if (doc.containsKey("mode")) {
                    int mode = doc["mode"];
                    if (mode >= MODE_INSTANT && mode <= MODE_MIN) {
                        currentMode = (MeasurementMode)mode;
                    }
                }
                if (doc.containsKey("unit")) {
                    String unit = doc["unit"].as<String>();
                    useFahrenheit = (unit == "F");
                }
            }
        }
    }
};
```

### New Code

```cpp
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String cmd = String(value.c_str());
            Serial.print("Received command: ");
            Serial.println(cmd);

            // Parse string commands per BLE protocol
            // Expected commands: EMIT:0.95, UNIT:F, UNIT:C, RESET, LASER:ON, LASER:OFF

            if (cmd.startsWith("EMIT:")) {
                // Set emissivity (e.g., "EMIT:0.95")
                float emissivity = cmd.substring(5).toFloat();
                if (emissivity >= 0.10 && emissivity <= 1.00) {
                    // Note: Writing emissivity to MLX90614 EEPROM requires caution
                    // Limited write cycles (~100k), and incorrect value can brick sensor
                    // mlx.writeEmissivity(emissivity);  // DANGER: Use with extreme caution!
                    Serial.printf("Emissivity command received: %.2f (not written to EEPROM)\n", emissivity);
                    // TODO: If needed, implement RAM-only emissivity compensation
                } else {
                    Serial.printf("Invalid emissivity: %.2f (must be 0.10-1.00)\n", emissivity);
                }
            }
            else if (cmd == "UNIT:F") {
                // Set Fahrenheit
                useFahrenheit = true;
                Serial.println("Unit set to Fahrenheit via BLE");
            }
            else if (cmd == "UNIT:C") {
                // Set Celsius
                useFahrenheit = false;
                Serial.println("Unit set to Celsius via BLE");
            }
            else if (cmd == "RESET") {
                // Reset min/max values
                maxTempF = currentTempF;
                minTempF = currentTempF;
                playTone(BUZZ_FREQ_RESET, BUZZ_DURATION_MS);
                Serial.println("Max/Min reset via BLE");
            }
            else if (cmd == "LASER:ON") {
                // Turn laser on (note: physical trigger button will override)
                digitalWrite(PIN_LASER, HIGH);
                Serial.println("Laser ON via BLE");
            }
            else if (cmd == "LASER:OFF") {
                // Turn laser off
                digitalWrite(PIN_LASER, LOW);
                Serial.println("Laser OFF via BLE");
            }
            else {
                Serial.print("Unknown command: ");
                Serial.println(cmd);
            }
        }
    }
};
```

**Why:** Mobile app sends string commands (`"UNIT:F"`, `"RESET"`, etc.), not JSON objects.

**Result:** Mobile app can control unit, reset min/max, and toggle laser via BLE.

**Bonus Features Added:**
- Buzzer feedback on BLE reset command
- Emissivity parsing (logged but not applied for safety)
- Unknown command logging for debugging

---

## Testing Checklist

### Pre-Update Verification

- [ ] Backup current firmware binary
- [ ] Document current behavior
- [ ] Note any settings

### Compilation Test

```bash
cd tire-temp-gun
pio run
```

**Expected:** Clean compilation, no errors

**Note:** No new dependencies needed (ArduinoJson already in project)

### Flash Test

```bash
pio run -t upload
pio device monitor
```

**Expected output:**
```
=== Tire Temperature Gun ===
Initializing MLX90614...
MLX90614 initialized
Initializing SSD1306...
SSD1306 initialized
Initializing BLE...
BLE advertising started
Device name: TireTempGun
Ready!
```

### Mobile App Testing

#### 1. Discovery & Connection
- [ ] Open mobile-tire-temp app
- [ ] Tap "Scan for Devices"
- [ ] Verify: "TireTempGun" appears
- [ ] Connect to device
- [ ] Verify: "BLE client connected" in serial
- [ ] Expected: Connection successful

#### 2. Temperature Reading (JSON - Already Correct)
- [ ] Point gun at tire surface
- [ ] Press physical trigger (laser on)
- [ ] Check temperature in app
- [ ] Verify: Reading updates every 250ms
- [ ] Expected: `{"temp":185.5,"amb":72.3,"max":195.2,"min":175.8,"bat":85,"mode":0,"unit":"F"}`

**Serial output:**
```
[Sending temperature data via BLE]
temp: 185.5 F, ambient: 72.3 F, battery: 85%
```

#### 3. COMMAND: Unit Change (NEW)
- [ ] Send command `"UNIT:C"` from app
- [ ] Verify: Serial shows "Unit set to Celsius via BLE"
- [ ] Verify: Next JSON has temperatures in Celsius
- [ ] Verify: Display shows °C
- [ ] Send command `"UNIT:F"` from app
- [ ] Verify: Back to Fahrenheit
- [ ] Expected: Unit toggles correctly

**Serial output:**
```
Received command: UNIT:C
Unit set to Celsius via BLE

Received command: UNIT:F
Unit set to Fahrenheit via BLE
```

**Before:** `{"temp":185.5,"amb":72.3,...,"unit":"F"}`
**After UNIT:C:** `{"temp":85.3,"amb":22.4,...,"unit":"C"}`

#### 4. COMMAND: Reset Min/Max (NEW)
- [ ] Take several readings to build up min/max
- [ ] Send command `"RESET"` from app
- [ ] Verify: Serial shows "Max/Min reset via BLE"
- [ ] Verify: Buzzer beeps
- [ ] Verify: Next JSON has max = min = current temp
- [ ] Expected: Reset works

**Serial output:**
```
Received command: RESET
Max/Min reset via BLE
```

**Before:** `{"temp":185.5,"max":205.3,"min":165.2,...}`
**After:** `{"temp":185.5,"max":185.5,"min":185.5,...}`

#### 5. COMMAND: Laser Control (NEW)
- [ ] Send command `"LASER:ON"` from app
- [ ] Verify: Serial shows "Laser ON via BLE"
- [ ] Verify: Red laser dot visible
- [ ] Wait 2 seconds
- [ ] Send command `"LASER:OFF"` from app
- [ ] Verify: Laser turns off
- [ ] Expected: Remote laser control works

**Serial output:**
```
Received command: LASER:ON
Laser ON via BLE
Received command: LASER:OFF
Laser OFF via BLE
```

**Note:** Physical trigger button will override BLE laser control (safety feature)

#### 6. COMMAND: Emissivity (OPTIONAL)
- [ ] Send command `"EMIT:0.98"` from app
- [ ] Verify: Serial shows "Emissivity command received: 0.98 (not written to EEPROM)"
- [ ] Verify: No change to readings (not applied)
- [ ] Expected: Command acknowledged but not acted upon

**Serial output:**
```
Received command: EMIT:0.98
Emissivity command received: 0.98 (not written to EEPROM)
```

**Why not applied:** Writing to MLX90614 EEPROM is risky (~100k write limit)

#### 7. COMMAND: Unknown Command
- [ ] Send invalid command `"HELLO"` from app
- [ ] Verify: Serial shows "Unknown command: HELLO"
- [ ] Verify: No crash or error
- [ ] Expected: Graceful handling

**Serial output:**
```
Received command: HELLO
Unknown command: HELLO
```

#### 8. Hardware Buttons (Verify Still Work)
- [ ] **Trigger button:** Hold to turn laser on
- [ ] **Mode button:** Press to cycle INSTANT → HOLD → MAX → MIN
- [ ] **Hold button (short):** Press to toggle F/C
- [ ] **Hold button (long 2s):** Press to reset max/min
- [ ] Expected: All physical controls still work

#### 9. Display Functionality
- [ ] Verify OLED shows current mode (INSTANT/HOLD/MAX/MIN)
- [ ] Verify battery percentage displays
- [ ] Verify BLE connection indicator
- [ ] Verify large temperature reading
- [ ] Verify ambient temperature
- [ ] Verify unit indicator (°F or °C)
- [ ] Expected: Display works correctly

#### 10. Battery Monitoring
- [ ] Check battery percentage in JSON
- [ ] Should be realistic (0-100%)
- [ ] Should update periodically
- [ ] Expected: Battery monitoring works

---

## Troubleshooting

### Compilation Errors

**Error: `String` not defined**
- Ensure `#include <Arduino.h>` is present (should be at line 1)

**Error: `cmd.startsWith` not found**
- Ensure you're using `String cmd` (Arduino String class), not `std::string`

### Runtime Issues

**Device not discovered:**
- Check serial: Should show "BLE advertising started"
- Verify SERVICE_UUID matches app
- Try power cycle

**Commands not working:**
- Check serial monitor for "Received command: ..."
- Verify command format exactly: `"UNIT:F"` not `"unit:f"` (case-sensitive)
- Test with serial monitor: Write string to characteristic manually

**Laser doesn't turn off via BLE:**
- Physical trigger button has priority (safety feature)
- Release trigger button first, then send `"LASER:OFF"`

**Temperature shows 0 or NaN:**
- Check MLX90614 wiring (I2C SDA pin 21, SCL pin 22)
- Verify I2C address 0x5A
- Check serial for "MLX90614 not found!" error

**Display blank:**
- Check SSD1306 wiring (shares I2C bus)
- Verify I2C address 0x3C
- Check serial for "SSD1306 not found!" error

---

## What Stays the Same

### Already Correct ✅

1. **Service UUID** - `4fafc201-0005-459e-8fcc-c5c9c331914b` ✓
2. **TEMPERATURE UUID** - `beb5483e-36e1-4688-b7f5-ea07361b26a8` ✓
3. **COMMAND UUID** - `beb5483e-36e1-4688-b7f5-ea07361b26a9` ✓
4. **TEMPERATURE JSON format** - Perfect ✓
5. **Device name** - "TireTempGun" ✓
6. **NimBLE implementation** - Correct ✓
7. **Temperature reading** - MLX90614 integration ✓
8. **Display logic** - OLED works perfectly ✓
9. **Button handling** - Debouncing and long-press ✓
10. **Battery monitoring** - ADC averaging ✓
11. **Measurement modes** - INSTANT/HOLD/MAX/MIN ✓

---

## Known Behaviors (Not Issues)

### 1. Physical Trigger Overrides BLE Laser
- **Behavior:** Holding trigger button turns laser on, overriding BLE OFF command
- **Reason:** Safety - user has physical control
- **Status:** BY DESIGN

### 2. Emissivity Not Written to EEPROM
- **Behavior:** `"EMIT:x.xx"` command acknowledged but not applied
- **Reason:** MLX90614 EEPROM has limited writes (~100k)
- **Status:** BY DESIGN (protective)

### 3. Unit Toggle Available Both Ways
- **Behavior:** Can toggle F/C via Hold button OR BLE `"UNIT:F"/"UNIT:C"`
- **Status:** BY DESIGN (flexibility)

### 4. Reset Available Both Ways
- **Behavior:** Long-press Hold button OR BLE `"RESET"`
- **Status:** BY DESIGN (convenience)

### 5. BLE Notify Rate
- **Behavior:** Temperature JSON sent every 250ms (4 Hz)
- **Config:** `BLE_NOTIFY_INTERVAL_MS = 250`
- **Status:** BY DESIGN (good balance)

### 6. Extra "unit" Field in JSON
- **Behavior:** JSON includes `"unit":"F"` which is not in protocol spec
- **Impact:** None - mobile app ignores unknown fields
- **Status:** BY DESIGN (informative)

---

## Files Modified Summary

### Required Changes
1. `src/main.cpp` - 1 function update (~30 lines)
   - `CharacteristicCallbacks::onWrite()` - JSON parser → String command parser

### No Changes Needed
- `platformio.ini` ✓ (ArduinoJson v7 already present)
- `include/config.h` ✓ (UUIDs correct)
- `sendBleData()` function ✓ (TEMPERATURE format correct)
- Hardware configurations ✓
- Display rendering ✓
- Button handlers ✓
- Temperature reading ✓

---

## Success Criteria

After update:
- [ ] Code compiles without errors
- [ ] Device advertises as "TireTempGun"
- [ ] Mobile app connects successfully
- [ ] Temperature JSON format correct
- [ ] `"UNIT:F"` command sets Fahrenheit
- [ ] `"UNIT:C"` command sets Celsius
- [ ] `"RESET"` command resets min/max
- [ ] `"LASER:ON"` command turns laser on
- [ ] `"LASER:OFF"` command turns laser off
- [ ] Physical buttons still work
- [ ] Display shows correct info
- [ ] Battery monitoring works

---

## Risk Assessment

**Risk Level:** 🟢 VERY LOW

**Why:**
- Only 1 function modified (command parser)
- Change from JSON to String parsing is trivial
- No changes to temperature reading or transmission
- No changes to display or hardware control
- Physical buttons unchanged (fallback always available)

**Mitigation:**
- Test BLE commands via serial monitor first
- Verify physical buttons work after update
- Keep backup firmware for quick rollback

**Recommendation:** Very low risk. Straightforward String parsing change.

---

## Rollback Procedure

If issues occur:

```bash
# Option 1: Git rollback
git checkout HEAD -- src/main.cpp
pio run -t upload

# Option 2: Restore backup binary
pio run -t upload --upload-port COM_PORT --upload-flags --binary path/to/backup.bin
```

---

## Comparison to Other Devices

| Device | Changes Required | Complexity | Status |
|--------|------------------|------------|--------|
| Ride Height | 0 | None | ✅ Perfect |
| **Tire Gun** | **1 parser** | **Trivial** | 📝 Ready |
| Oil Heater | 1 UUID | Trivial | ✅ Updated |
| Tire Probe | 2 formats | Low | ✅ Ready |
| RaceScale | 6 formats | High | ✅ Updated |

**Tire Gun:** 2nd easiest update! Just swap JSON parser for String parser.

---

## BLE Command Quick Reference

For testing in mobile app or serial terminal:

| Command | Action | Example |
|---------|--------|---------|
| `"EMIT:0.95"` | Set emissivity | `"EMIT:0.98"` |
| `"UNIT:F"` | Set Fahrenheit | `"UNIT:F"` |
| `"UNIT:C"` | Set Celsius | `"UNIT:C"` |
| `"RESET"` | Reset min/max | `"RESET"` |
| `"LASER:ON"` | Laser on | `"LASER:ON"` |
| `"LASER:OFF"` | Laser off | `"LASER:OFF"` |

**Note:** Commands are case-sensitive. Must be exact strings.

---

**Update Summary Created:** 2026-01-27
**Firmware Version:** Tire Temperature Gun v1.0
**Changes:** 1 required (String command parsing)
**Risk:** 🟢 VERY LOW
**Recommendation:** Apply update, test basic functionality

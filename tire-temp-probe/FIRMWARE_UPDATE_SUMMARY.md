# Tire Temperature Probe - Firmware Update Summary

**Date:** 2026-01-27
**Status:** 📝 READY FOR UPDATES
**Mobile App:** mobile-tire-probe
**Service UUID:** `4fafc201-0004-459e-8fcc-c5c9c331914b`

---

## Changes Required

### Summary

| Change | File | Function | Priority |
|--------|------|----------|----------|
| SYSTEM_STATUS: Binary → JSON | `src/ble_service.cpp` | `bleTransmitSystemStatus()` | **REQUIRED** |
| Temperatures: Celsius → Fahrenheit | `src/ble_service.cpp` | `bleTransmitCornerReading()` | **REQUIRED** |
| Add DEVICE_CONFIG characteristic | `src/ble_service.cpp` + `include/ble_protocol.h` | `bleInit()` | OPTIONAL |

**Total Changes:** 2 required, 1 optional

---

## Change #1: SYSTEM_STATUS Format (REQUIRED)

### File: `src/ble_service.cpp`

### Current Code (Line 100-115)

```cpp
void bleTransmitSystemStatus(const SystemStatus& status, uint8_t capturedCount) {
    if (!deviceConnected || systemStatusChar == nullptr) {
        return;
    }

    // Build binary packet (8 bytes)
    uint8_t packet[8];
    packet[0] = (uint8_t)status.state;
    packet[1] = status.batteryPercent;
    memcpy(&packet[2], &status.batteryVoltage, sizeof(float));  // 4 bytes
    packet[6] = status.charging ? 1 : 0;
    packet[7] = capturedCount;

    systemStatusChar->setValue(packet, 8);
    systemStatusChar->notify();
}
```

### New Code

```cpp
void bleTransmitSystemStatus(const SystemStatus& status, uint8_t capturedCount) {
    if (!deviceConnected || systemStatusChar == nullptr) {
        return;
    }

    // Build JSON document
    // Mobile app expects: { battery: number, isCharging: boolean, firmware: string }
    StaticJsonDocument<128> doc;
    doc["battery"] = status.batteryPercent;
    doc["isCharging"] = status.charging;
    doc["firmware"] = DEVICE_MODEL;  // "TTP-4CH-v1" from config.h

    // Serialize to string
    char jsonBuffer[128];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    systemStatusChar->setValue((uint8_t*)jsonBuffer, len);
    systemStatusChar->notify();

    Serial.printf("[BLE] TX Status: Bat:%d%% Charging:%s FW:%s\n",
                  status.batteryPercent,
                  status.charging ? "Yes" : "No",
                  DEVICE_MODEL);
}
```

**Why:** Mobile app expects JSON format, not binary. This matches the protocol standard.

**Result:** Mobile app can parse system status correctly.

---

## Change #2: Temperature Units (REQUIRED)

### File: `src/ble_service.cpp`

### Current Code (Line 70-98)

```cpp
void bleTransmitCornerReading(const CornerReading& reading) {
    if (!deviceConnected || cornerReadingChar == nullptr) {
        return;
    }

    // Build JSON document
    StaticJsonDocument<200> doc;

    // Corner name mapping
    const char* cornerNames[] = {"RF", "LF", "LR", "RR"};
    doc["corner"] = cornerNames[reading.corner];
    doc["tireInside"] = reading.tireInside;    // ← Celsius
    doc["tireMiddle"] = reading.tireMiddle;    // ← Celsius
    doc["tireOutside"] = reading.tireOutside;  // ← Celsius
    doc["brakeTemp"] = reading.brakeTemp;      // ← Celsius

    // Serialize to string
    char jsonBuffer[200];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    cornerReadingChar->setValue((uint8_t*)jsonBuffer, len);
    cornerReadingChar->notify();

    Serial.printf("[BLE] TX Corner: %s | In:%.1f Mid:%.1f Out:%.1f Brake:%.1f\n",
                  cornerNames[reading.corner],
                  reading.tireInside, reading.tireMiddle,
                  reading.tireOutside, reading.brakeTemp);
}
```

### New Code

```cpp
void bleTransmitCornerReading(const CornerReading& reading) {
    if (!deviceConnected || cornerReadingChar == nullptr) {
        return;
    }

    // Build JSON document
    StaticJsonDocument<200> doc;

    // Corner name mapping
    const char* cornerNames[] = {"RF", "LF", "LR", "RR"};
    doc["corner"] = cornerNames[reading.corner];

    // Convert Celsius to Fahrenheit (mobile app expects Fahrenheit)
    float tireInsideF = (reading.tireInside * 9.0f / 5.0f) + 32.0f;
    float tireMiddleF = (reading.tireMiddle * 9.0f / 5.0f) + 32.0f;
    float tireOutsideF = (reading.tireOutside * 9.0f / 5.0f) + 32.0f;
    float brakeTempF = (reading.brakeTemp * 9.0f / 5.0f) + 32.0f;

    doc["tireInside"] = tireInsideF;
    doc["tireMiddle"] = tireMiddleF;
    doc["tireOutside"] = tireOutsideF;
    doc["brakeTemp"] = brakeTempF;

    // Serialize to string
    char jsonBuffer[200];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    cornerReadingChar->setValue((uint8_t*)jsonBuffer, len);
    cornerReadingChar->notify();

    Serial.printf("[BLE] TX Corner: %s | In:%.1fF Mid:%.1fF Out:%.1fF Brake:%.1fF\n",
                  cornerNames[reading.corner],
                  tireInsideF, tireMiddleF,
                  tireOutsideF, brakeTempF);
}
```

**Why:** Mobile app protocol expects temperatures in Fahrenheit, not Celsius.

**Result:** Mobile app displays correct temperature values.

---

## Change #3: Add DEVICE_CONFIG Characteristic (OPTIONAL)

### File: `include/ble_protocol.h`

### Add After Line 24

```cpp
// Device configuration (corner assignment)
#define DEVICE_CONFIG_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
```

### File: `src/ble_service.cpp`

### Add Static Variable (After Line 10)

```cpp
static NimBLECharacteristic* deviceConfigChar = nullptr;
static uint8_t currentCorner = 1;  // 0=LF, 1=RF, 2=LR, 3=RR (default RF)
```

### Add Callback Class (After ServerCallbacks class, ~Line 26)

```cpp
// Device config callbacks for corner assignment
class DeviceConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            uint8_t corner = (uint8_t)value[0];
            if (corner <= 3) {  // Valid range: 0-3
                currentCorner = corner;
                const char* cornerNames[] = {"LF", "RF", "LR", "RR"};
                Serial.printf("[BLE] Corner assignment changed to: %s (%d)\n",
                             cornerNames[corner], corner);
                // TODO: Save to NVS for persistence
            }
        }
    }
};
```

### Modify `bleInit()` Function (Add Before `pService->start()` at Line 51)

```cpp
    // Device config characteristic (read + write)
    deviceConfigChar = pService->createCharacteristic(
        DEVICE_CONFIG_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    deviceConfigChar->setCallbacks(new DeviceConfigCallbacks());

    // Set initial value (0=LF, 1=RF, 2=LR, 3=RR)
    uint8_t cornerID = currentCorner;
    deviceConfigChar->setValue(&cornerID, 1);
```

**Why:** Allows mobile app to read/write corner assignment for multi-device setups.

**Priority:** OPTIONAL (firmware already uses sequential capture, doesn't strictly need this)

---

## Testing Checklist

### Pre-Update Verification

- [ ] Backup current firmware binary
- [ ] Document current behavior
- [ ] Note any custom settings

### Compilation Test

```bash
cd tire-temp-probe
pio run
```

**Expected:** Clean compilation, no errors

**If errors occur:**
- Check that `DEVICE_MODEL` is defined in `config.h`
- Verify ArduinoJson is in `platformio.ini` lib_deps (should be there)

### Flash Test

```bash
pio run -t upload
pio device monitor
```

**Expected output:**
```
[BLE] Initializing...
[BLE] Service initialized
[BLE] Advertising started
[Probes] 4x MAX31855K initialized
[Display] OLED ready
```

### Mobile App Testing

#### 1. Discovery
- [ ] Open mobile-tire-probe app
- [ ] Tap "Scan for Devices"
- [ ] Verify: "TireProbe" appears
- [ ] Expected: Device discovered successfully

#### 2. Connection
- [ ] Tap device to connect
- [ ] Verify: "Client connected" in serial monitor
- [ ] Verify: Connection established in app
- [ ] Expected: No errors

#### 3. System Status (NEW JSON FORMAT)
- [ ] Check status display in app
- [ ] Verify: Battery percentage shows (e.g., 85%)
- [ ] Verify: Charging status shows (true/false)
- [ ] Verify: Firmware version shows "TTP-4CH-v1"
- [ ] Expected: JSON `{"battery":85,"isCharging":false,"firmware":"TTP-4CH-v1"}`

**Serial output:**
```
[BLE] TX Status: Bat:85% Charging:No FW:TTP-4CH-v1
```

#### 4. Corner Reading - Right Front (NEW FAHRENHEIT)
- [ ] Place probes on RF tire (inside, middle, outside)
- [ ] Place brake probe on RF rotor
- [ ] Wait for temperature stability (~1 second)
- [ ] Verify: CORNER_READING notification received
- [ ] Verify: Corner shows "RF"
- [ ] Verify: Temperatures in Fahrenheit range (e.g., 150-250°F for tire, 300-500°F for brake)
- [ ] Expected: `{"corner":"RF","tireInside":185.0,"tireMiddle":188.5,"tireOutside":182.3,"brakeTemp":356.5}`

**Serial output:**
```
[BLE] TX Corner: RF | In:185.0F Mid:188.5F Out:182.3F Brake:356.5F
```

**Before (Celsius):** 85°C, 87°C, 84°C, 180°C
**After (Fahrenheit):** 185°F, 188.6°F, 183.2°F, 356°F

#### 5. Sequential Capture (All 4 Corners)
- [ ] Capture RF (Right Front) - verify transmission
- [ ] Device advances to LF state
- [ ] Capture LF (Left Front) - verify transmission
- [ ] Device advances to LR state
- [ ] Capture LR (Left Rear) - verify transmission
- [ ] Device advances to RR state
- [ ] Capture RR (Right Rear) - verify transmission
- [ ] Device shows "Session Complete"
- [ ] Expected: All 4 corners in mobile app

#### 6. Temperature Accuracy Validation
- [ ] Compare tire inside/middle/outside spread
- [ ] Typical tire temps: 150-250°F (66-121°C)
- [ ] Typical brake temps: 300-600°F (149-316°C)
- [ ] Verify: No negative values
- [ ] Verify: No unreasonable spikes
- [ ] Expected: Data makes sense

#### 7. Battery Monitoring
- [ ] Check battery percentage in app
- [ ] Should update every 2 seconds (STATUS_TX_INTERVAL_MS)
- [ ] Connect USB charger
- [ ] Verify: `isCharging` becomes `true` in app
- [ ] Disconnect charger
- [ ] Verify: `isCharging` becomes `false`
- [ ] Expected: Real-time battery updates

#### 8. Display & LED Validation
- [ ] OLED shows current corner
- [ ] OLED shows temperatures
- [ ] LED blinks during measurement (active)
- [ ] LED shows green on capture
- [ ] Expected: Visual feedback works

---

## Troubleshooting

### Compilation Errors

**Error: `DEVICE_MODEL` not defined**
- Verify `config.h` has `#define DEVICE_MODEL "TTP-4CH-v1"`
- Check include path

**Error: `serializeJson` not found**
- Verify ArduinoJson in `platformio.ini`: `bblanchon/ArduinoJson@^6.21.0`
- Run `pio pkg install`

### Runtime Issues

**Device not discovered:**
- Check serial: Should show "Advertising started"
- Verify SERVICE_UUID matches app
- Try power cycle

**Mobile app shows "Invalid JSON":**
- Check serial output for `[BLE] TX Status:` and `[BLE] TX Corner:`
- Verify JSON format in serial monitor
- Use online JSON validator

**Temperatures show as 0 or NaN:**
- Check MAX31855K wiring
- Verify thermocouple connections
- Check serial for probe errors

**Wrong temperature values:**
- Celsius showing instead of Fahrenheit → Update not applied
- Should see values like 185°F, not 85°C
- Check serial monitor for unit suffix (F vs C)

**Battery always 0%:**
- Check PIN_BATTERY_ADC wiring
- Verify voltage divider circuit
- Check ADC readings in serial

---

## What Stays the Same

### Already Correct ✅

1. **Service UUID** - `4fafc201-0004-459e-8fcc-c5c9c331914b` ✓
2. **CORNER_READING UUID** - `beb5483e-36e1-4688-b7f5-ea07361b26ac` ✓
3. **SYSTEM_STATUS UUID** - `beb5483e-36e1-4688-b7f5-ea07361b26aa` ✓
4. **JSON field names** - corner, tireInside, tireMiddle, tireOutside, brakeTemp ✓
5. **Corner names** - "RF", "LF", "LR", "RR" ✓
6. **ArduinoJson usage** - Already using library correctly ✓
7. **NimBLE stack** - Already using NimBLE correctly ✓
8. **Sequential capture logic** - State machine works perfectly ✓
9. **Temperature smoothing** - 8-sample average works well ✓
10. **Stability detection** - Works correctly ✓

---

## Known Behaviors (Not Issues)

### 1. Sequential Capture Workflow
- **Behavior:** Device progresses through RF → LF → LR → RR automatically
- **Reason:** Design decision for consistent workflow
- **Status:** BY DESIGN

### 2. Stability Wait Time
- **Behavior:** Must wait ~1 second for temperature stability before capture
- **Reason:** `STABILITY_DURATION_MS = 1000` ensures accurate readings
- **Status:** BY DESIGN

### 3. Device Name
- **Behavior:** All devices advertise as "TireProbe"
- **Status:** OK for now, but should add MAC suffix for 4-device setup
- **Future:** "TireProbe_EEFF" for uniqueness

### 4. Temperature Smoothing Delay
- **Behavior:** Takes ~800ms to get stable reading (8 samples × 100ms)
- **Reason:** `TEMP_SMOOTHING_SAMPLES = 8` reduces noise
- **Status:** BY DESIGN (good for accuracy)

### 5. Display Updates
- **Behavior:** OLED shows Celsius internally
- **Note:** After update, BLE sends Fahrenheit but display may still show Celsius
- **Future:** Update display to show Fahrenheit if needed

---

## Files Modified Summary

### Required Changes
1. `src/ble_service.cpp` - 2 function updates
   - `bleTransmitSystemStatus()` - Binary → JSON
   - `bleTransmitCornerReading()` - Add C→F conversion

### Optional Changes
2. `include/ble_protocol.h` - Add DEVICE_CONFIG_UUID
3. `src/ble_service.cpp` - Add deviceConfigChar initialization

### No Changes Needed
- `platformio.ini` ✓ (ArduinoJson already present)
- `include/config.h` ✓ (all constants correct)
- `include/types.h` ✓ (data structures correct)
- `src/probes.cpp` ✓ (thermocouple reading correct)
- `src/main.cpp` ✓ (workflow correct)

---

## Success Criteria

After update:
- [ ] Code compiles without errors
- [ ] Device advertises as "TireProbe"
- [ ] Mobile app connects successfully
- [ ] System status shows JSON format
- [ ] Battery percentage displays
- [ ] Charging state displays
- [ ] Firmware version displays
- [ ] Temperatures show in Fahrenheit (150-250°F range for tires)
- [ ] Corner readings transmit correctly
- [ ] All 4 corners can be captured
- [ ] Sequential workflow functions
- [ ] LED and display work

---

## Risk Assessment

**Risk Level:** 🟡 LOW-MEDIUM

**Why:**
- Only 2 function modifications
- ArduinoJson already in use
- Temperature conversion is standard formula
- JSON simpler than binary

**Mitigation:**
- Test on ONE device first before updating all 4
- Keep backup firmware binary
- Serial monitor for debugging

---

## Deployment Plan

### Phase 1: Single Device Test
1. Update firmware on ONE tire probe
2. Test all functionality
3. Verify mobile app compatibility
4. Run through full 4-corner capture

### Phase 2: Full Deployment (After Phase 1 Success)
5. Update remaining 3 tire probes
6. Label each device (RF, LF, LR, RR)
7. Test all 4 devices simultaneously
8. Verify no interference or conflicts

### Phase 3: Production Use
9. Document any issues
10. Monitor for stability
11. Gather user feedback

---

## Rollback Procedure

If issues occur:

```bash
# Option 1: Restore backup binary
pio run -t upload --upload-port COM_PORT --upload-flags --binary path/to/backup.bin

# Option 2: Revert code changes
git checkout HEAD -- src/ble_service.cpp
pio run -t upload
```

---

## Comparison to Other Devices

| Device | Changes Required | Complexity | Status |
|--------|------------------|------------|--------|
| Ride Height | 0 | None | ✅ Perfect |
| Oil Heater | 1 UUID | Trivial | ✅ Updated |
| **Tire Probe** | **2 formats** | **Low** | 📝 Ready |
| RaceScale | 6 formats | High | ✅ Updated |

**Tire Probe:** 3rd easiest update, 2 straightforward format conversions

---

**Update Summary Created:** 2026-01-27
**Firmware Version:** TTP-4CH-v1
**Changes:** 2 required (JSON + Fahrenheit), 1 optional (DEVICE_CONFIG)
**Risk:** 🟡 LOW-MEDIUM
**Recommendation:** Apply updates and test on one device first

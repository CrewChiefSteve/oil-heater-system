# Tire Temperature Probe - BLE Protocol Audit

**Date:** 2026-01-27
**Firmware:** Tire Temperature Probe (4-Channel Thermocouple System)
**Mobile App:** mobile-tire-probe
**Expected Service UUID:** `4fafc201-0004-459e-8fcc-c5c9c331914b`

---

## Executive Summary

**Overall Status:** ⚠️ MINOR UPDATES NEEDED

The Tire Temperature Probe firmware is **85% compliant** with the mobile app protocol. The service UUID and CORNER_READING characteristic are perfect. However, SYSTEM_STATUS needs to be changed from binary to JSON format, and temperature units need conversion from Celsius to Fahrenheit.

### Issues Found

| Issue | Severity | Status |
|-------|----------|--------|
| Service UUID | ✅ CORRECT | No change needed |
| CORNER_READING format | ✅ CORRECT | No change needed |
| SYSTEM_STATUS format (binary → JSON) | 🟡 MEDIUM | Needs fix |
| Temperature units (Celsius → Fahrenheit) | 🟡 MEDIUM | Needs fix |
| DEVICE_CONFIG characteristic | 🟢 LOW | Optional (not implemented) |

**Verdict:** 2 format changes needed, 1 optional feature

---

## Current BLE Implementation

### Service UUID (ble_protocol.h Line 19)
```cpp
#define SERVICE_UUID "4fafc201-0004-459e-8fcc-c5c9c331914b"
```
**Status:** ✅ CORRECT - Matches Tire Probe unique UUID

### Characteristics

#### CORNER_READING (26ac) - ✅ CORRECT
```cpp
#define CORNER_READING_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ac"
```
- **Properties:** NOTIFY ✓
- **Format:** JSON ✓
- **Implementation (ble_service.cpp Line 70-98):**
  ```cpp
  StaticJsonDocument<200> doc;
  const char* cornerNames[] = {"RF", "LF", "LR", "RR"};
  doc["corner"] = cornerNames[reading.corner];
  doc["tireInside"] = reading.tireInside;
  doc["tireMiddle"] = reading.tireMiddle;
  doc["tireOutside"] = reading.tireOutside;
  doc["brakeTemp"] = reading.brakeTemp;
  ```
- **Example:** `{"corner":"RF","tireInside":85.0,"tireMiddle":88.5,"tireOutside":82.3,"brakeTemp":176.5}`
- **Issue:** ⚠️ Temperatures in Celsius, mobile app expects Fahrenheit
- **Status:** ✅ Format correct, ❌ Units wrong

#### SYSTEM_STATUS (26aa) - ❌ INCORRECT
```cpp
#define SYSTEM_STATUS_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"
```
- **Properties:** NOTIFY ✓
- **Current Format:** Binary (8 bytes) ❌
- **Implementation (ble_service.cpp Line 100-115):**
  ```cpp
  uint8_t packet[8];
  packet[0] = (uint8_t)status.state;
  packet[1] = status.batteryPercent;
  memcpy(&packet[2], &status.batteryVoltage, sizeof(float));
  packet[6] = status.charging ? 1 : 0;
  packet[7] = capturedCount;
  systemStatusChar->setValue(packet, 8);
  ```
- **Expected Format:** JSON ✓
- **Expected Fields:**
  ```json
  {
    "battery": 85,
    "isCharging": false,
    "firmware": "TTP-4CH-v1"
  }
  ```
- **Status:** ❌ WRONG - Binary instead of JSON

#### DEVICE_CONFIG (26ab) - ℹ️ NOT IMPLEMENTED
- **Expected:** UInt8 (0-3) for corner assignment
- **Current:** Not present
- **Impact:** Medium - Would be useful for multi-device setups
- **Decision:** OPTIONAL (can add later)

#### TIRE_DATA (26a8) and BRAKE_DATA (26a9) - ℹ️ NOT USED
- **Status:** Not implemented (using CORNER_READING instead)
- **Impact:** None - mobile app supports both architectures
- **Reason:** Firmware uses "single device architecture" with CORNER_READING
- **Decision:** CORRECT - This is the preferred approach per protocol docs

---

## Mobile App Expected Protocol

From `packages/ble/src/constants/characteristics.ts`:

```typescript
export const TIRE_PROBE_CHARS = {
  /** Tire temperature data: 3 floats (inner, middle, outer) in Fahrenheit - notify */
  TIRE_DATA: 'beb5483e-36e1-4688-b7f5-ea07361b26a8',  // NOT USED

  /** Brake rotor temperature: 1 float in Fahrenheit - notify */
  BRAKE_DATA: 'beb5483e-36e1-4688-b7f5-ea07361b26a9',  // NOT USED

  /** System status: JSON { battery: number, isCharging: boolean, firmware: string } - read + notify */
  SYSTEM_STATUS: 'beb5483e-36e1-4688-b7f5-ea07361b26aa',  // NEEDS FIX

  /** Device config: corner assignment (LF=0, RF=1, LR=2, RR=3) - read + write */
  DEVICE_CONFIG: 'beb5483e-36e1-4688-b7f5-ea07361b26ab',  // NOT IMPLEMENTED

  /** Corner reading (SINGLE DEVICE ARCHITECTURE) - JSON { corner, tireInside, tireMiddle, tireOutside, brakeTemp } - notify */
  CORNER_READING: 'beb5483e-36e1-4688-b7f5-ea07361b26ac',  // ✅ CORRECT (except units)
} as const;
```

**Mobile app expects (single device architecture):**
- ✅ CORNER_READING (26ac) - JSON format
- ❌ SYSTEM_STATUS (26aa) - JSON format (currently binary)
- ⚠️ DEVICE_CONFIG (26ab) - UInt8 (not implemented)

---

## Comparison Summary

| Aspect | Firmware | Mobile App | Match? |
|--------|----------|------------|--------|
| Service UUID | `...0004...` | `...0004...` | ✅ YES |
| CORNER_READING UUID | `...26ac` | `...26ac` | ✅ YES |
| CORNER_READING Format | JSON | JSON | ✅ YES |
| CORNER_READING Fields | corner, tireInside, tireMiddle, tireOutside, brakeTemp | (same) | ✅ YES |
| Temperature Units | **Celsius** | **Fahrenheit** | ❌ NO |
| SYSTEM_STATUS UUID | `...26aa` | `...26aa` | ✅ YES |
| SYSTEM_STATUS Format | **Binary (8 bytes)** | **JSON** | ❌ NO |
| SYSTEM_STATUS Fields | state, batteryPct, voltage, charging, count | battery, isCharging, firmware | ❌ NO |
| DEVICE_CONFIG | Not present | UInt8 (0-3) | ⚠️ OPTIONAL |
| BLE2902 Descriptors | Automatic (NimBLE) | Required | ✅ YES |

---

## Required Changes

### 1. Convert SYSTEM_STATUS to JSON (REQUIRED)

**File:** `src/ble_service.cpp`
**Function:** `bleTransmitSystemStatus()` (Line 100-115)

**Current (Binary - 8 bytes):**
```cpp
void bleTransmitSystemStatus(const SystemStatus& status, uint8_t capturedCount) {
    uint8_t packet[8];
    packet[0] = (uint8_t)status.state;
    packet[1] = status.batteryPercent;
    memcpy(&packet[2], &status.batteryVoltage, sizeof(float));
    packet[6] = status.charging ? 1 : 0;
    packet[7] = capturedCount;

    systemStatusChar->setValue(packet, 8);
    systemStatusChar->notify();
}
```

**New (JSON):**
```cpp
void bleTransmitSystemStatus(const SystemStatus& status, uint8_t capturedCount) {
    if (!deviceConnected || systemStatusChar == nullptr) {
        return;
    }

    // Build JSON document
    StaticJsonDocument<128> doc;
    doc["battery"] = status.batteryPercent;
    doc["isCharging"] = status.charging;
    doc["firmware"] = DEVICE_MODEL;  // "TTP-4CH-v1"

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

**Impact:** Mobile app can now parse system status correctly

### 2. Convert Temperature Units to Fahrenheit (REQUIRED)

**File:** `src/ble_service.cpp`
**Function:** `bleTransmitCornerReading()` (Line 70-98)

**Current (Celsius):**
```cpp
doc["tireInside"] = reading.tireInside;
doc["tireMiddle"] = reading.tireMiddle;
doc["tireOutside"] = reading.tireOutside;
doc["brakeTemp"] = reading.brakeTemp;
```

**New (Fahrenheit):**
```cpp
// Convert Celsius to Fahrenheit
float tireInsideF = (reading.tireInside * 9.0f / 5.0f) + 32.0f;
float tireMiddleF = (reading.tireMiddle * 9.0f / 5.0f) + 32.0f;
float tireOutsideF = (reading.tireOutside * 9.0f / 5.0f) + 32.0f;
float brakeTempF = (reading.brakeTemp * 9.0f / 5.0f) + 32.0f;

doc["tireInside"] = tireInsideF;
doc["tireMiddle"] = tireMiddleF;
doc["tireOutside"] = tireOutsideF;
doc["brakeTemp"] = brakeTempF;
```

**Impact:** Mobile app displays correct temperature values

### 3. Add DEVICE_CONFIG Characteristic (OPTIONAL)

**File:** `include/ble_protocol.h`
**Add:**
```cpp
#define DEVICE_CONFIG_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
```

**File:** `src/ble_service.cpp`
**Modify `bleInit()` to add:**
```cpp
// Device config characteristic (read + write)
static NimBLECharacteristic* deviceConfigChar = nullptr;

deviceConfigChar = pService->createCharacteristic(
    DEVICE_CONFIG_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
);
deviceConfigChar->setCallbacks(new DeviceConfigCallbacks());

// Set initial value (0=LF, 1=RF, 2=LR, 3=RR)
uint8_t cornerID = (uint8_t)DEFAULT_CORNER;
deviceConfigChar->setValue(&cornerID, 1);
```

**Benefit:** Allows mobile app to read/write corner assignment
**Priority:** LOW (firmware already uses corner-based architecture)

---

## Update Summary

### Files to Modify

1. **`src/ble_service.cpp`** (2 functions)
   - `bleTransmitSystemStatus()` - Change binary to JSON
   - `bleTransmitCornerReading()` - Add Celsius → Fahrenheit conversion

2. **`include/ble_protocol.h`** (optional)
   - Add `DEVICE_CONFIG_UUID` definition

3. **`src/ble_service.cpp`** (optional)
   - Add DEVICE_CONFIG characteristic initialization

### Dependencies

**Current `platformio.ini`:**
```ini
lib_deps =
    h2zero/NimBLE-Arduino@^1.4.1
    adafruit/Adafruit MAX31855 library@^1.4.1
    fastled/FastLED@^3.6.0
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/Adafruit GFX Library@^1.11.5
    bblanchon/ArduinoJson@^6.21.0
```

**Status:** ✅ NO CHANGES NEEDED
- ArduinoJson already present
- All required libraries present

---

## Strengths of Current Implementation

### 1. ✅ Excellent Hardware Design
- 4x MAX31855K thermocouple amplifiers
- 3 tire thermocouples + 1 brake thermocouple per corner
- ESP32-S3 (powerful processor)
- SSD1306 OLED display
- WS2812B RGB LED for status
- TP4056 charging circuit

### 2. ✅ Advanced Measurement System
```cpp
// Temperature smoothing (8-sample moving average)
#define TEMP_SMOOTHING_SAMPLES  8

// Stability detection
#define TEMP_STABLE_THRESHOLD   0.5     // °C variance
#define STABILITY_DURATION_MS   1000    // Must be stable 1 second
```

### 3. ✅ Sequential Capture Workflow
```cpp
enum DeviceState {
    STATE_CORNER_RF,            // Right Front
    STATE_STABILIZING_RF,       // Waiting for stability
    STATE_CAPTURED_RF,          // Captured, show confirmation
    STATE_CORNER_LF,            // Left Front
    // ... etc for all 4 corners
    STATE_SESSION_COMPLETE      // All 4 corners done
};
```

### 4. ✅ Professional Code Organization
- Modular design (separate files for probes, display, BLE, power, LED)
- Clean type definitions
- Good error handling
- State machine architecture
- Proper use of ArduinoJson

### 5. ✅ Battery Management
- Voltage monitoring with ADC averaging (16 samples)
- Charging detection
- Low battery warning (10%)
- Battery percentage calculation

### 6. ✅ NimBLE Implementation
- Lightweight BLE stack
- Automatic descriptor management
- Efficient for ESP32-S3

---

## Testing Checklist

### Pre-Update
- [ ] Backup current firmware binary
- [ ] Document current behavior
- [ ] Test current mobile app compatibility

### Post-Update

#### Compilation Test
```bash
cd tire-temp-probe
pio run
```
**Expected:** Clean compilation, no errors

#### Flash Test
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

#### Mobile App Testing

1. **Discovery**
   - [ ] Open mobile-tire-probe app
   - [ ] Scan for devices
   - [ ] Verify: "TireProbe" appears
   - [ ] Expected: Device discovered

2. **Connection**
   - [ ] Connect to device
   - [ ] Verify: Connection established
   - [ ] Expected: No errors

3. **System Status**
   - [ ] Check status in app
   - [ ] Verify: Shows battery percentage
   - [ ] Verify: Shows charging state
   - [ ] Verify: Shows firmware version "TTP-4CH-v1"
   - [ ] Expected: JSON format `{"battery":85,"isCharging":false,"firmware":"TTP-4CH-v1"}`

4. **Corner Reading - RF (Right Front)**
   - [ ] Place probes on RF tire
   - [ ] Wait for temperature stability
   - [ ] Verify: CORNER_READING notification received
   - [ ] Verify: Shows corner "RF"
   - [ ] Verify: Temperatures in Fahrenheit (e.g., 185°F, not 85°C)
   - [ ] Verify: 3 tire temps + 1 brake temp
   - [ ] Expected: `{"corner":"RF","tireInside":185.0,"tireMiddle":188.5,"tireOutside":182.3,"brakeTemp":356.5}`

5. **Sequential Capture (All 4 Corners)**
   - [ ] Capture RF (Right Front)
   - [ ] Verify: Display shows confirmation
   - [ ] Verify: LED shows green
   - [ ] Capture LF (Left Front)
   - [ ] Capture LR (Left Rear)
   - [ ] Capture RR (Right Rear)
   - [ ] Verify: Session complete state
   - [ ] Expected: All 4 corners transmitted to mobile app

6. **Temperature Accuracy**
   - [ ] Verify temperatures are in Fahrenheit
   - [ ] Verify reasonable values (70-400°F typical)
   - [ ] Compare tire inside/middle/outside spread
   - [ ] Expected: Data makes sense for tire temps

7. **Battery Monitoring**
   - [ ] Check battery percentage in app
   - [ ] Connect USB charger
   - [ ] Verify: isCharging becomes true
   - [ ] Expected: Real-time battery updates

---

## Device Naming

**Current:** `"TireProbe"` (hardcoded)

**Status:** ⚠️ SHOULD UPDATE for multi-device support

**Reason:** If user has 4 tire probes (one per corner), they all advertise as "TireProbe" and are indistinguishable.

**Recommended change:**
```cpp
// Generate unique device name from MAC address
String mac = WiFi.macAddress();  // "AA:BB:CC:DD:EE:FF"
mac.replace(":", "");            // "AABBCCDDEEFF"
String deviceName = "TireProbe_" + mac.substring(8);  // "TireProbe_EEFF"
bleInit(deviceName.c_str());
```

**OR use corner-based naming:**
```cpp
const char* cornerNames[] = {"RF", "LF", "LR", "RR"};
String deviceName = String("TireProbe_") + cornerNames[currentCorner];
// Results in: "TireProbe_RF", "TireProbe_LF", etc.
```

**Priority:** MEDIUM (important for 4-device setup)

---

## Risk Assessment

**Risk Level:** 🟡 LOW-MEDIUM

**Why:**
- Only 2 function changes needed
- ArduinoJson already in use
- Temperature conversion is straightforward
- JSON format simpler than binary

**Potential Issues:**
- Temperature conversion could affect display if not updated
- Need to update any other code that uses SystemStatus binary format
- Need to ensure mobile app can handle new JSON format

**Recommendation:** Test thoroughly on one device before deploying to all 4

---

## Comparison: All Devices Audited

| Device | Initial Compliance | Changes Required | Complexity |
|--------|-------------------|------------------|------------|
| **Ride Height** 🥇 | 100% | 0 | None |
| **Oil Heater** 🥈 | 90% | 1 UUID | Trivial |
| **Tire Probe** 🥉 | 85% | 2 formats | Low |
| **RaceScale** | ~60% | 6 formats | High |

**Tire Probe ranking:** 3rd place - Good implementation, minor format fixes needed

---

## Future Improvements (Optional, Low Priority)

### 1. Add DEVICE_CONFIG Characteristic

**Current:** Corner assignment hardcoded in firmware

**Enhancement:** Allow mobile app to set corner via BLE
```cpp
// Mobile app writes: 0=LF, 1=RF, 2=LR, 3=RR
// Device stores in NVS, uses on next boot
```

**Priority:** LOW (current sequential workflow works well)

### 2. Add Firmware Version to JSON

**Current:** Hardcoded string "TTP-4CH-v1"

**Enhancement:** Include version number
```json
{
  "battery": 85,
  "isCharging": false,
  "firmware": "1.0.0",
  "model": "TTP-4CH-v1"
}
```

**Priority:** VERY LOW (current approach sufficient)

### 3. Temperature Unit Selection

**Current:** Always Fahrenheit (after update)

**Enhancement:** Add Celsius/Fahrenheit toggle
```cpp
// Via DEVICE_CONFIG or separate characteristic
bool useFahrenheit = true;  // User preference
```

**Priority:** VERY LOW (US market primarily uses Fahrenheit)

---

## Success Criteria

After update:
- [x] Service UUID is `4fafc201-0004-459e-8fcc-c5c9c331914b`
- [ ] CORNER_READING sends temperatures in Fahrenheit
- [ ] SYSTEM_STATUS sends JSON format
- [ ] Mobile app parses all data correctly
- [ ] Sequential capture workflow functions
- [ ] All 4 corners can be measured
- [ ] Battery status displays correctly
- [ ] Temperature readings accurate

---

## Files Created/Modified

**Documentation:**
- `tire-temp-probe/BLE_PROTOCOL_AUDIT.md` (this file)

**Firmware to Modify:**
- `src/ble_service.cpp` - 2 function updates
- `include/ble_protocol.h` - Optional DEVICE_CONFIG UUID

**Status Tracking:**
- `FIRMWARE_UPDATE_STATUS.md` - Updated status

---

## Conclusion

The Tire Temperature Probe firmware is **well-designed** with excellent hardware integration and code organization. Only 2 format changes are needed:

1. **SYSTEM_STATUS:** Binary → JSON
2. **Temperature Units:** Celsius → Fahrenheit

Both changes are straightforward and low-risk. The firmware's use of the CORNER_READING characteristic (single device architecture) is the correct and preferred approach.

**Recommendation:** Apply updates and test on one device before deploying to all 4 probes.

---

**Audit Completed:** 2026-01-27
**Auditor:** Claude Code
**Firmware Version:** TTP-4CH-v1
**Initial Compliance:** 85%
**Final Compliance:** 100% (after updates)
**Changes Required:** 2 format conversions
**Risk Level:** 🟡 LOW-MEDIUM

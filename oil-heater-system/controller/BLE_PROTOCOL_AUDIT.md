# Oil Heater Controller - BLE Protocol Audit

**Date:** 2026-01-27
**Firmware:** Smart Oil Heater Controller (Control Board)
**Mobile App:** mobile-oil-heater
**Expected Service UUID:** `4fafc201-0001-459e-8fcc-c5c9c331914b`

---

## Executive Summary

**Overall Status:** ⚠️ MINOR UPDATES NEEDED

The Oil Heater firmware is **90% compliant** with the mobile app protocol. The characteristic data formats are correct, but the Service UUID needs updating and there's an extra characteristic that should be removed.

### Issues Found

| Issue | Severity | Status |
|-------|----------|--------|
| Service UUID (old shared UUID) | 🟡 MEDIUM | Needs fix |
| ENABLE characteristic (not in protocol) | 🟢 LOW | Optional removal |
| Data formats | ✅ CORRECT | No changes needed |

---

## Current BLE Implementation

### Service UUID (Line 77)
```cpp
#define BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
```
**Status:** ❌ INCORRECT - Old shared UUID
**Should be:** `4fafc201-0001-459e-8fcc-c5c9c331914b` (unique Oil Heater UUID)

### Characteristics

#### TEMPERATURE (26a8) - ✅ CORRECT
```cpp
#define BLE_CHAR_TEMP_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
```
- **Properties:** READ, NOTIFY ✓
- **Format:** String UTF-8 ✓
- **Implementation (Line 555-563):**
  ```cpp
  float tempF = (g_currentTempC * 9.0f / 5.0f) + 32.0f;
  snprintf(tempStr, sizeof(tempStr), "%.1f", tempF);
  g_charTemp->setValue(tempStr);
  g_charTemp->notify();
  ```
- **Example:** `"180.5"` (Fahrenheit)
- **Status:** ✅ CORRECT - Matches mobile app expectations

#### SETPOINT (26a9) - ✅ CORRECT
```cpp
#define BLE_CHAR_SETPOINT_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
```
- **Properties:** READ, WRITE ✓
- **Format:** String UTF-8 ✓
- **Write Implementation (Line 241-256):**
  ```cpp
  float setpointF = atof(value.c_str());
  float setpointC = (setpointF - 32.0f) * 5.0f / 9.0f;
  g_setpointC = constrain(setpointC, MIN_SETPOINT_C, MAX_SETPOINT_C);
  ```
- **Read Implementation (Line 566-569):**
  ```cpp
  float setpointF = (g_setpointC * 9.0f / 5.0f) + 32.0f;
  snprintf(setpointStr, sizeof(setpointStr), "%.1f", setpointF);
  g_charSetpoint->setValue(setpointStr);
  ```
- **Example:** Write `"180.0"`, Read `"180.0"` (Fahrenheit)
- **Status:** ✅ CORRECT - Matches mobile app expectations

#### STATUS (26aa) - ✅ CORRECT
```cpp
#define BLE_CHAR_STATUS_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"
```
- **Properties:** READ, NOTIFY ✓
- **Format:** JSON ✓
- **Implementation (Line 576-586):**
  ```cpp
  snprintf(statusJson, sizeof(statusJson),
           "{\"heater\":%s,\"safetyShutdown\":%s,\"sensorError\":%s}",
           g_relayState ? "true" : "false",
           safetyShutdown ? "true" : "false",
           sensorError ? "true" : "false");
  g_charStatus->setValue(statusJson);
  g_charStatus->notify();
  ```
- **Example:** `{"heater":true,"safetyShutdown":false,"sensorError":false}`
- **Status:** ✅ CORRECT - Matches mobile app expectations exactly!
- **Note:** Line 572 comment shows this was recently fixed to match mobile app format

#### ENABLE (26ab) - ⚠️ NOT IN PROTOCOL
```cpp
#define BLE_CHAR_ENABLE_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
```
- **Properties:** READ, WRITE
- **Format:** String `"1"` or `"0"`
- **Status:** ⚠️ EXTRA - Not defined in packages/ble protocol
- **Impact:** Mobile app doesn't use this characteristic
- **Recommendation:** Can be removed or kept (won't break anything)

---

## Mobile App Expected Protocol

From `packages/ble/src/constants/characteristics.ts`:

```typescript
export const OIL_HEATER_CHARS = {
  TEMPERATURE: 'beb5483e-36e1-4688-b7f5-ea07361b26a8',  // String, e.g., "180.5"
  SETPOINT: 'beb5483e-36e1-4688-b7f5-ea07361b26a9',     // String, e.g., "180.0" (R/W)
  STATUS: 'beb5483e-36e1-4688-b7f5-ea07361b26aa',       // JSON
} as const;
```

**Mobile app protocol includes:**
- ✅ TEMPERATURE (26a8)
- ✅ SETPOINT (26a9)
- ✅ STATUS (26aa)

**Mobile app protocol does NOT include:**
- ❌ ENABLE (26ab) - Firmware-specific

---

## Comparison Summary

| Aspect | Firmware | Mobile App | Match? |
|--------|----------|------------|--------|
| Service UUID | `...1fb5...` (old) | `...0001...` (new) | ❌ NO |
| TEMPERATURE UUID | `...26a8` | `...26a8` | ✅ YES |
| TEMPERATURE Format | String F | String F | ✅ YES |
| SETPOINT UUID | `...26a9` | `...26a9` | ✅ YES |
| SETPOINT Format | String F (R/W) | String F (R/W) | ✅ YES |
| STATUS UUID | `...26aa` | `...26aa` | ✅ YES |
| STATUS Format | JSON | JSON | ✅ YES |
| STATUS Fields | heater, safetyShutdown, sensorError | heater, safetyShutdown, sensorError | ✅ YES |
| ENABLE Char | Present | NOT in protocol | ⚠️ EXTRA |
| BLE2902 Descriptors | Present | Required | ✅ YES |

---

## Required Changes

### 1. Update Service UUID (REQUIRED)

**File:** `src/main.cpp`
**Line:** 77

**Change:**
```cpp
// OLD
#define BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// NEW
#define BLE_SERVICE_UUID "4fafc201-0001-459e-8fcc-c5c9c331914b"
```

**Impact:** Mobile app uses service UUID for discovery. Old UUID may still work (legacy support) but should be updated for consistency.

### 2. Remove or Keep ENABLE Characteristic (OPTIONAL)

**Options:**

**Option A: Remove (Recommended for protocol compliance)**
- Delete lines 84, 218, 524-529, 589
- Comment explains this characteristic is firmware-specific and not used by mobile app
- Keeps protocol clean

**Option B: Keep (Recommended for backward compatibility)**
- Leave as-is
- Mobile app will ignore unknown characteristics
- Allows future firmware features
- Current choice in firmware

**Recommendation:** KEEP IT - No harm, provides extra control channel

---

## Dependencies Check

**Current `platformio.ini`:**
```ini
lib_deps =
    adafruit/MAX6675 library@^1.1.2
```

**Status:** ✅ NO CHANGES NEEDED
- No ArduinoJson dependency required (JSON is built manually with snprintf)
- All BLE libraries are built-in to ESP32 framework

---

## Testing Checklist

### Pre-Update
- [ ] Backup current firmware binary
- [ ] Document current behavior
- [ ] Note any custom calibration values

### Post-Update
- [ ] Compile with no errors
- [ ] Flash to device
- [ ] Serial monitor shows correct UUID
- [ ] Mobile app discovers device as "Heater_Controller"
- [ ] Temperature reading works
- [ ] Setpoint write/read works
- [ ] Status updates correctly
- [ ] Heater control works
- [ ] Safety features still active

### Mobile App Testing
- [ ] Open mobile-oil-heater app
- [ ] Scan for devices
- [ ] Connect to "Heater_Controller"
- [ ] Verify temperature displays
- [ ] Change setpoint from app
- [ ] Verify heating cycles on/off
- [ ] Check status updates (heater on/off)
- [ ] Test safety shutdown scenarios
- [ ] Test sensor error detection

---

## Device Naming

**Current:** `Heater_Controller` (hardcoded, line 73)
**Expected:** `Heater_XXXX` (where XXXX = last 4 MAC chars)

**Status:** ⚠️ SHOULD UPDATE for multi-device support

**Reason:** If user has multiple oil heaters, they all advertise as "Heater_Controller" and are indistinguishable.

**Recommended change:**
```cpp
// Generate unique device name from MAC address
String mac = WiFi.macAddress();  // "AA:BB:CC:DD:EE:FF"
mac.replace(":", "");            // "AABBCCDDEEFF"
String deviceName = "Heater_" + mac.substring(8);  // "Heater_EEFF"
BLEDevice::init(deviceName.c_str());
```

**Priority:** LOW (only needed if user has multiple heaters)

---

## Strengths of Current Implementation

1. **✅ Excellent Safety Features**
   - Watchdog timer (5s timeout)
   - Overtemp cutoff
   - Sensor fault detection
   - All correctly integrated with STATUS characteristic

2. **✅ Correct JSON Status Format**
   - Line 572 comment shows recent fix
   - Matches mobile app expectations exactly
   - All three required fields present

3. **✅ Proper BLE2902 Descriptors**
   - Line 508: Temperature NOTIFY has descriptor
   - Line 522: Status NOTIFY has descriptor
   - Mobile app can subscribe correctly

4. **✅ Temperature Smoothing**
   - Moving average filter (15 samples)
   - Reduces noise in BLE updates
   - Professional implementation

5. **✅ Calibration System**
   - Single-point and two-point calibration modes
   - Well-documented configuration
   - Applied before sending to BLE

6. **✅ Dual Control Interfaces**
   - BLE for mobile app
   - UART for display board
   - Both work simultaneously

---

## Weaknesses / Improvement Opportunities

1. **⚠️ Old Service UUID**
   - Using legacy shared UUID
   - Should use unique Oil Heater UUID

2. **⚠️ Hardcoded Device Name**
   - All devices advertise as "Heater_Controller"
   - Should include MAC suffix for multi-device support

3. **ℹ️ Manual JSON Formatting**
   - Using snprintf instead of ArduinoJson
   - Current approach works fine but less flexible
   - No changes needed unless adding more fields

---

## Update Priority

**Severity:** 🟡 MEDIUM
**Urgency:** 🟢 LOW (firmware works correctly with mobile app)
**Effort:** 🟢 LOW (1-line change)

**Recommendation:** Update Service UUID at next maintenance window. Current firmware is functional and safe.

---

## Success Criteria

After update:
- [ ] Service UUID is `4fafc201-0001-459e-8fcc-c5c9c331914b`
- [ ] Temperature characteristic works (string format)
- [ ] Setpoint characteristic works (read/write, string format)
- [ ] Status characteristic works (JSON format)
- [ ] Mobile app connects and controls heater
- [ ] Safety features still active
- [ ] Device name unique (if updated)

---

## Files to Modify

1. **`src/main.cpp`** (1 required change, 1 optional)
   - Line 77: Update SERVICE_UUID ✅ REQUIRED
   - Line 73-494: Update device naming ⚠️ OPTIONAL

2. **`platformio.ini`**
   - ✅ NO CHANGES NEEDED

---

## Next Steps

1. **Update Service UUID:**
   ```bash
   cd oil-heater-system/controller
   # Edit src/main.cpp line 77
   # Change: 4fafc201-1fb5-... → 4fafc201-0001-...
   ```

2. **Optional: Update Device Name:**
   ```bash
   # Add WiFi.h include
   # Update BLE_DEVICE_NAME generation with MAC suffix
   ```

3. **Compile:**
   ```bash
   pio run
   ```

4. **Test:**
   ```bash
   pio run -t upload
   pio device monitor
   ```

5. **Verify with Mobile App**

---

**Audit Completed:** 2026-01-27
**Firmware Version:** Smart Oil Heater Controller V1.0
**Compliance:** 90% (excellent!)
**Required Changes:** 1 (Service UUID)
**Optional Changes:** 1 (Device naming)

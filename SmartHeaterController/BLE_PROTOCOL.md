# BLE Protocol Documentation
## Smart Oil Heater Controller

**Firmware Version**: 1.0
**Last Updated**: 2025-12-31

---

## Overview

This document describes the Bluetooth Low Energy (BLE) protocol for the Smart Oil Heater Controller. The controller exposes a custom GATT service with three characteristics for monitoring and controlling an ESP32-based oil heater thermostat system.

**Device Name**: `Heater_Controller`

---

## Service Definition

### Primary Service

| Property | Value |
|----------|-------|
| **Service UUID** | `4fafc201-0001-459e-8fcc-c5c9c331914b` |
| **Description** | Oil Heater Controller Service |
| **Type** | Custom/Primary |

**Note**: This UUID MUST match `SERVICE_UUIDS.OIL_HEATER` in `@crewchiefsteve/ble` package. See `packages/ble/src/constants/uuids.ts` for all device UUIDs.

---

## Characteristics

### 1. Temperature Characteristic

**Purpose**: Reports the current thermocouple temperature reading in Fahrenheit.

| Property | Value |
|----------|-------|
| **UUID** | `beb5483e-36e1-4688-b7f5-ea07361b26a8` |
| **Properties** | READ, NOTIFY |
| **Data Type** | ASCII String |
| **Format** | Floating-point formatted as "%.1f" |
| **Example** | `"180.5"` (180.5°F) |
| **Range** | 0.0°F to 999.0°F (999.0 indicates sensor error) |
| **Update Rate** | 500ms (when client connected) |

**Data Format**:
- **Encoding**: UTF-8 ASCII string
- **Length**: Variable (typically 4-6 bytes)
- **Precision**: One decimal place

**Example Values**:
```
"72.0"    → 72.0°F (room temperature)
"180.5"   → 180.5°F (typical operating temperature)
"999.0"   → Sensor error (thermocouple disconnected)
```

**Error Indication**:
- Reading of `999.0` indicates sensor error (MAX6675 fault)
- Check the Status characteristic for more details

---

### 2. Setpoint Characteristic

**Purpose**: Reports and controls the target temperature setpoint in Fahrenheit.

| Property | Value |
|----------|-------|
| **UUID** | `beb5483e-36e1-4688-b7f5-ea07361b26a9` |
| **Properties** | READ, WRITE, NOTIFY |
| **Data Type** | ASCII String |
| **Format** | Floating-point formatted as "%.1f" |
| **Example** | `"180.0"` (180.0°F setpoint) |
| **Range** | 50.0°F to 280.0°F |
| **Default** | 180.0°F (race oil operating temperature) |
| **Update Rate** | 500ms (when client connected) |

**Data Format**:
- **Encoding**: UTF-8 ASCII string
- **Length**: Variable (typically 4-6 bytes)
- **Precision**: One decimal place (values are rounded internally)

**Write Behavior**:
- **Input Format**: ASCII string representing a float (e.g., `"200.0"` or `"200"`)
- **Validation**: Value must be between 50.0°F and 280.0°F (inclusive)
- **Response**: On successful write, characteristic value updates and notification is sent with confirmed value
- **Error Handling**: Invalid values are rejected silently (no error code returned, check serial log)

**Example Write Operations**:
```
Write "200.0" → Setpoint changes to 200.0°F ✓
Write "250.5" → Setpoint changes to 250.5°F ✓
Write "300.0" → Rejected (exceeds MAX_SETPOINT_F=280.0°F) ✗
Write "40.0"  → Rejected (below MIN_SETPOINT_F=50.0°F) ✗
```

**Side Effects**:
- Writing a valid setpoint will **reset safety shutdown** if:
  - Current temperature < 300.0°F AND
  - No sensor error detected
- Display updates immediately
- Thermostat logic re-evaluates with new setpoint on next cycle

---

### 3. Status Characteristic

**Purpose**: Reports system status including heater state, safety status, and sensor health.

| Property | Value |
|----------|-------|
| **UUID** | `beb5483e-36e1-4688-b7f5-ea07361b26aa` |
| **Properties** | READ, NOTIFY |
| **Data Type** | JSON String |
| **Update Rate** | 500ms (when client connected) |

**Data Format**:
- **Encoding**: UTF-8 JSON string
- **Length**: Variable (typically 50-70 bytes)

**JSON Structure**:
```json
{
  "heater": "ON" | "OFF",
  "safety": "OK" | "SHUTDOWN",
  "sensor": "OK" | "ERROR"
}
```

**Field Definitions**:

| Field | Type | Values | Description |
|-------|------|--------|-------------|
| `heater` | String | `"ON"`, `"OFF"` | Current relay state (heater power) |
| `safety` | String | `"OK"`, `"SHUTDOWN"` | Safety system status |
| `sensor` | String | `"OK"`, `"ERROR"` | Thermocouple sensor health |

**Example Values**:
```json
{"heater":"OFF","safety":"OK","sensor":"OK"}     → Normal operation, heater off
{"heater":"ON","safety":"OK","sensor":"OK"}      → Normal operation, heater on
{"heater":"OFF","safety":"SHUTDOWN","sensor":"OK"} → Over-temperature shutdown
{"heater":"OFF","safety":"SHUTDOWN","sensor":"ERROR"} → Sensor fault shutdown
```

**Status Meanings**:

- **heater: "ON"** - Relay activated, heater is powered
- **heater: "OFF"** - Relay deactivated, heater is off
- **safety: "OK"** - System operating normally
- **safety: "SHUTDOWN"** - Emergency shutdown active (over-temp or sensor error)
- **sensor: "OK"** - Thermocouple reading valid
- **sensor: "ERROR"** - Thermocouple disconnected or MAX6675 fault

---

## Connection Management

### Advertising

- **Device Name**: `Heater_Controller`
- **Advertising**: Continuous (restarts after disconnect)
- **Scan Response**: Enabled
- **Service UUID**: Included in advertising packet

### Connection Parameters

- **Connection Interval**: Minimum preferred 0x06 (7.5ms)
- **Connection Supervision**: Standard BLE parameters

### Notifications

All characteristics with NOTIFY property send updates every **500ms** when a client is subscribed.

**To Enable Notifications**:
1. Connect to the device
2. Write `0x0001` to the Client Characteristic Configuration Descriptor (CCCD/0x2902) for each characteristic you want to monitor

---

## Typical Usage Patterns

### Read Current State

**Objective**: Get current temperature, setpoint, and status.

```
1. Connect to "Heater_Controller"
2. Discover service 4fafc201-0001-459e-8fcc-c5c9c331914b
3. READ from Temperature characteristic → e.g., "175.5"
4. READ from Setpoint characteristic → e.g., "180.0"
5. READ from Status characteristic → e.g., {"heater":"ON","safety":"OK","sensor":"OK"}
```

---

### Monitor Temperature in Real-Time

**Objective**: Subscribe to live temperature updates.

```
1. Connect to device
2. Enable notifications on Temperature characteristic (write 0x0001 to CCCD)
3. Receive notifications every 500ms with updated temperature
```

---

### Change Setpoint

**Objective**: Set new target temperature to 200°F.

```
1. Connect to device
2. WRITE "200.0" to Setpoint characteristic
3. Device validates (50.0 ≤ 200.0 ≤ 280.0) → Valid ✓
4. Receive notification with confirmed value "200.0"
5. Thermostat begins controlling to new setpoint
6. If device was in safety shutdown, it may reset (if conditions allow)
```

**Error Example**:
```
1. WRITE "350.0" to Setpoint characteristic
2. Device rejects silently (350.0 > 280.0°F max)
3. No notification sent, setpoint unchanged
4. Check serial log for error message
```

---

### Monitor for Safety Shutdown

**Objective**: Detect when safety system triggers.

```
1. Subscribe to Status characteristic notifications
2. Monitor "safety" field in JSON
3. If "safety":"SHUTDOWN" detected:
   - Check "sensor" field: "ERROR" indicates thermocouple fault
   - Check Temperature characteristic: If reading > 300.0°F, over-temp shutdown
   - Heater will be "OFF" and locked
4. To reset: Write valid setpoint when temp < 300°F and sensor OK
```

---

## Command Reference

### Write Commands

| Characteristic | Command Format | Example | Result |
|----------------|----------------|---------|--------|
| Setpoint | ASCII float string | `"185.0"` | Sets target to 185.0°F |
| Setpoint | ASCII float string | `"220"` | Sets target to 220.0°F |

**No other write commands are currently supported.**

---

## Data Type Reference

### ASCII Float String Format

**Encoding**: UTF-8 text representation of floating-point number

**Examples**:
- `"180.0"` - Explicit decimal
- `"180"` - Integer format (parsed as 180.0)
- `"180.5"` - One decimal place
- `"200.25"` - Two decimal places (accepted, rounded internally)

**Parsing**:
- Uses standard C `atof()` function
- Leading/trailing whitespace is tolerated
- Invalid strings (e.g., `"abc"`) parse as 0.0 and are rejected

---

## Safety and Limits

### Temperature Limits

| Parameter | Value | Description |
|-----------|-------|-------------|
| **MIN_SETPOINT_F** | 50.0°F | Minimum allowed setpoint |
| **MAX_SETPOINT_F** | 280.0°F | Maximum allowed setpoint |
| **SAFETY_MAX_TEMP_F** | 300.0°F | Emergency shutdown temperature |
| **SENSOR_ERROR_TEMP** | 999.0°F | Temperature reading indicating sensor error |

### Thermostat Behavior

- **Hysteresis**: ±1.0°F around setpoint (configurable in firmware)
- **Relay Cycle Time**: Minimum 10 seconds between state changes (prevents relay wear)
- **Safety Override**: Safety shutdown disables thermostat until reset

### Safety Shutdown Triggers

1. **Over-Temperature**: `currentTemp > 300.0°F`
2. **Sensor Error**: MAX6675 fault detection (D2 bit set in raw reading)

**Reset Conditions**:
- User adjusts setpoint via BLE or touchscreen AND
- `currentTemp < 300.0°F` AND
- No active sensor error

---

## Error Handling

### Write Validation Errors

**Symptom**: Write appears to succeed, but value doesn't change.

**Causes**:
- Value out of range (< 50.0°F or > 280.0°F)
- Invalid string format

**Debugging**:
- Connect to serial monitor (115200 baud)
- Check for error messages in format:
  ```
  BLE Write Error: Setpoint 350.0°F out of range (50.0-280.0°F)
  ```

### Sensor Errors

**Symptom**: Temperature reads 999.0°F constantly.

**Causes**:
- Thermocouple disconnected
- MAX6675 SPI communication error
- Faulty thermocouple or wiring

**Detection**:
- Temperature characteristic reads `"999.0"`
- Status characteristic shows `"sensor":"ERROR"`

### Connection Issues

**Symptom**: Cannot connect or frequent disconnects.

**Solutions**:
- Ensure device is in range (BLE ~10m typical)
- Check that device is advertising (LED indicator or serial log)
- Restart device if needed (power cycle)
- On disconnect, device automatically restarts advertising

---

## Firmware Implementation Notes

### Source Files

- **Main Implementation**: `src/main.cpp`
  - BLE initialization: `initBluetooth()` (line 288)
  - BLE updates: `updateBluetooth()` (line 344)
  - Write callback: `SetpointCallbacks::onWrite()` (line 48)

### Update Timing

- **Temperature Reading**: Every 1000ms (from MAX6675 sensor)
- **Display Update**: Every 500ms
- **BLE Notifications**: Every 500ms (when client connected)

### Dependencies

- **ESP32 BLE Arduino Library** (`BLEDevice.h`, `BLEServer.h`, etc.)
- BLE stack configured for single client connection

---

## Mobile App Development Guide

### Recommended Libraries

**iOS**: CoreBluetooth framework
**Android**: Android BLE API (android.bluetooth.le)
**Cross-Platform**: React Native BLE Manager, Flutter Blue, etc.

### Sample Connection Flow (Pseudocode)

```javascript
// 1. Scan for device
const device = await scanForDevice("Heater_Controller");

// 2. Connect
await device.connect();

// 3. Discover services
const service = await device.getService("4fafc201-0001-459e-8fcc-c5c9c331914b");

// 4. Get characteristics
const tempChar = await service.getCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8");
const setpointChar = await service.getCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a9");
const statusChar = await service.getCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26aa");

// 5. Subscribe to notifications
await tempChar.startNotifications((data) => {
  const temp = parseFloat(data); // Parse ASCII string to float
  console.log("Temperature:", temp);
});

await statusChar.startNotifications((data) => {
  const status = JSON.parse(data); // Parse JSON
  console.log("Heater:", status.heater);
  console.log("Safety:", status.safety);
});

// 6. Read current setpoint
const currentSetpoint = parseFloat(await setpointChar.read());
console.log("Current setpoint:", currentSetpoint);

// 7. Write new setpoint
const newSetpoint = 200.0;
await setpointChar.write(newSetpoint.toString()); // Convert to ASCII string

// 8. Monitor for confirmation
await setpointChar.startNotifications((data) => {
  const confirmedSetpoint = parseFloat(data);
  if (confirmedSetpoint === newSetpoint) {
    console.log("Setpoint confirmed!");
  }
});
```

### UI Recommendations

**Display Elements**:
- Large temperature display (current reading)
- Setpoint input/slider (50-280°F range)
- Heater status indicator (ON/OFF with color coding)
- Safety/error alerts (prominent warning for "SHUTDOWN" or "ERROR")

**User Controls**:
- Setpoint adjustment (±5°F buttons or slider)
- Real-time temperature graph (optional)
- Connection status indicator

**Error Handling**:
- Validate setpoint locally before writing (50-280°F)
- Display timeout warning if no notifications received for >2 seconds
- Show prominent alert for safety shutdown

---

## Changelog

### Version 1.0 (2025-12-31)
- Initial BLE protocol implementation
- Added READ/NOTIFY for Temperature and Status characteristics
- Added READ/WRITE/NOTIFY for Setpoint characteristic
- Write validation with range checking (50-280°F)
- Automatic safety shutdown reset on valid setpoint write

---

## Support and Contact

For firmware issues or questions, refer to:
- **Project Repository**: SmartHeaterController/
- **Hardware Documentation**: CLAUDE.md
- **Serial Monitor**: 115200 baud for debug output

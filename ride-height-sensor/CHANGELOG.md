# Ride Height Sensor Firmware Update

## Version 2.0 - Corner Configuration Support

Updated to match RaceScale corner configuration pattern for BLE compatibility with @crewchiefsteve/ble package.

### Changes Made

#### 1. Configuration Updates (`include/config.h`)

- Changed `BLE_DEVICE_NAME` to `BLE_DEVICE_NAME_BASE` for dynamic naming
- Added new BLE characteristics:
  - `CHAR_STATUS_UUID` (beb5483e-36e1-4688-b7f5-ea07361b26aa) - Status (Read/Notify)
  - `CHAR_CORNER_UUID` (beb5483e-36e1-4688-b7f5-ea07361b26ad) - Corner ID (Read/Write/Notify)
- Added NVS configuration:
  - `NVS_NAMESPACE` = "rh_sensor_v1"
  - `NVS_CORNER_KEY` = "corner_id"
  - `NVS_ZERO_OFFSET_KEY` = "zero_offset"
  - `DEFAULT_CORNER` = "01"

#### 2. Main Firmware Updates (`src/main.cpp`)

**Added NVS Persistence:**
- Added `Preferences` library include
- Created `loadSettings()` function to load corner ID and zero offset from NVS
- Created `saveZeroOffset()` function to persist calibration data
- Added `preferences` global object

**Dynamic Device Naming:**
- Added `cornerID`, `deviceName`, and `deviceStatus` state variables
- Device name format: `RH-Sensor_XX` (where XX is corner ID like "LF", "RF", "01", etc.)
- Updated `initializeBLE()` to use dynamic device name

**New BLE Characteristics:**
- Added `pStatusCharacteristic` pointer
- Added `pCornerCharacteristic` pointer
- Created Status characteristic (Read/Notify) - reports device state ("idle", "continuous", "calibrating")
- Created Corner ID characteristic (Read/Write/Notify) - configurable corner assignment

**Corner Configuration:**
- Created `CornerCallbacks` class with write/read handlers
- Validates corner ID format (LF, RF, LR, RR, or 01-99)
- Saves to NVS and notifies connected clients
- Updates take effect immediately without restart

**Serial Command Interface:**
- Added `handleSerialCommands()` function with support for:
  - `corner <ID>` - Set corner identity (e.g., "corner LF")
  - `info` - Display current settings (corner, zero offset, device name)
  - `zero` - Perform zero calibration
  - `help` - Show command help

**Status Tracking:**
- Updated `performZeroCalibration()` to set status during calibration
- Added status updates in `CommandCallbacks` for all commands:
  - Continuous mode start: status = "continuous"
  - Continuous mode stop: status = "idle"
  - Calibration: status = "calibrating" → "idle"
- Zero offset now persists to NVS automatically

**Initialization Flow:**
1. Load settings from NVS (corner ID, zero offset)
2. Build dynamic device name based on corner ID
3. Initialize BLE with dynamic name
4. Set up all characteristics including new Status and Corner

**Main Loop:**
- Added `handleSerialCommands()` call for serial interface

### BLE Characteristics Summary

| UUID (last 2 bytes) | Name | Type | Description |
|---------------------|------|------|-------------|
| ...26a8 | Height Data | R/N | "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85" |
| ...26a9 | Command | W | 'R'=single, 'C'=continuous, 'S'=stop, 'Z'=zero |
| ...26aa | Status | R/N | "idle", "continuous", "calibrating" |
| ...26ad | Corner ID | R/W/N | "LF", "RF", "LR", "RR", "01", "02", etc. |

### Compatibility Notes

- **BLE Service UUID:** `4fafc201-0003-459e-8fcc-c5c9c331914b`
- **Compatible with:** @crewchiefsteve/ble package (see packages/ble/src/constants/uuids.ts)
- **Corner ID Format:**
  - Standard: "LF", "RF", "LR", "RR" (left/right front/rear)
  - Numeric: "01" through "99"
  - Case-insensitive (automatically converted to uppercase)

### Usage Examples

#### Via Serial (115200 baud)

```
# Set corner to Left Front
corner LF

# Set corner to position 01
corner 01

# View current settings
info

# Perform zero calibration
zero

# Show help
help
```

#### Via BLE

```javascript
// Read current corner ID
const cornerChar = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26ad');
const value = await cornerChar.readValue();
const cornerID = new TextDecoder().decode(value);
console.log(`Corner: ${cornerID}`);

// Set corner ID (takes effect immediately)
await cornerChar.writeValue(new TextEncoder().encode('LF'));

// Subscribe to status updates
const statusChar = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26aa');
await statusChar.startNotifications();
statusChar.addEventListener('characteristicvaluechanged', (event) => {
  const status = new TextDecoder().decode(event.target.value);
  console.log(`Status: ${status}`);
});
```

### Migration from V1.x

1. Flash updated firmware
2. Connect via serial monitor (115200 baud)
3. Set corner identity:
   ```
   corner LF
   ```
4. Device will now advertise as "RH-Sensor_LF"
5. Corner ID and zero calibration persist across reboots

### Known Issues

- NimBLE-Arduino v1.4.1 has a compilation issue on some Windows environments. If build fails with "nimble_port_freertos.h: No such file or directory", try:
  - Using PlatformIO on Linux/WSL
  - Updating to latest ESP32 Arduino core
  - Using NimBLE-Arduino v1.4.0 instead

### Files Modified

- `include/config.h` - Added new UUIDs and NVS configuration
- `src/main.cpp` - All functionality updates listed above
- No changes to hardware wiring or pin assignments

### Next Steps

If this is the first sensor being configured:
1. Set corner ID via serial or BLE
2. Perform zero calibration with car at ride height
3. Label physical sensor with corner ID
4. Repeat for remaining three corners (LF, RF, LR, RR)

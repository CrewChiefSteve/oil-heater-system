# RaceScale BLE Mock Firmware

Mock firmware for testing mobile app with **single corner scale**. The mobile app connects to **4 separate ESP32 instances** simultaneously (one for each corner).

## Hardware Required
- ESP32 dev board (any basic model)
- USB cable for programming and power
- **4 ESP32s total** to simulate complete setup (LF, RF, LR, RR)

## UUIDs
- Service: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- Weight: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (Float32LE binary)
- Tare: `beb5483e-36e1-4688-b7f5-ea07361b26ad` (Write UInt8)
- Calibration: `beb5483e-36e1-4688-b7f5-ea07361b26aa` (Float32LE)
- Temperature: `beb5483e-36e1-4688-b7f5-ea07361b26ab` (Float32LE)
- Status: `beb5483e-36e1-4688-b7f5-ea07361b26ac` (JSON)
- Battery: `beb5483e-36e1-4688-b7f5-ea07361b26ae` (UInt8)
- Corner ID: `beb5483e-36e1-4688-b7f5-ea07361b26af` (String)

## Build & Upload

```bash
cd mock-firmware/racescale-mock
pio run --target upload
pio device monitor
```

## Configuration

### Set Corner Identity (Serial)
Send one of these commands via serial (115200 baud):
```
LF      # Left Front
RF      # Right Front
LR      # Left Rear
RR      # Right Rear
```

**IMPORTANT**: Corner ID is stored in NVS (persistent). Restart ESP32 after changing to update BLE device name.

### Default Weights by Corner
- LF: 285.5 lbs
- RF: 292.3 lbs
- LR: 278.1 lbs
- RR: 295.8 lbs

## Serial Commands
- `LF` / `RF` / `LR` / `RR` - Set corner identity
- `TARE` - Zero the weight
- `STATUS` - Show current state

## Button Functions
- **BOOT button**: Tare (zero weight)

## BLE Advertisement
- Device name: `Scale-LF` (or RF, LR, RR based on config)
- Updates every 500ms when connected

## Data Format

### Weight (CRITICAL - Binary!)
Weight is sent as **Float32LE binary** (NOT a string):
```cpp
float weight = 285.5;
pWeightChar->setValue((uint8_t*)&weight, sizeof(float));
pWeightChar->notify();
```

Mobile app expects 4-byte IEEE 754 float in little-endian format.

### Status JSON
```json
{
  "weight": 285.5,
  "stable": true,
  "battery": 85
}
```

## Weight Settling Simulation
After tare:
- First 3 seconds: Variance ±0.5 lbs (settling)
- After 3 seconds: Variance ±0.1 lbs (stable)

## Multi-Scale Setup

To test complete 4-corner system:

1. **Program 4 ESP32s** with this firmware
2. **Configure each corner** via serial before first use:
   - ESP32 #1: Send `LF`
   - ESP32 #2: Send `RF`
   - ESP32 #3: Send `LR`
   - ESP32 #4: Send `RR`
3. **Restart all ESP32s** to update BLE names
4. **Power all 4 simultaneously** (USB power banks work)
5. **Open mobile app**, scan should show:
   - Scale-LF
   - Scale-RF
   - Scale-LR
   - Scale-RR
6. **Connect to all 4** (app supports multi-connection)

## Testing Checklist

- [ ] Flash firmware successfully
- [ ] Set corner ID via serial
- [ ] Restart ESP32
- [ ] BLE device name shows correct corner (e.g., "Scale-LF")
- [ ] Mobile app scan finds device
- [ ] Connect successfully
- [ ] Weight appears in app (250-400 lbs range)
- [ ] Weight updates every 500ms
- [ ] Tare button zeros weight
- [ ] Weight settling animation works (3 seconds)
- [ ] Battery percentage shows (85%)
- [ ] Temperature reading appears (~72°F)
- [ ] Disconnect/reconnect works
- [ ] Can connect to multiple scales simultaneously

## Troubleshooting

**Device name doesn't change after serial config**
- Must restart ESP32 for BLE name to update
- Check serial output confirms: "Saved corner: XX to NVS"

**Mobile app shows string instead of number**
- Weight MUST be Float32LE binary, not ASCII string
- Check pWeightChar->setValue() uses `(uint8_t*)&weight, sizeof(float)`

**Weight not updating**
- Check BLE connection status in serial monitor
- Updates only sent when `deviceConnected == true`

**Multiple scales interfere**
- This is normal - each scale is independent
- Mobile app should scan and connect to all 4 separately
- Ensure each has unique corner ID

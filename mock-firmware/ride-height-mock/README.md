# Ride Height BLE Mock Firmware

Mock firmware for testing mobile app dual ultrasonic sensor ride height measurement.

## Hardware Required
- ESP32 dev board
- USB cable

## UUIDs
- Service: `4fafc201-0003-459e-8fcc-c5c9c331914b`
- Height Data: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (CSV string, notify)
- Command: `beb5483e-36e1-4688-b7f5-ea07361b26a9` (Write)

## Build & Upload

```bash
cd mock-firmware/ride-height-mock
pio run --target upload
pio device monitor
```

## BLE Advertisement
- Device name: `RideHeight`
- Data sent on command or continuous mode

## Serial Commands
- `R` - Single reading
- `C` - Start continuous mode (500ms updates)
- `S` - Stop continuous mode
- `Z` - Zero sensors
- `STATUS` - Show current state

## Button Functions
- **BOOT button**: Single reading

## Data Format

### Height Data (CSV String)
```
S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85
```

Fields:
- `S1`: Sensor 1 reading (mm)
- `S2`: Sensor 2 reading (mm)
- `AVG`: Average of both sensors (mm)
- `IN`: Average converted to inches
- `BAT`: Battery voltage (V)

### Commands (Write Single Char)
- `'R'` or `'r'` - Single reading
- `'C'` or `'c'` - Continuous mode
- `'S'` or `'s'` - Stop continuous
- `'Z'` or `'z'` - Zero sensors

## Sensor Simulation

### Normal Operation
- Sensor 1: 120-130mm (typical ride height range)
- Sensor 2: 120-130mm
- Variance: ±0.5mm per reading
- Sensors stay within 5mm of each other

### Sensor Delta Warning
If sensors differ by >5mm:
- Warning printed to serial
- Indicates misalignment or fault
- Sensors auto-correct to stay realistic

### Continuous Mode
When enabled:
- Sends reading every 500ms
- Simulates real-time ride height monitoring
- Useful for testing suspension changes

## Testing Checklist

- [ ] Flash firmware successfully
- [ ] BLE device "RideHeight" appears in scan
- [ ] Connect successfully
- [ ] Send 'R' command via app
- [ ] Receive CSV data notification
- [ ] Start continuous mode ('C')
- [ ] Receive updates every 500ms
- [ ] Stop continuous mode ('S')
- [ ] Zero sensors ('Z')
- [ ] Verify S1 and S2 reset to 0.0mm
- [ ] Disconnect/reconnect works

## Expected Data Flow

### Single Reading Mode (Default)
```
App sends: 'R'
ESP32 responds: "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"
App waits for next command
```

### Continuous Mode
```
App sends: 'C'
ESP32 starts streaming every 500ms:
  "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"
  "S1:123.6,S2:125.3,AVG:124.4,IN:4.90,BAT:3.85"
  "S1:123.2,S2:124.9,AVG:124.0,IN:4.88,BAT:3.85"
  ...
App sends: 'S' to stop
```

## Troubleshooting

**No data received after 'R' command**
- Check BLE connection is active
- Verify command characteristic has WRITE property
- Check serial monitor shows "Command received: 'R'"

**Continuous mode doesn't stop**
- Send 'S' command explicitly
- Disconnect will also stop (auto-reset)

**Sensors show unrealistic values**
- Firmware constrains to 100-150mm range
- If seeing 0.0mm, sensors were zeroed - send 'R' to regenerate

**Battery voltage dropping to 0V**
- Simulated battery drain is very slow
- Reset ESP32 to restore to 3.85V

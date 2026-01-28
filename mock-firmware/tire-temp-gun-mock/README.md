# Tire Temperature Gun BLE Mock Firmware

Mock firmware for testing mobile app IR temperature gun interface with spot/continuous modes.

## Hardware Required
- ESP32 dev board
- USB cable

## UUIDs
- Service: `4fafc201-0005-459e-8fcc-c5c9c331914b`
- Temperature: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (JSON)
- Command: `beb5483e-36e1-4688-b7f5-ea07361b26a9` (Write string)

## Build & Upload

```bash
cd mock-firmware/tire-temp-gun-mock
pio run --target upload
pio device monitor
```

## BLE Advertisement
- Device name: `TireTempGun`
- Updates every 2 seconds (spot mode) or 1 second (continuous mode)

## Serial Commands
- `READ` or `R` - Take new reading
- `RESET` - Reset session max/min
- `EMIT <val>` - Set emissivity (0.1-1.0), e.g., `EMIT 0.95`
- `F` / `C` - Set unit to Fahrenheit or Celsius
- `SPOT` - Set spot mode
- `CONT` - Set continuous mode
- `STATUS` - Show current state

## Button Functions
- **BOOT button**: Take new reading

## Data Format

### Temperature JSON
```json
{
  "temp": 185.5,
  "amb": 72.3,
  "max": 195.2,
  "min": 175.8,
  "bat": 85,
  "mode": 0
}
```

Fields:
- `temp`: Current reading (°F or °C based on unit setting)
- `amb`: Ambient temperature
- `max`: Session maximum
- `min`: Session minimum
- `bat`: Battery percentage (0-100)
- `mode`: 0 = spot mode, 1 = continuous mode

### Commands (Write String)
- `"EMIT:0.95"` - Set emissivity (0.1-1.0)
- `"UNIT:F"` - Set unit to Fahrenheit
- `"UNIT:C"` - Set unit to Celsius
- `"RESET"` - Reset session max/min to current reading
- `"MODE:SPOT"` - Set spot mode
- `"MODE:CONTINUOUS"` - Set continuous mode

## Operating Modes

### Spot Mode (mode: 0)
- Takes reading on button press or trigger
- Sends update every 2 seconds
- Good for individual tire measurements

### Continuous Mode (mode: 1)
- Constantly streams readings
- Sends update every 1 second
- Useful for scanning across tire surface

## Emissivity Effect

Different materials require different emissivity settings:
- Rubber (tire): 0.95 (default)
- Metal (brake rotor): 0.10-0.20
- Paint: 0.90-0.95

Temperature reading is multiplied by emissivity:
```
Displayed Temp = Measured Temp × Emissivity
```

## Temperature Ranges

- Tire surface: 150-220°F (65-105°C)
- Ambient: 65-85°F (18-29°C)
- Session max/min track extremes

## Testing Checklist

- [ ] Flash firmware successfully
- [ ] BLE device "TireTempGun" appears in scan
- [ ] Connect successfully
- [ ] Receive JSON temperature data
- [ ] Press button to generate new reading
- [ ] Verify max/min track properly
- [ ] Send "RESET" command
- [ ] Verify max/min reset to current temp
- [ ] Send "EMIT:0.50" command
- [ ] Verify temp reading changes (halved)
- [ ] Send "UNIT:C" command
- [ ] Verify temp converts to Celsius
- [ ] Send "MODE:CONTINUOUS" command
- [ ] Verify updates increase to 1/sec
- [ ] Send "MODE:SPOT" command
- [ ] Verify updates return to 2/sec
- [ ] Disconnect/reconnect works

## Expected Behavior

### Spot Mode Example
```
Time    Temp      Max      Min      Action
----    ----      ---      ---      ------
0s      185.5°F   185.5°F  185.5°F  Initial
5s      185.5°F   185.5°F  185.5°F  (no change)
10s     192.3°F   192.3°F  185.5°F  Button pressed (new reading)
15s     192.3°F   192.3°F  185.5°F  (no change)
20s     178.1°F   192.3°F  178.1°F  Button pressed (new reading)
```

### Continuous Mode Example
```
Time    Temp      Mode
----    ----      ----
0s      185.5°F   Spot (2s updates)
5s      "MODE:CONTINUOUS" sent
6s      188.2°F   Continuous (1s updates)
7s      191.4°F   Continuous
8s      189.7°F   Continuous
9s      "MODE:SPOT" sent
11s     189.7°F   Spot (2s updates)
```

### Emissivity Example
```
Emissivity  Measured  Displayed  Material
----------  --------  ---------  --------
0.95        200°F     190.0°F    Rubber tire
0.50        200°F     100.0°F    Polished metal
0.20        200°F     40.0°F     Shiny brake rotor
1.00        200°F     200.0°F    Perfect black body
```

## Troubleshooting

**Temperature doesn't change**
- In spot mode, temp only changes on button press or READ command
- In continuous mode, temp generates new value every second
- Check mode value in JSON output

**Emissivity command not working**
- Must send as string: "EMIT:0.95"
- Value must be 0.1-1.0
- Check serial monitor for confirmation

**Max/min values wrong**
- Use RESET command to clear session history
- Max/min only update when new reading exceeds current extremes

**Unit conversion wrong**
- F→C formula: (°F - 32) × 5/9
- Check unit field in JSON matches expected

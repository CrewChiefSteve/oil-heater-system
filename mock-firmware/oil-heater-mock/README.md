# Oil Heater BLE Mock Firmware

Mock firmware for testing mobile app oil heater controller interface.

## Hardware Required
- ESP32 dev board
- USB cable

## UUIDs
- Service: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- Temperature: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (ASCII string)
- Setpoint: `beb5483e-36e1-4688-b7f5-ea07361b26a9` (ASCII string, read/write)
- Status: `beb5483e-36e1-4688-b7f5-ea07361b26aa` (JSON)

## Build & Upload

```bash
cd mock-firmware/oil-heater-mock
pio run --target upload
pio device monitor
```

## BLE Advertisement
- Device name: `OilHeater`
- Updates every 1 second when connected

## Serial Commands
- `SET <temp>` - Set target temperature (100-250°F), e.g., `SET 180`
- `FAULT` - Toggle safety shutdown
- `SENSOR` - Toggle sensor error
- `STATUS` - Show current state

## Button Functions
- **BOOT button**: Toggle safety shutdown

## Data Format

### Temperature (ASCII String)
```
"185.5"
```

### Setpoint (ASCII String)
```
"180.0"
```

### Status JSON (EXACT format required)
```json
{
  "heater": true,
  "safetyShutdown": false,
  "sensorError": false
}
```

## Heating Simulation

Starting from 70°F, setpoint 180°F:

1. **Heating phase**: Temp rises ~2°F/second when heater ON
2. **Control logic**:
   - Turn ON when: `temp < setpoint - 5°F`
   - Turn OFF when: `temp > setpoint + 2°F`
3. **Cooling phase**: Temp drops ~0.5°F/second when heater OFF
4. **Cycle**: Maintains ±5°F around setpoint

## Safety Features

### Over-Temperature Shutdown
- Triggers at 290°F
- Heater forced OFF
- `safetyShutdown: true` in status JSON
- Clear by changing setpoint

### Sensor Error
- Simulated via serial `SENSOR` command or mobile app
- Heater forced OFF when active
- `sensorError: true` in status JSON

## Testing Checklist

- [ ] Flash firmware successfully
- [ ] BLE device "OilHeater" appears in scan
- [ ] Connect successfully
- [ ] Temperature starts at ~70°F
- [ ] Temperature rises when heater ON
- [ ] Heater cycles around setpoint
- [ ] Write new setpoint via BLE (app UI)
- [ ] Temperature adjusts to new setpoint
- [ ] Trigger fault via button
- [ ] Status shows `safetyShutdown: true`
- [ ] Heater turns OFF during fault
- [ ] Clear fault by adjusting setpoint
- [ ] Disconnect/reconnect works

## Expected Behavior Timeline

```
Time    Temp    Heater  Notes
----    ----    ------  -----
0s      70°F    ON      Initial state
30s     130°F   ON      Heating up
45s     160°F   ON      Approaching setpoint
55s     175°F   ON      Below threshold (180-5=175)
58s     182°F   OFF     Exceeded setpoint + 2°F
62s     180°F   OFF     Coasting down
70s     176°F   ON      Below setpoint - 5°F
...     ...     ...     Cycles around 180±5°F
```

## Troubleshooting

**Temperature not changing**
- Check heater state in serial output
- Verify setpoint is different from current temp
- Check for safety shutdown active

**Can't write setpoint**
- Ensure characteristic has WRITE property
- Check setpoint is in range 100-250°F
- Mobile app must send ASCII string, not binary

**Status JSON format wrong**
- Must match EXACT format: `{"heater":true,"safetyShutdown":false,"sensorError":false}`
- Boolean values must be lowercase `true`/`false`, not "true"/"false" strings

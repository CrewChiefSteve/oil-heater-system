# Tire Temperature Probe BLE Mock Firmware

Mock firmware for testing mobile app sequential corner tire/brake temperature workflow.

## Hardware Required
- ESP32 dev board
- USB cable

## UUIDs
- Service: `4fafc201-0004-459e-8fcc-c5c9c331914b`
- Tire Data: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (3x Float32LE binary)
- Brake Data: `beb5483e-36e1-4688-b7f5-ea07361b26a9` (Float32LE binary)
- System Status: `beb5483e-36e1-4688-b7f5-ea07361b26aa` (JSON)
- Config: `beb5483e-36e1-4688-b7f5-ea07361b26ab` (JSON)
- Corner Reading: `beb5483e-36e1-4688-b7f5-ea07361b26ac` (JSON)

## Build & Upload

```bash
cd mock-firmware/tire-temp-probe-mock
pio run --target upload
pio device monitor
```

## BLE Advertisement
- Device name: `TireProbe`
- Sequential corner workflow: RF → LF → LR → RR

## Serial Commands
- `READ` or `R` - Trigger corner reading and advance
- `RESET` - Reset to RF corner
- `STATUS` - Show current state

## Button Functions
- **BOOT button**: Trigger corner reading and auto-advance

## Data Format

### Tire Data (Binary - 12 bytes)
3x Float32LE (inside, middle, outside):
```cpp
float tireData[3] = {185.5, 192.3, 188.1};
pTireChar->setValue((uint8_t*)tireData, 12);
```

### Brake Data (Binary - 4 bytes)
Float32LE brake temperature:
```cpp
float brakeTemp = 378.5;
pBrakeChar->setValue((uint8_t*)&brakeTemp, 4);
```

### Corner Reading JSON
```json
{
  "corner": "RF",
  "tireInside": 185.5,
  "tireMiddle": 192.3,
  "tireOutside": 188.1,
  "brakeTemp": 378.5
}
```

### System Status JSON
```json
{
  "battery": 85,
  "isCharging": false,
  "firmware": "1.0.0"
}
```

## Sequential Workflow

1. **Connect** to TireProbe
2. **Wait** for user to position probe at RF tire
3. **Trigger** reading (button or app command)
4. **Send** RF data (tire + brake temps)
5. **Auto-advance** to next corner (LF) after 3 seconds
6. **Repeat** for LF, LR, RR
7. **Session complete** after all 4 corners
8. **Loop back** to RF for next session

## Temperature Ranges

### Tire Temperatures
- Inside: 180-210°F (typically hottest due to camber)
- Middle: 185-205°F
- Outside: 175-195°F
- All temps within ±3°F of each other (realistic)

### Brake Temperature
- 300-600°F (typical race conditions)
- Higher after hard braking zones

## Testing Checklist

- [ ] Flash firmware successfully
- [ ] BLE device "TireProbe" appears in scan
- [ ] Connect successfully
- [ ] Press button to trigger RF reading
- [ ] Receive tire data (3 floats) on tire characteristic
- [ ] Receive brake temp (1 float) on brake characteristic
- [ ] Receive complete corner JSON on corner characteristic
- [ ] Verify auto-advance to LF after 3 seconds
- [ ] Complete full session (RF→LF→LR→RR)
- [ ] Verify session loops back to RF
- [ ] Check battery percentage decreases slowly
- [ ] Disconnect/reconnect works

## Expected Workflow

```
Time    Corner  Action          Output
----    ------  ------          ------
0s      RF      Connect         "Ready for corner: RF"
5s      RF      Button press    Tire: I=185.5 M=192.3 O=188.1, Brake: 378.5
8s      LF      Auto-advance    "Next corner in 3 seconds: LF"
15s     LF      Button press    Tire: I=180.2 M=187.4 O=183.6, Brake: 402.1
18s     LR      Auto-advance    "Next corner in 3 seconds: LR"
25s     LR      Button press    Tire: I=188.9 M=195.1 O=190.3, Brake: 365.8
28s     RR      Auto-advance    "Next corner in 3 seconds: RR"
35s     RR      Button press    Tire: I=192.3 M=198.7 O=194.2, Brake: 421.5
38s     RF      Auto-advance    "Session complete! Starting over at RF"
```

## Troubleshooting

**Corner doesn't advance**
- Auto-advance happens 3 seconds after reading
- Use `RESET` command to manually go back to RF

**Temperature values look wrong**
- Tire temps: 150-220°F is normal range
- Brake temps: 300-600°F is normal range
- Check mobile app expects Float32LE binary, not ASCII strings

**Battery doesn't change**
- Drains very slowly (10% chance per reading)
- Simulates long battery life

**Can't trigger reading from app**
- App should write to Command characteristic
- Check button trigger works via serial monitor first

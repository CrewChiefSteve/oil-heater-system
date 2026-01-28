# Mock Firmware Architecture

Visual reference for BLE mock firmware suite structure.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Mobile App (Race Telemetry)                  │
│                         iOS / Android                            │
└───────────┬─────────────┬─────────────┬──────────────┬──────────┘
            │             │             │              │
         BLE │          BLE│          BLE│           BLE│
            │             │             │              │
┌───────────▼─────┐   ┌───▼─────┐   ┌──▼──────┐   ┌──▼──────────┐
│  RaceScale-LF   │   │ Scale-RF│   │ Scale-LR│   │  Scale-RR   │
│   ESP32 #1      │   │ ESP32 #2│   │ ESP32 #3│   │  ESP32 #4   │
│  Weight: 285lbs │   │ Wt: 292 │   │ Wt: 278 │   │  Wt: 295    │
└─────────────────┘   └─────────┘   └─────────┘   └─────────────┘

┌───────────────────────────────────────────────────────────────────┐
│                    Or Single Device Testing                        │
└───────────┬─────────────┬─────────────┬──────────────┬───────────┘
            │             │             │              │
┌───────────▼─────┐   ┌───▼─────────┐  ┌──▼──────────┐  ┌──▼──────┐
│  Oil Heater     │   │ Tire Probe  │  │ Ride Height │  │ Tire Gun│
│   ESP32 #1      │   │  ESP32 #1   │  │  ESP32 #1   │  │ESP32 #1 │
│  180°F → 185°F  │   │ RF→LF→LR→RR │  │ 124.2mm     │  │ 185.5°F │
└─────────────────┘   └─────────────┘  └─────────────┘  └─────────┘
```

## BLE Service Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         BLE Services                             │
└──────┬──────────────┬──────────────┬──────────────┬─────────────┘
       │              │              │              │
   ┌───▼────────┐ ┌──▼──────────┐ ┌─▼─────────┐ ┌─▼──────────┐
   │ Oil Heater │ │  RaceScale  │ │Ride Height│ │ Tire Probe │
   │ & RaceScale│ │             │ │           │ │            │
   │   SHARED   │ │             │ │           │ │            │
   │   UUID:    │ │             │ │  UUID:    │ │  UUID:     │
   │   -914b    │ │             │ │  -0003-   │ │  -0004-    │
   └────────────┘ └─────────────┘ └───────────┘ └────────────┘

   Different characteristics distinguish services
```

### Service UUID Map

```
Service                UUID (last 4 chars)    Device Count
-----------------------------------------------------------------
Oil Heater            ...-914b                1
RaceScale             ...-914b                4 (LF,RF,LR,RR)
Ride Height           -0003-...               1
Tire Probe            -0004-...               1
Tire Gun              -0005-...               1
```

## Data Flow Architecture

### RaceScale (Most Complex - 4 Devices)

```
ESP32 #1 (LF)                    ESP32 #2 (RF)
┌─────────────┐                  ┌─────────────┐
│ Weight: 285 │──BLE notify──►   │ Weight: 292 │──BLE notify──►
│ Battery: 85%│   500ms          │ Battery: 85%│   500ms
│ Temp: 72°F  │                  │ Temp: 72°F  │
│ Corner: LF  │                  │ Corner: RF  │
└─────────────┘                  └─────────────┘
       │                                │
       │                                │
       └────────────┬───────────────────┘
                    │
                    ▼
          ┌──────────────────┐
          │   Mobile App      │
          │ ┌───┬───┬───┬───┐│
          │ │LF │RF │LR │RR ││
          │ │285│292│278│295││
          │ └───┴───┴───┴───┘│
          └──────────────────┘
```

### Oil Heater (Simplest - Bidirectional)

```
ESP32                           Mobile App
┌──────────────────┐           ┌──────────────────┐
│                  │──temp──►  │                  │
│  Current: 185°F  │  1000ms   │  Display:        │
│  Setpoint: 180°F │           │  185°F / 180°F   │
│  Heater: ON      │◄─setpoint─│                  │
│                  │  (write)  │  [User adjusts]  │
└──────────────────┘           └──────────────────┘
```

### Tire Probe (Sequential Workflow)

```
Time: 0s              5s                  10s                 15s
State: RF            LF                  LR                  RR
       │             │                   │                   │
       ▼             ▼                   ▼                   ▼
   ┌───────┐     ┌───────┐          ┌───────┐          ┌───────┐
   │ Take  │     │ Take  │          │ Take  │          │ Take  │
   │Reading│────►│Reading│─────────►│Reading│─────────►│Reading│
   │       │  3s │       │   3s     │       │   3s     │       │
   └───┬───┘     └───┬───┘          └───┬───┘          └───┬───┘
       │             │                  │                  │
       │ Tire: I/M/O │                  │                  │
       │ Brake: temp │                  │                  │
       ▼             ▼                  ▼                  ▼
   ┌────────────────────────────────────────────────────────────┐
   │                    Mobile App                              │
   │  Session: RF(done) → LF(done) → LR(done) → RR(done)       │
   └────────────────────────────────────────────────────────────┘
```

## File Structure

```
mock-firmware/
│
├── README.md              ─┐
├── TESTING_GUIDE.md        │
├── SUMMARY.md              ├─ Documentation (4 files)
├── ARCHITECTURE.md        ─┘
│
├── racescale-mock/        ─┐
│   ├── platformio.ini      │
│   ├── README.md           ├─ Project 1 (3 files)
│   └── src/                │
│       └── main.cpp       ─┘
│
├── oil-heater-mock/       ─┐
│   ├── platformio.ini      │
│   ├── README.md           ├─ Project 2 (3 files)
│   └── src/                │
│       └── main.cpp       ─┘
│
├── ride-height-mock/      ─┐
│   ├── platformio.ini      │
│   ├── README.md           ├─ Project 3 (3 files)
│   └── src/                │
│       └── main.cpp       ─┘
│
├── tire-temp-probe-mock/  ─┐
│   ├── platformio.ini      │
│   ├── README.md           ├─ Project 4 (3 files)
│   └── src/                │
│       └── main.cpp       ─┘
│
└── tire-temp-gun-mock/    ─┐
    ├── platformio.ini      │
    ├── README.md           ├─ Project 5 (3 files)
    └── src/                │
        └── main.cpp       ─┘

Total: 19 files
```

## State Machine: Oil Heater Example

```
                    ┌──────────────┐
                    │   STARTUP    │
                    │  Temp: 70°F  │
                    │ Setpoint: 180│
                    └──────┬───────┘
                           │
                           ▼
                    ┌──────────────┐
           ┌────────┤   HEATING    │
           │        │  Heater: ON  │
           │        │ Temp rising  │
           │        └──────┬───────┘
           │               │ temp > setpoint+2°F
           │               ▼
           │        ┌──────────────┐
           │        │   COOLING    │
           │        │  Heater: OFF │
           │        │ Temp falling │
           │        └──────┬───────┘
           │               │ temp < setpoint-5°F
           └───────────────┘

    Button press or over-temp
           │
           ▼
    ┌──────────────┐
    │    FAULT     │
    │ Heater: OFF  │
    │ Safety: YES  │
    └──────┬───────┘
           │ User adjusts setpoint
           │ AND conditions safe
           ▼
    [Return to HEATING/COOLING]
```

## Memory Architecture

```
ESP32 Memory Layout (Typical)
┌─────────────────────────────────────────┐
│ Flash (4MB)                              │ 100%
├─────────────────────────────────────────┤
│ Firmware + Libraries  ░░░░░░░░          │ 600KB (15%)
│ NimBLE Stack         ░░░                │ 200KB (5%)
│ ArduinoJson          ░                  │ 50KB (1%)
│ ESP32 Core           ░░░                │ 150KB (4%)
│ Free                                    │ 3MB (75%)
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│ RAM (520KB)                              │ 100%
├─────────────────────────────────────────┤
│ BLE Buffers          ░░░░░░             │ 100KB (19%)
│ Variables            ░░                 │ 30KB (6%)
│ Stack                ░░                 │ 40KB (8%)
│ Heap (dynamic)       ░░░                │ 60KB (12%)
│ Free                                    │ 290KB (55%)
└─────────────────────────────────────────┘
```

## Communication Protocol Layers

```
Layer 7: Application
        ├─ RaceScale: Weight monitoring
        ├─ Oil Heater: Temperature control
        ├─ Ride Height: Suspension measurement
        ├─ Tire Probe: Sequential corner capture
        └─ Tire Gun: IR temperature reading

Layer 6: Data Format
        ├─ Float32LE (4 bytes)
        ├─ ASCII String ("123.4")
        ├─ JSON ({"key": value})
        └─ CSV (S1:123.4,S2:125.1)

Layer 5: BLE GATT
        ├─ Services (5 unique UUIDs)
        ├─ Characteristics (Read/Write/Notify)
        └─ Descriptors (optional)

Layer 4: BLE L2CAP
        ├─ Connection management
        ├─ MTU negotiation (default 23 bytes)
        └─ Flow control

Layer 3: BLE Link Layer
        ├─ Advertising (connectable)
        ├─ Connection parameters
        └─ Channel hopping (37-39)

Layer 2: BLE PHY
        ├─ 2.4 GHz ISM band
        ├─ GFSK modulation
        └─ 1 Mbps data rate

Layer 1: Hardware
        └─ ESP32 Bluetooth radio
```

## Update Rate Timing Diagram

```
Time (ms):  0    500   1000  1500  2000  2500  3000
            │     │     │     │     │     │     │
RaceScale:  │─────┼─────┼─────┼─────┼─────┼─────┼  (500ms)
            ▼     ▼     ▼     ▼     ▼     ▼     ▼

Oil Heater: │───────────┼───────────┼───────────┼  (1000ms)
            ▼           ▼           ▼           ▼

Ride Height:│─────┼─────┼─────┼─────┼─────┼─────┼  (500ms continuous)
(Continuous)▼     ▼     ▼     ▼     ▼     ▼     ▼

Tire Gun:   │─────────────────┼─────────────────┼  (2000ms spot mode)
(Spot Mode) ▼                 ▼                 ▼

Tire Gun:   │─────────┼─────────┼─────────┼─────┼  (1000ms continuous)
(Cont Mode) ▼         ▼         ▼         ▼     ▼

Tire Probe: │[trigger]────────────[trigger]─────┼  (on demand)
            ▼                     ▼              ▼
```

## Power State Diagram

```
┌─────────────────────────────────────────────────────┐
│                  USB Connected                       │
│                    (always)                          │
└──────────────────┬──────────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
    ▼              ▼              ▼
┌────────┐   ┌──────────┐   ┌─────────┐
│ IDLE   │   │ADVERTISING│  │CONNECTED│
│ ~60mA  │──►│  ~80mA    │─►│ ~100mA  │
└────────┘   └──────────┘   └─────────┘
    ▲              ▲              │
    │              │              │
    └──────────────┴──────────────┘
         (disconnect/timeout)

Battery simulation only affects reported
battery % in BLE data, not actual power draw.
```

## Data Type Size Reference

```
Type            Size    Example           Use Case
────────────────────────────────────────────────────────
Float32LE       4 bytes 285.5 lbs        RaceScale weight
Float32LE×3     12 bytes[185.5,192.3...] Tire probe 3-pt
ASCII String    N bytes "185.5"          Oil heater temp
JSON            N bytes {"heater":true}  Status messages
CSV             N bytes "S1:123.4,..."   Ride height data
UInt8           1 byte  85 (percent)     Battery level
Char            1 byte  'R' (command)    Single commands
String          N bytes "LF" or "RESET"  Corner ID or cmds
```

## Testing Flow Diagram

```
┌────────────┐
│ Flash ESP32│
└─────┬──────┘
      │
      ▼
┌─────────────┐
│ Serial Boot │──┐
│ Message OK? │  │
└─────┬───────┘  │
      │YES       │NO
      ▼          │
┌─────────────┐  │
│BLE Advertise│  │
│  Starts?    │  │
└─────┬───────┘  │
      │YES       │NO
      ▼          │
┌─────────────┐  │
│Mobile App   │  │
│   Scan      │  │
└─────┬───────┘  │
      │Found     │Not Found
      ▼          │
┌─────────────┐  │
│  Connect    │  │
│  Success?   │  │
└─────┬───────┘  │
      │YES       │NO──────────┐
      ▼          ▼            │
┌─────────────┐ ┌──────────┐ │
│ Data Appears│ │Troubleshoot│ │
│  Correct?   │ │  (README) ├─┘
└─────┬───────┘ └──────────┘
      │YES
      ▼
┌─────────────┐
│Test Commands│
│  & Buttons  │
└─────┬───────┘
      │
      ▼
┌─────────────┐
│   PASS!     │
└─────────────┘
```

## Multi-Scale Connection Pattern (RaceScale)

```
Mobile App                  ESP32 Devices
┌──────────┐               ┌──────────┐
│          │◄─────BLE─────►│ Scale-LF │
│          │   Connection 1│          │
│          │               └──────────┘
│          │               ┌──────────┐
│  Race    │◄─────BLE─────►│ Scale-RF │
│  Scale   │   Connection 2│          │
│   App    │               └──────────┘
│          │               ┌──────────┐
│          │◄─────BLE─────►│ Scale-LR │
│          │   Connection 3│          │
│          │               └──────────┘
│          │               ┌──────────┐
│          │◄─────BLE─────►│ Scale-RR │
│          │   Connection 4│          │
└──────────┘               └──────────┘

Each connection independent:
- Separate BLE connection handle
- Independent notifications (500ms each)
- Individual tare/battery/temp
- Identified by Corner ID characteristic
```

## Key Design Decisions

### Why NimBLE over ESP32 BLE?
```
Feature              NimBLE    ESP32 BLE
──────────────────────────────────────────
Memory footprint     Lower     Higher
Multi-connection     Better    Limited
Stability            Stable    Issues in 3.x
Community support    Active    Moderate
Mobile compatibility Excellent Good

Decision: NimBLE for lower memory and
better multi-device support (RaceScale needs 4).
```

### Why Mock Data vs Fixed Values?
```
Fixed Values:
  Weight: 285.0 lbs (always)

Mock Data:
  Weight: 285.23, 285.67, 284.89... (varies ±0.5)

Benefit: Tests app's ability to handle
real-time updates and variance, catches
UI refresh bugs.
```

### Why Serial + Button Control?
```
Serial Only:
  - Need computer connected
  - Can't test standalone

Button Only:
  - Limited commands
  - Hard to configure

Both:
  ✓ Rapid testing with serial
  ✓ Standalone demo with button
  ✓ Best of both worlds
```

---

**Architecture Notes**:
- All firmware uses **single-file** design (main.cpp only)
- No external dependencies beyond NimBLE + ArduinoJson
- State machines kept simple (oil heater most complex)
- Update rates tuned for mobile app refresh (avoid flooding)
- Memory footprint optimized for ESP32 classic (not S3)

**See Also**:
- README.md - Quick start guide
- TESTING_GUIDE.md - Step-by-step testing
- SUMMARY.md - Project overview
- Individual project READMEs - Device specifics

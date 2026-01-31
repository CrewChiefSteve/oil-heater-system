#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// Device operating states - Sequential capture workflow
enum DeviceState {
    STATE_INITIALIZING,         // Startup, hardware init
    STATE_WAITING_CONNECTION,   // BLE advertising, waiting for app connection
    STATE_CORNER_RF,            // Ready for Right Front tire
    STATE_STABILIZING_RF,       // RF temps detected, waiting for stability
    STATE_CAPTURED_RF,          // RF captured, displaying confirmation
    STATE_CORNER_LF,            // Ready for Left Front tire
    STATE_STABILIZING_LF,       // LF temps detected, waiting for stability
    STATE_CAPTURED_LF,          // LF captured, displaying confirmation
    STATE_CORNER_LR,            // Ready for Left Rear tire
    STATE_STABILIZING_LR,       // LR temps detected, waiting for stability
    STATE_CAPTURED_LR,          // LR captured, displaying confirmation
    STATE_CORNER_RR,            // Ready for Right Rear tire
    STATE_STABILIZING_RR,       // RR temps detected, waiting for stability
    STATE_CAPTURED_RR,          // RR captured, displaying confirmation
    STATE_SESSION_COMPLETE,     // All 4 corners captured
    STATE_ERROR                 // Hardware fault detected
};

// Vehicle corner identification - v2: Explicit UInt8 values for BLE CORNER_ID characteristic
enum Corner {
    CORNER_LF = 0,  // Left Front (v2: matches BLE protocol UInt8 value 0)
    CORNER_RF = 1,  // Right Front (v2: matches BLE protocol UInt8 value 1)
    CORNER_LR = 2,  // Left Rear (v2: matches BLE protocol UInt8 value 2)
    CORNER_RR = 3   // Right Rear (v2: matches BLE protocol UInt8 value 3)
};

// Individual probe data structure
struct ProbeData {
    float temperature;      // Current temperature (Celsius)
    float avgTemperature;   // Smoothed average temperature
    bool isValid;           // Sensor reading valid
    bool isStable;          // Temperature stabilized
    uint8_t errorCount;     // Consecutive error count
    uint32_t lastReadTime;  // Timestamp of last read (ms)

    ProbeData() :
        temperature(0.0),
        avgTemperature(0.0),
        isValid(false),
        isStable(false),
        errorCount(0),
        lastReadTime(0) {}
};

// Tire channel data (3 probes: inside, middle, outside)
struct TireChannel {
    ProbeData inside;
    ProbeData middle;
    ProbeData outside;
    float averageTemp;      // Average across all 3 probes

    TireChannel() : averageTemp(0.0) {}
};

// Brake channel data (single probe)
struct BrakeChannel {
    ProbeData rotor;

    BrakeChannel() {}
};

// Complete measurement dataset
struct MeasurementData {
    TireChannel tire;
    BrakeChannel brake;
    Corner corner;
    uint32_t timestamp;

    MeasurementData() :
        corner(CORNER_RF),
        timestamp(0) {}
};

// System status structure
struct SystemStatus {
    DeviceState state;
    uint8_t batteryPercent;
    float batteryVoltage;
    bool charging;
    uint32_t uptimeMs;

    SystemStatus() :
        state(STATE_INITIALIZING),
        batteryPercent(0),
        batteryVoltage(0.0),
        charging(false),
        uptimeMs(0) {}
};

// Corner reading structure - Single corner capture data
struct CornerReading {
    Corner corner;          // Which corner this reading is from
    float tireInside;       // Inside tire temp (Celsius)
    float tireMiddle;       // Middle tire temp (Celsius)
    float tireOutside;      // Outside tire temp (Celsius)
    float brakeTemp;        // Brake rotor temp (Celsius)
    float tireAverage;      // Calculated average of 3 tire temps
    float tireSpread;       // max - min of 3 tire temps
    uint32_t timestamp;     // millis() at capture time

    CornerReading() :
        corner(CORNER_RF),
        tireInside(0.0),
        tireMiddle(0.0),
        tireOutside(0.0),
        brakeTemp(0.0),
        tireAverage(0.0),
        tireSpread(0.0),
        timestamp(0) {}
};

// Session data structure - Complete 4-corner measurement session
struct SessionData {
    CornerReading corners[4];   // RF, LF, LR, RR in sequence
    uint8_t capturedCount;      // Number of corners captured (0-4)
    bool isComplete;            // True when all 4 corners captured

    SessionData() :
        capturedCount(0),
        isComplete(false) {}
};

#endif // TYPES_H

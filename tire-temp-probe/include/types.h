#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// Device operating states
enum DeviceState {
    STATE_INITIALIZING,     // Startup, hardware init
    STATE_IDLE,             // No active measurement
    STATE_MEASURING,        // Actively reading temperatures
    STATE_TRANSMITTING,     // Sending BLE data
    STATE_LOW_BATTERY,      // Battery below threshold
    STATE_ERROR             // Hardware fault detected
};

// Vehicle corner identification
enum Corner {
    CORNER_FL,  // Front Left
    CORNER_FR,  // Front Right
    CORNER_RL,  // Rear Left
    CORNER_RR   // Rear Right
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
        corner(CORNER_FL),
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

#endif // TYPES_H

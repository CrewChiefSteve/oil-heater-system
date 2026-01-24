#ifndef PROBES_H
#define PROBES_H

#include <Arduino.h>
#include "types.h"

// Initialize all thermocouple probes
void probesInit();

// Read all probes and update measurement data
void probesUpdate(MeasurementData &data);

// Read individual probe
bool readProbe(uint8_t csPin, ProbeData &probe);

// Check if temperature reading is valid
bool isTemperatureValid(float temp);

// Update smoothed average temperature
void updateMovingAverage(ProbeData &probe);

// Check if probe temperature is stable
bool checkStability(ProbeData &probe);

// Calculate tire average temperature from 3 probes
float calculateTireAverage(const TireChannel &tire);

#endif // PROBES_H

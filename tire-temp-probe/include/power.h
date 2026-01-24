#ifndef POWER_H
#define POWER_H

#include <Arduino.h>
#include "types.h"

// Initialize power management
void powerInit();

// Update battery status
void powerUpdate(SystemStatus &status);

// Read battery voltage
float powerReadVoltage();

// Calculate battery percentage
uint8_t powerCalculatePercent(float voltage);

// Check if charging
bool powerIsCharging();

#endif // POWER_H

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>
#include "types.h"

// Initialize BLE service
void bleInit(const char* deviceName);

// Start BLE advertising
void bleStartAdvertising();

// Stop BLE advertising
void bleStopAdvertising();

// Check if a client is connected
bool bleIsConnected();

// Transmit tire temperature data
void bleTransmitTireData(const TireChannel &tire, Corner corner, uint32_t timestamp);

// Transmit brake temperature data
void bleTransmitBrakeData(const BrakeChannel &brake, Corner corner, uint32_t timestamp);

// Transmit system status
void bleTransmitSystemStatus(const SystemStatus &status);

// Get current corner assignment
Corner bleGetCorner();

// Update BLE service (call in loop)
void bleUpdate();

#endif // BLE_SERVICE_H

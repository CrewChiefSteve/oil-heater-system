#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>
#include "types.h"

// Initialize BLE service with corner ID for dynamic device naming
void bleInit(const char* deviceName, uint8_t cornerID);

// Start advertising
void bleStartAdvertising();

// Check connection status
bool bleIsConnected();

// Transmit corner reading (v2: JSON format with temp1/temp2/temp3)
void bleTransmitCornerReading(const CornerReading& reading);

// Transmit system status (v2: JSON format with batteryLow/sensorError/probeConnected)
void bleTransmitSystemStatus(const SystemStatus& status);

// Update BLE service (call in loop)
void bleUpdate();

// Get corner ID string from UInt8 value (0=LF, 1=RF, 2=LR, 3=RR)
const char* getCornerString(uint8_t cornerID);

#endif // BLE_SERVICE_H

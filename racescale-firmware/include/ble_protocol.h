#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <Arduino.h>

// ================================================================
// BLE UUIDS - CrewChiefSteve Standard
// ================================================================

// Service UUID - MUST match @crewchiefsteve/ble package
// See packages/ble/src/constants/uuids.ts
#define SERVICE_UUID            "4fafc201-0002-459e-8fcc-c5c9c331914b"

// Characteristic UUIDs
#define WEIGHT_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Weight (read/notify)
#define TARE_CHAR_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Tare command (write)
#define CALIBRATION_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // Calibration (write)
#define TEMP_CHAR_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // Temperature (read/notify)
#define STATUS_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26ac"  // Status (read/notify)
#define CORNER_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26ad"  // Corner ID (read/write)

#endif // BLE_PROTOCOL_H

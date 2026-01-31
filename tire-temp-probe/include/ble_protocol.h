#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

/**
 * BLE Protocol v2 Constants - Tire Temperature Probe
 *
 * SINGLE SOURCE OF TRUTH: packages/ble/src/constants/uuids.ts
 * MUST MATCH: SERVICE_UUIDS.TIRE_TEMP_PROBE, TIRE_TEMP_PROBE_CHARS
 *
 * Mobile apps import from @crewchiefsteve/ble package.
 * Firmware copies values exactly from package - NEVER define locally.
 *
 * Last verified: 2026-01-31 - v2 Protocol Compliance Update
 * See: BLE_PROTOCOL_REFERENCE.md
 */

// Service UUID
// packages/ble: SERVICE_UUIDS.TIRE_TEMP_PROBE
#define SERVICE_UUID "4fafc201-0004-459e-8fcc-c5c9c331914b"

// Characteristics (v2 - 3 total, all JSON except CORNER_ID)
// packages/ble: TIRE_TEMP_PROBE_CHARS.*
#define CORNER_READING_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // v2: moved from 26ac to 26a8
#define STATUS_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // v2: renamed from SYSTEM_STATUS, now JSON
#define CORNER_ID_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26af"  // v2: new, UInt8 (0-3)

/*
 * BLE Packet Formats (v2 Protocol)
 *
 * CORNER_READING (26a8) - JSON format (sent on capture):
 * {
 *   "corner": "LF",        // "LF", "RF", "LR", or "RR"
 *   "temp1": 185.5,        // Outer tire temp (°F)
 *   "temp2": 190.2,        // Middle tire temp (°F)
 *   "temp3": 188.0,        // Inner tire temp (°F)
 *   "unit": "F"            // Temperature unit ("F" or "C")
 * }
 *
 * STATUS (26aa) - JSON format (sent periodically):
 * {
 *   "batteryLow": false,   // Battery below threshold
 *   "sensorError": false,  // Thermocouple error detected
 *   "probeConnected": true // All probes responding
 * }
 *
 * CORNER_ID (26af) - UInt8 format (1 byte):
 *   0 = LF (Left Front)
 *   1 = RF (Right Front)
 *   2 = LR (Left Rear)
 *   3 = RR (Right Rear)
 *
 * NOTE: All NOTIFY characteristics include BLE2902 descriptors for iOS compatibility
 */

#endif // BLE_PROTOCOL_H

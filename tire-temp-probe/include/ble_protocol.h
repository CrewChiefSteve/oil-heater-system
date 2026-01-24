#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <Arduino.h>

// BLE Service UUIDs
// Service UUID - MUST match @crewchiefsteve/ble package
// See packages/ble/src/constants/uuids.ts
#define SERVICE_UUID_TIRE_TEMP      "4fafc201-0004-459e-8fcc-c5c9c331914b"

// Characteristic UUIDs
#define CHAR_UUID_TIRE_DATA         "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Tire temps (notify)
#define CHAR_UUID_BRAKE_DATA        "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Brake temp (notify)
#define CHAR_UUID_SYSTEM_STATUS     "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // Battery, state (notify)
#define CHAR_UUID_DEVICE_CONFIG     "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // Corner assignment (read/write)

// Packet type identifiers
#define PKT_TYPE_TIRE_TEMPS         0x01
#define PKT_TYPE_BRAKE_TEMP         0x02
#define PKT_TYPE_SYSTEM_STATUS      0x03
#define PKT_TYPE_DEVICE_CONFIG      0x04

/*
 * BLE Packet Structures
 *
 * All temperatures in Celsius (float32)
 * All timestamps in milliseconds since boot (uint32_t)
 *
 * TIRE_DATA Packet (20 bytes):
 * ┌────────┬──────────┬──────────┬──────────┬─────────────┬─────────┐
 * │ Type   │ Inside   │ Middle   │ Outside  │ Timestamp   │ Corner  │
 * │ (1B)   │ (4B)     │ (4B)     │ (4B)     │ (4B)        │ (1B)    │
 * └────────┴──────────┴──────────┴──────────┴─────────────┴─────────┘
 *
 * BRAKE_DATA Packet (13 bytes):
 * ┌────────┬──────────┬─────────────┬─────────┐
 * │ Type   │ Rotor    │ Timestamp   │ Corner  │
 * │ (1B)   │ (4B)     │ (4B)        │ (1B)    │
 * └────────┴──────────┴─────────────┴─────────┘
 *
 * SYSTEM_STATUS Packet (11 bytes):
 * ┌────────┬─────────┬─────────┬─────────┬──────────┬───────────┐
 * │ Type   │ State   │ Battery │ Voltage │ Charging │ Uptime    │
 * │ (1B)   │ (1B)    │ (1B)    │ (4B)    │ (1B)     │ (4B)      │
 * └────────┴─────────┴─────────┴─────────┴──────────┴───────────┘
 *
 * DEVICE_CONFIG Packet (2 bytes):
 * ┌────────┬─────────┐
 * │ Type   │ Corner  │
 * │ (1B)   │ (1B)    │
 * └────────┴─────────┘
 */

// Packet size constants
#define PKT_SIZE_TIRE_DATA      18
#define PKT_SIZE_BRAKE_DATA     13
#define PKT_SIZE_SYSTEM_STATUS  11
#define PKT_SIZE_DEVICE_CONFIG  2

#endif // BLE_PROTOCOL_H

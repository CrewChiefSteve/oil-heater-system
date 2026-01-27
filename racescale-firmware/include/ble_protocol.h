#ifndef BLE_PROTOCOL_H
#define BLE_PROTOCOL_H

#include <Arduino.h>

/**
 * BLE Protocol Constants - RaceScale
 *
 * SINGLE SOURCE OF TRUTH: packages/ble/src/constants/
 * - uuids.ts: SERVICE_UUIDS.RACESCALE
 * - characteristics.ts: RACESCALE_CHARS
 *
 * Mobile app: mobile-racescale
 * Last verified: 2026-01-27 after BLE Protocol Audit
 * See: BLE_PROTOCOL_REFERENCE.md in monorepo root
 */

// ================================================================
// SERVICE UUID - RaceScale (0002)
// ================================================================

// ✅ UPDATED: Changed from old shared UUID (4fafc201-1fb5-...) to unique RaceScale UUID
#define SERVICE_UUID "4fafc201-0002-459e-8fcc-c5c9c331914b"

// ================================================================
// CHARACTERISTIC UUIDs
// ================================================================

/**
 * WEIGHT (26a8)
 * Properties: READ, NOTIFY
 * Format: Float32LE (4 bytes)
 * Unit: Pounds (lbs)
 * Update Rate: 4 Hz
 *
 * Usage:
 *   float weight = 123.45f;
 *   pWeightChar->setValue((uint8_t*)&weight, sizeof(float));
 *   pWeightChar->notify();
 */
#define WEIGHT_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

/**
 * CALIBRATION (26aa)
 * Properties: WRITE
 * Format: Float32LE (4 bytes) - ✅ UPDATED from String
 * Unit: Pounds (lbs)
 * Purpose: Calibrate scale with known weight
 *
 * Usage (Read from app):
 *   std::string value = pCalibrationChar->getValue();
 *   float knownWeight;
 *   memcpy(&knownWeight, value.data(), 4);
 */
#define CALIBRATION_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"

/**
 * TEMPERATURE (26ab)
 * Properties: READ, NOTIFY
 * Format: Float32LE (4 bytes) - ✅ UPDATED from String
 * Unit: Degrees Fahrenheit
 * Purpose: Load cell temperature compensation
 * Update Rate: 0.2 Hz (every 5 seconds)
 *
 * Usage:
 *   float temp = 72.5f;
 *   pTempChar->setValue((uint8_t*)&temp, sizeof(float));
 *   pTempChar->notify();
 */
#define TEMP_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"

/**
 * STATUS (26ac)
 * Properties: READ, NOTIFY
 * Format: JSON UTF-8 - ✅ UPDATED from simple String
 * Example: {"zeroed":true,"calibrated":true,"error":""}
 *
 * Fields:
 * - zeroed (bool): Scale has been tared
 * - calibrated (bool): Scale has been calibrated
 * - error (string): Error message (empty if no error)
 *
 * Usage:
 *   #include <ArduinoJson.h>
 *   StaticJsonDocument<128> doc;
 *   doc["zeroed"] = true;
 *   doc["calibrated"] = true;
 *   doc["error"] = "";
 *   String json;
 *   serializeJson(doc, json);
 *   pStatusChar->setValue(json.c_str());
 *   pStatusChar->notify();
 */
#define STATUS_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ac"

/**
 * TARE (26ad)
 * Properties: WRITE
 * Format: UInt8 (1 byte) - ✅ UPDATED from String "1"
 * Command: Write 0x01 to zero the scale
 *
 * Usage (Read from app):
 *   std::string value = pTareChar->getValue();
 *   if (value.length() > 0 && value[0] == 0x01) {
 *     performTare();
 *   }
 */
#define TARE_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ad"
#define TARE_COMMAND 0x01  // ✅ NEW: Tare command value

/**
 * BATTERY (26ae)
 * Properties: READ, NOTIFY
 * Format: UInt8 (1 byte)
 * Unit: Percentage (0-100)
 * Update Rate: 0.1 Hz (every 10 seconds)
 *
 * Usage:
 *   uint8_t batteryPercent = 85;
 *   pBatteryChar->setValue(&batteryPercent, 1);
 *   pBatteryChar->notify();
 */
#define BATTERY_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ae"

/**
 * CORNER_ID (26af)
 * Properties: READ, WRITE, NOTIFY
 * Format: UInt8 (1 byte) - ✅ UPDATED from String "LF"/"RF"/etc
 * Values: 0=LF, 1=RF, 2=LR, 3=RR
 * Purpose: Configure which corner this scale measures
 *
 * Usage (Read from app):
 *   std::string value = pCornerChar->getValue();
 *   uint8_t corner = (value.length() > 0) ? value[0] : 0;
 *
 * Usage (Write to app):
 *   uint8_t corner = CORNER_LF;
 *   pCornerChar->setValue(&corner, 1);
 *   pCornerChar->notify();
 */
#define CORNER_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26af"

// ================================================================
// CORNER IDENTIFIERS
// ================================================================

#define CORNER_LF 0  // Left Front
#define CORNER_RF 1  // Right Front
#define CORNER_LR 2  // Left Rear
#define CORNER_RR 3  // Right Rear

// Corner name strings for BLE device naming
static const char* CORNER_NAMES[] = {"LF", "RF", "LR", "RR"};

// ✅ NEW: Helper function to convert string corner ID to uint8_t
inline uint8_t cornerStringToUInt8(const String& cornerStr) {
  String upper = cornerStr;
  upper.toUpperCase();

  if (upper == "LF") return CORNER_LF;
  if (upper == "RF") return CORNER_RF;
  if (upper == "LR") return CORNER_LR;
  if (upper == "RR") return CORNER_RR;

  // Numeric string ("0", "1", "2", "3")
  int val = cornerStr.toInt();
  if (val >= 0 && val <= 3) return (uint8_t)val;

  return CORNER_LF;  // Default
}

// ✅ NEW: Helper function to convert uint8_t corner ID to string name
inline String cornerUInt8ToString(uint8_t corner) {
  if (corner >= 0 && corner <= 3) {
    return String(CORNER_NAMES[corner]);
  }
  return "LF";  // Default
}

#endif // BLE_PROTOCOL_H

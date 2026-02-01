#pragma once
// =================================================================
// CrewChiefSteve Mock Firmware — BLE Protocol v2 Configuration
// Source of truth: BLE_PROTOCOL_REFERENCE.md (2026-01-27)
// 
// DO NOT modify UUIDs here — they must match the mobile apps.
// =================================================================

#include <Arduino.h>

// ===================== SERVICE UUIDs =====================
// Pattern: 4fafc201-XXXX-459e-8fcc-c5c9c331914b
// XXXX = device ID (0001–0005)

#define SVC_OIL_HEATER    "4fafc201-0001-459e-8fcc-c5c9c331914b"
#define SVC_RACESCALE     "4fafc201-0002-459e-8fcc-c5c9c331914b"
#define SVC_RIDE_HEIGHT   "4fafc201-0003-459e-8fcc-c5c9c331914b"
#define SVC_TIRE_PROBE    "4fafc201-0004-459e-8fcc-c5c9c331914b"
#define SVC_TIRE_TEMP_GUN "4fafc201-0005-459e-8fcc-c5c9c331914b"

// Legacy UUID — DO NOT USE (v1 shared UUID)
// #define SVC_LEGACY     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// ===================== CHARACTERISTIC UUIDs =====================
// Pattern: beb5483e-36e1-4688-b7f5-ea07361b26XX
// XX = characteristic ID (a8–af)

#define CHR_26A8 "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Primary data
#define CHR_26A9 "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Secondary / command
#define CHR_26AA "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // Status (most) / Cal (scale)
#define CHR_26AB "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // Config / extra
#define CHR_26AC "beb5483e-36e1-4688-b7f5-ea07361b26ac"  // Extra
#define CHR_26AD "beb5483e-36e1-4688-b7f5-ea07361b26ad"  // Extra
#define CHR_26AE "beb5483e-36e1-4688-b7f5-ea07361b26ae"  // Extra
#define CHR_26AF "beb5483e-36e1-4688-b7f5-ea07361b26af"  // CORNER_ID (standard slot)

// ===================== CORNER IDs =====================
enum CornerId : uint8_t {
    CORNER_LF = 0,
    CORNER_RF = 1,
    CORNER_LR = 2,
    CORNER_RR = 3
};
static const char* CORNER_NAMES[] = {"LF", "RF", "LR", "RR"};

// ===================== DEVICE TYPES =====================
enum DeviceType : uint8_t {
    DEV_OIL_HEATER    = 0,
    DEV_RACESCALE     = 1,
    DEV_RIDE_HEIGHT   = 2,
    DEV_TIRE_PROBE    = 3,
    DEV_TIRE_TEMP_GUN = 4,
    DEV_COUNT         = 5
};
static const char* DEVICE_TYPE_NAMES[] = {
    "Oil Heater", "RaceScale", "Ride Height", "Tire Probe", "Tire Temp Gun"
};

// ===================== SIMULATION DEFAULTS =====================

// Oil Heater
#define SIM_AMBIENT_TEMP          72.0f    // °F
#define SIM_HEATER_SETPOINT       180.0f   // °F default
#define SIM_HEATER_MIN_SP         100.0f   // °F
#define SIM_HEATER_MAX_SP         250.0f   // °F
#define SIM_HEATER_HEAT_RATE      5.0f     // °F/sec max heating
#define SIM_HEATER_COOL_RATE      0.5f     // °F/sec cooling
#define SIM_HEATER_NOISE          0.3f     // °F noise amplitude
#define SIM_HEATER_SAFETY_TEMP    260.0f   // °F safety shutdown

// RaceScale (typical race car corner weights)
#define SIM_SCALE_LF              548.0f   // lbs
#define SIM_SCALE_RF              532.0f
#define SIM_SCALE_LR              572.0f
#define SIM_SCALE_RR              558.0f
#define SIM_SCALE_NOISE           0.15f    // lbs noise when settled
#define SIM_SCALE_SETTLE_TIME     2.0f     // seconds to settle
#define SIM_SCALE_LOAD_CELL_TEMP  75.0f    // °F ambient load cell temp

// Ride Height
#define SIM_RH_BASE_MM            124.0f   // mm
#define SIM_RH_S1_OFFSET          0.0f     // mm offset from base
#define SIM_RH_S2_OFFSET          1.7f     // mm offset (slight difference)
#define SIM_RH_JITTER             0.2f     // mm noise
#define SIM_RH_BATTERY_V          3.85f    // starting battery voltage

// Tire Probe
#define SIM_TIRE_INNER            188.0f   // °F (slightly hotter — camber)
#define SIM_TIRE_MIDDLE           185.0f   // °F
#define SIM_TIRE_OUTER            182.0f   // °F
#define SIM_BRAKE_TEMP            450.0f   // °F
#define SIM_TIRE_NOISE            1.5f     // °F noise
#define SIM_TIRE_DRIFT            0.3f     // °F/sec drift rate

// Tire Temp Gun
#define SIM_GUN_TEMP              185.0f   // °F
#define SIM_GUN_AMBIENT           72.3f    // °F
#define SIM_GUN_NOISE             2.0f     // °F noise (IR gun is noisier)
#define SIM_GUN_DEFAULT_EMISSIVITY 0.95f

// ===================== UPDATE INTERVALS (ms) =====================
#define UPD_HEATER_TEMP           500      // 2 Hz
#define UPD_HEATER_STATUS         2000     // 0.5 Hz or on change
#define UPD_SCALE_WEIGHT          250      // 4 Hz
#define UPD_SCALE_TEMP            5000     // 0.2 Hz
#define UPD_SCALE_BATTERY         10000    // 0.1 Hz
#define UPD_SCALE_STATUS          2000     // 0.5 Hz
#define UPD_RH_HEIGHT             500      // 2 Hz
#define UPD_RH_STATUS             1000     // 1 Hz
#define UPD_PROBE_READING         1000     // 1 Hz
#define UPD_PROBE_STATUS          2000     // 0.5 Hz
#define UPD_GUN_TEMP              250      // 4 Hz continuous mode

// ===================== BLE ADVERTISING =====================
#define BLE_MTU_SIZE              512
#define ADV_MIN_INTERVAL          0x06     // iOS connection workaround
#define ADV_MAX_INTERVAL          0x12

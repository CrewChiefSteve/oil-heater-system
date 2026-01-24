#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PIN DEFINITIONS - ESP32-C3 Mini
// ============================================================================

// I2C Bus (shared between both VL53L1X sensors)
#define PIN_SDA 4
#define PIN_SCL 5

// XSHUT pins for sensor address assignment
#define PIN_XSHUT_SENSOR1 6  // Sensor 1 -> 0x30
#define PIN_XSHUT_SENSOR2 7  // Sensor 2 -> 0x31

// User Interface
#define PIN_BUTTON 9   // Input button (INPUT_PULLUP, active LOW)
#define PIN_LED 8      // Status LED (active HIGH)

// Battery Monitoring
#define PIN_BATTERY_ADC 2  // ADC input for battery voltage

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================

// VL53L1X I2C addresses (after XSHUT sequencing)
#define SENSOR1_ADDRESS 0x30
#define SENSOR2_ADDRESS 0x31
#define SENSOR_DEFAULT_ADDRESS 0x29  // Factory default before reassignment

// Timing Budget (measurement time per sensor)
#define TIMING_BUDGET_MS 33  // ~30Hz update rate

// Distance Mode
#define DISTANCE_MODE_LONG true  // true = 4m range, false = 1.3m range

// Outlier rejection threshold
#define OUTLIER_THRESHOLD_MM 10.0  // If delta > 10mm, use lower value

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================

#define BUTTON_DEBOUNCE_MS 50  // Debounce time in milliseconds

// ============================================================================
// BATTERY MONITORING
// ============================================================================

// ADC configuration (ESP32-C3 ADC is 12-bit: 0-4095)
#define ADC_RESOLUTION 4095.0
#define ADC_REFERENCE_VOLTAGE 3.3  // ESP32-C3 ADC reference voltage

// Battery voltage divider ratio (adjust based on actual hardware)
// If using a 2:1 divider (2x 10kΩ resistors): ratio = 2.0
#define VOLTAGE_DIVIDER_RATIO 2.0

// ============================================================================
// BLE CONFIGURATION
// ============================================================================

// Device name (will be dynamic: "RH-Sensor_XX" where XX is corner ID)
#define BLE_DEVICE_NAME_BASE "RH-Sensor"

// Service UUID - MUST match @crewchiefsteve/ble package
// See packages/ble/src/constants/uuids.ts
#define SERVICE_UUID "4fafc201-0003-459e-8fcc-c5c9c331914b"

// Characteristic UUIDs
#define CHAR_HEIGHT_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"   // Height data (R/N)
#define CHAR_COMMAND_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Commands (W)
#define CHAR_STATUS_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"   // Status (R/N)
#define CHAR_CORNER_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ad"   // Corner ID (R/W/N)

// BLE commands
#define CMD_SINGLE_READING 'R'     // Take single reading
#define CMD_CONTINUOUS_START 'C'   // Start continuous reading mode
#define CMD_CONTINUOUS_STOP 'S'    // Stop continuous reading mode
#define CMD_ZERO_CALIBRATION 'Z'   // Zero calibration (store offset)

// BLE connection parameters
#define BLE_MTU_SIZE 128  // Maximum transmission unit

// ============================================================================
// NVS CONFIGURATION
// ============================================================================

#define NVS_NAMESPACE "rh_sensor_v1"
#define NVS_CORNER_KEY "corner_id"
#define NVS_ZERO_OFFSET_KEY "zero_offset"

// Default corner ID (if not set in NVS)
#define DEFAULT_CORNER "01"

// ============================================================================
// OPERATIONAL PARAMETERS
// ============================================================================

// Continuous mode update interval
#define CONTINUOUS_UPDATE_INTERVAL_MS 100  // 10Hz in continuous mode

// LED blink patterns (ms)
#define LED_BLINK_READING 50     // Quick blink during reading
#define LED_BLINK_ERROR 200      // Slow blink on error
#define LED_BLINK_CONNECTED 1000 // Heartbeat when BLE connected

// Zero calibration
#define ZERO_OFFSET_MAX_MM 500.0  // Maximum allowed zero offset

// ============================================================================
// DATA FORMAT
// ============================================================================

// Example: "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"
// Maximum string length calculation:
// S1:xxxx.x (9) + , (1) + S2:xxxx.x (9) + , (1) + AVG:xxxx.x (10) + , (1) + IN:xx.xx (8) + , (1) + BAT:x.xx (8) = 48 chars + null
#define DATA_STRING_MAX_LENGTH 64

#endif // CONFIG_H

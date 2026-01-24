#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================================================================
// PIN DEFINITIONS (ESP32-S3 Optimized)
// ================================================================

#define DOUT 42
#define CLK 41
#define ONE_WIRE_BUS 6     // DS18B20 Temp Sensor
#define ZERO_BUTTON 5      // Tare Button

// I2C Pins for OLED (ESP32-S3 custom)
#define I2C_SDA 8
#define I2C_SCL 9

// ================================================================
// OLED CONFIGURATION
// ================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// ================================================================
// RACE-OPTIMIZED CONFIGURATION
// ================================================================

struct ScaleConfig {
    // HX711 Settings
    static constexpr uint8_t HX711_SAMPLES = 2;           // Internal averaging
    static constexpr uint8_t SAMPLE_RATE = 80;            // 80 Hz HX711 rate

    // Adaptive Filter Settings - TUNED FOR FAST RESPONSE
    static constexpr float FAST_FILTER_ALPHA = 0.7f;      // During weight changes
    static constexpr float SLOW_FILTER_ALPHA = 0.20f;     // When stable
    static constexpr float CHANGE_DETECT_THRESHOLD = 0.3f; // lbs to trigger fast mode
    static constexpr float STABILITY_RANGE = 0.15f;        // +/- range when stable
    static constexpr uint32_t SETTLE_TIME_MS = 1500;       // Time to switch to slow filter

    // Zero Deadband - prevents wandering at zero
    static constexpr float ZERO_DEADBAND = 0.3f;          // Snap to 0 if under this

    // Temperature Compensation
    static constexpr float TEMP_COEFFICIENT = 0.0002f;
    static constexpr float REFERENCE_TEMP = 70.0f;

    // Display Settings
    static constexpr uint32_t UPDATE_RATE_MS = 25;        // 40Hz display updates
    static constexpr float ROUND_THRESHOLD = 0.05f;       // Snap to 0.05 lb increments
    static constexpr float NOISE_THRESHOLD = 0.10f;       // Display deadband

    // Button Settings
    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 200;   // Button debounce time
    static constexpr uint32_t BUTTON_HOLD_MS = 3000;      // Hold time for calibration

    // Temperature Reading
    static constexpr uint32_t TEMP_UPDATE_MS = 5000;      // Temperature update interval

    // BLE Update Rate
    static constexpr uint32_t BLE_UPDATE_MS = 250;        // 4Hz BLE updates

    // Serial Debug Output Rate
    static constexpr uint32_t DEBUG_OUTPUT_MS = 500;      // Debug print interval
};

// Default calibration factor (will be loaded from NVS if saved)
#define DEFAULT_CALIBRATION 2843.0f

// Default corner ID (if not set in NVS)
#define DEFAULT_CORNER "01"

// NVS namespace
#define NVS_NAMESPACE "racescale_v3"
#define NVS_CAL_KEY "cal_factor"
#define NVS_CORNER_KEY "corner_id"

#endif // CONFIG_H

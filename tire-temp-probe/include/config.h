#ifndef CONFIG_H
#define CONFIG_H

// Device identification
#define DEVICE_NAME_BASE        "TireProbe"     // Base name (corner appended: TireProbe_LF)
#define DEVICE_MODEL            "TTP-4CH-v1"

// NVS Configuration (Non-Volatile Storage for settings persistence)
#define NVS_NAMESPACE           "tireprobe_v2"
#define NVS_CORNER_KEY          "corner_id"

// Temperature reading configuration
#define TEMP_READ_INTERVAL_MS   100     // Read thermocouples every 100ms (increased frequency for stability detection)
#define TEMP_STABLE_THRESHOLD   0.5     // Degrees C variance allowed for stability
#define STABILITY_DURATION_MS   1000    // Must be stable for this duration before auto-capture
#define TEMP_SMOOTHING_SAMPLES  8       // Moving average window size

// Capture feedback timing
#define CAPTURE_DISPLAY_MS      1500    // Show capture confirmation screen for this duration
#define CAPTURE_LED_MS          1000    // Show green LED for this duration after capture

// BLE transmission configuration
#define STATUS_TX_INTERVAL_MS   2000    // System status broadcast interval
#define BLE_TX_POWER            ESP_PWR_LVL_P9  // Maximum power for range

// Temperature unit preference
#define USE_FAHRENHEIT          true    // true = Fahrenheit, false = Celsius

// Battery monitoring
#define BATTERY_READ_INTERVAL_MS    5000    // Read battery every 5s
#define BATTERY_ADC_SAMPLES         16      // ADC averaging samples
#define BATTERY_VOLTAGE_DIVIDER     2.0     // Voltage divider ratio (if used)
#define BATTERY_MIN_VOLTAGE         3.3     // Minimum battery voltage (V)
#define BATTERY_MAX_VOLTAGE         4.2     // Maximum battery voltage (V)
#define BATTERY_LOW_THRESHOLD       10      // Low battery warning (%)

// LED status indication intervals
#define LED_BRIGHTNESS          64      // 0-255, lower for battery life
#define LED_IDLE_INTERVAL_MS    2000    // Slow blink when idle
#define LED_ACTIVE_INTERVAL_MS  500     // Fast blink when measuring
#define LED_ERROR_INTERVAL_MS   200     // Very fast blink on error

// Thermocouple error thresholds
#define MAX_TEMP_C              400.0   // Maximum reasonable temperature
#define MIN_TEMP_C              -10.0   // Minimum reasonable temperature

// Corner assignment (for multi-probe systems)
#define DEFAULT_CORNER_ID       0           // Default corner: 0=LF, 1=RF, 2=LR, 3=RR

#endif // CONFIG_H

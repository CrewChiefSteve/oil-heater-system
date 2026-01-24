#ifndef CONFIG_H
#define CONFIG_H

// Device identification
#define DEVICE_NAME             "TireTemp_Probe"
#define DEVICE_MODEL            "TTP-4CH-v1"

// Temperature reading configuration
#define TEMP_READ_INTERVAL_MS   250     // Read thermocouples every 250ms
#define TEMP_STABLE_THRESHOLD   0.5     // Degrees C change to consider stable
#define TEMP_STABLE_COUNT       4       // Consecutive stable readings required
#define TEMP_SMOOTHING_SAMPLES  8       // Moving average window size

// BLE transmission configuration
#define BLE_TX_INTERVAL_MS      500     // Broadcast data every 500ms
#define BLE_DEVICE_NAME         DEVICE_NAME
#define BLE_TX_POWER            ESP_PWR_LVL_P9  // Maximum power for range

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
#define DEFAULT_CORNER          CORNER_FL  // Default corner assignment

#endif // CONFIG_H

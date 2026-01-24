#ifndef CONFIG_H
#define CONFIG_H

// ==================== Pin Definitions ====================

// I2C Bus (shared by MLX90614 and SSD1306)
#define PIN_SDA 21
#define PIN_SCL 22

// I2C Device Addresses
#define MLX90614_ADDR 0x5A
#define SSD1306_ADDR 0x3C

// Buttons (all with internal pull-up)
#define PIN_TRIGGER 13
#define PIN_MODE 12
#define PIN_HOLD 14

// Laser module (active HIGH)
#define PIN_LASER 27

// Piezo buzzer
#define PIN_BUZZER 25

// Battery voltage sense (ADC)
#define PIN_BAT_SENSE 34

// ==================== Hardware Constants ====================

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // No reset pin

// Battery monitoring
#define BAT_VOLTAGE_DIVIDER_RATIO 2.0  // R1=R2, voltage divider ratio
#define BAT_MAX_VOLTAGE 4.2            // Li-ion full charge
#define BAT_MIN_VOLTAGE 3.3            // Li-ion cutoff
#define BAT_ADC_SAMPLES 16             // Averaging for stable reading

// Button debouncing
#define DEBOUNCE_MS 50
#define LONG_PRESS_MS 2000

// ==================== Measurement Settings ====================

// MLX90614 settings
#define DEFAULT_EMISSIVITY 0.95  // Rubber tire emissivity
#define TEMP_READ_INTERVAL_MS 100  // 10 Hz sampling

// Temperature ranges (Fahrenheit)
#define TEMP_MIN_F -40.0
#define TEMP_MAX_F 500.0

// Display update
#define DISPLAY_UPDATE_INTERVAL_MS 100

// ==================== BLE Settings ====================

// BLE device name
#define BLE_DEVICE_NAME "TireTempGun"

// BLE Service and Characteristic UUIDs
// Service UUID - MUST match @crewchiefsteve/ble package
// See packages/ble/src/constants/uuids.ts
#define SERVICE_UUID "4fafc201-0005-459e-8fcc-c5c9c331914b"
#define CHAR_TEMP_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Notify
#define CHAR_CMD_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"   // Write

// BLE transmission interval
#define BLE_NOTIFY_INTERVAL_MS 250  // 4 Hz when connected

// ==================== Buzzer Settings ====================

// Buzzer feedback tones
#define BUZZ_FREQ_BUTTON 2000  // Button press
#define BUZZ_FREQ_MODE 2500    // Mode change
#define BUZZ_FREQ_RESET 3000   // Max/Min reset
#define BUZZ_DURATION_MS 50

// ==================== Measurement Modes ====================

enum MeasurementMode {
    MODE_INSTANT = 0,  // Live temperature
    MODE_HOLD = 1,     // Freeze last reading
    MODE_MAX = 2,      // Track maximum
    MODE_MIN = 3       // Track minimum
};

#endif // CONFIG_H

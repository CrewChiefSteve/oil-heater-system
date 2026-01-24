#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// I2C Configuration
// ========================================
#define I2C_SDA_PIN 32
#define I2C_SCL_PIN 25
#define I2C_FREQUENCY 100000  // 100kHz

// ========================================
// MCP23017 I/O Expander Configuration
// ========================================
#define MCP23017_ADDR 0x27

// MCP23017 Pin Assignments (Port A)
#define MCP_RELAY_PIN 0      // GPA0 - Relay control (HIGH=ON, LOW=OFF)
#define MCP_MAX6675_SCK 1    // GPA1 - MAX6675 clock
#define MCP_MAX6675_CS 2     // GPA2 - MAX6675 chip select
#define MCP_MAX6675_SO 3     // GPA3 - MAX6675 data out

// ========================================
// Display Configuration (CYD)
// ========================================
// These are configured in platformio.ini build_flags
// TFT_MISO=12, TFT_MOSI=13, TFT_SCLK=14
// TFT_CS=15, TFT_DC=2, TFT_RST=-1, TFT_BL=21
// TOUCH_CS=33

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

// Display colors
#define COLOR_BG TFT_BLACK
#define COLOR_TEXT TFT_WHITE
#define COLOR_TEMP_NORMAL TFT_CYAN
#define COLOR_TEMP_HIGH TFT_RED
#define COLOR_SETPOINT TFT_YELLOW
#define COLOR_HEATER_ON TFT_RED
#define COLOR_HEATER_OFF TFT_GREEN
#define COLOR_BUTTON TFT_BLUE
#define COLOR_BUTTON_TEXT TFT_WHITE

// ========================================
// Thermostat Configuration
// ========================================
#define DEFAULT_SETPOINT_F 180.0f      // Default setpoint in Fahrenheit (race oil)
#define TEMP_HYSTERESIS_F 1.0f         // Temperature hysteresis band (±1°F)
#define MIN_SETPOINT_F 50.0f           // Minimum allowed setpoint
#define MAX_SETPOINT_F 280.0f          // Maximum allowed setpoint
#define SETPOINT_INCREMENT 5.0f        // Increment/decrement step for setpoint

// ========================================
// Safety Configuration
// ========================================
#define SAFETY_MAX_TEMP_F 300.0f       // Maximum safe temperature (race oil)
#define SENSOR_ERROR_TEMP 999.0f       // Temperature value indicating sensor error
#define MAX6675_ERROR_VALUE 4095       // MAX6675 raw value indicating error

// ========================================
// Timing Configuration
// ========================================
#define TEMP_READ_INTERVAL 15000        // Read temperature every 15 seconds (ms)
#define DISPLAY_UPDATE_INTERVAL 1000    // Update display every 1 second (ms)
#define TOUCH_DEBOUNCE_MS 200          // Touch debounce delay (ms)
#define RELAY_MIN_CYCLE_TIME 10000     // Minimum relay on/off cycle time (10s in ms)

// ========================================
// Touch Button Configuration
// ========================================
#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 60
#define BUTTON_Y_POS 240

// Up button position
#define BUTTON_UP_X 60
#define BUTTON_UP_Y BUTTON_Y_POS

// Down button position
#define BUTTON_DOWN_X 320
#define BUTTON_DOWN_Y BUTTON_Y_POS

// Touch calibration (adjust these if touch is not accurate)
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3700
#define TOUCH_MIN_Y 240
#define TOUCH_MAX_Y 3800

#endif // CONFIG_H

// ================================================================
//   ESP32 BLE Race Scale - PRODUCTION VERSION V4.0
//   CrewChiefSteve Standard - Configurable Corner Identity
//   Features: Adaptive filtering, NVS storage, fast response,
//   temperature compensation, button handling, async operations,
//   Serial calibration, zero deadband, corner configuration
//   Target: ESP32-S3 with custom pinout (GPIO 42/41 HX711, I2C 8/9)
// ================================================================

#include <HX711.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include <ArduinoJson.h>  // For STATUS JSON encoding
#include "config.h"
#include "ble_protocol.h"
#include "adaptive_filter.h"
#include "button_handler.h"

// ================================================================
// GLOBAL OBJECTS
// ================================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
HX711 scale;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
Preferences preferences;
AdaptiveFilter filter;
ButtonHandler tareButton(ZERO_BUTTON);

BLEServer* pServer = nullptr;
BLECharacteristic* pWeightChar = nullptr;
BLECharacteristic* pTareChar = nullptr;
BLECharacteristic* pCalibrationChar = nullptr;
BLECharacteristic* pTempChar = nullptr;
BLECharacteristic* pStatusChar = nullptr;
BLECharacteristic* pCornerChar = nullptr;
BLECharacteristic* pBatteryChar = nullptr;  // ADDED: Battery percentage characteristic

// ================================================================
// STATE VARIABLES
// ================================================================

bool deviceConnected = false;
bool displayAvailable = false;  // Track if OLED is connected
float temperature = 70.0f;
float compensatedCalibration = DEFAULT_CALIBRATION;
float BASE_CALIBRATION = DEFAULT_CALIBRATION;
float currentWeight = 0;
float displayWeight = 0;
bool isStable = false;

// Corner identity (configurable via BLE/NVS)
String cornerID = DEFAULT_CORNER;  // String name for display/NVS (e.g., "LF")
uint8_t cornerIDInt = CORNER_LF;   // UInt8 for BLE (0-3)
String deviceName = "RaceScale_" + String(DEFAULT_CORNER);

// Async temperature reading
bool tempRequested = false;
unsigned long tempRequestTime = 0;

// Timing variables
unsigned long lastDisplayUpdate = 0;
unsigned long lastTempUpdate = 0;
unsigned long lastBLEUpdate = 0;

// ================================================================
// FORWARD DECLARATIONS
// ================================================================

void updateCalibration();
void performPrecisionTare();
void performCalibration();
void initializeBLE();
void loadSettings();
void saveSettings();
void updateDisplay();
void updateBLE();
void handleAsyncTemp();
void handleSerialCommands();
void setCornerID(const String& newCorner);

// ================================================================
// BLE CALLBACKS
// ================================================================

class MyServerCB : public BLEServerCallbacks {
    void onConnect(BLEServer* p) {
        deviceConnected = true;
        Serial.println("BLE Connected");

        // ✅ UPDATED: Send current status as JSON on connect
        if (pStatusChar) {
            StaticJsonDocument<128> doc;
            doc["zeroed"] = true;  // Assume tared if running
            doc["calibrated"] = (BASE_CALIBRATION > 0);
            doc["error"] = "";
            String json;
            serializeJson(doc, json);
            pStatusChar->setValue(json.c_str());
            pStatusChar->notify();
        }
    }

    void onDisconnect(BLEServer* p) {
        deviceConnected = false;
        Serial.println("BLE Disconnected");
        // Auto-restart advertising
        BLEDevice::getAdvertising()->start();
    }
};

class TareCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) {
        // ✅ UPDATED: Changed from String "1" to UInt8 0x01
        std::string value = c->getValue();
        if (value.length() > 0 && value[0] == TARE_COMMAND) {
            Serial.println("BLE Request: TARE (UInt8 0x01)");
            performPrecisionTare();
        }
    }
};

class CalibrationCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) {
        // ✅ UPDATED: Changed from String to Float32LE (4 bytes)
        std::string value = c->getValue();
        if (value.length() == 4) {  // Float32LE is exactly 4 bytes
            float knownWeight;
            memcpy(&knownWeight, value.data(), 4);

            if (knownWeight > 0) {
                float currentReading = scale.get_units(10);
                float ratio = currentReading / knownWeight;
                BASE_CALIBRATION = BASE_CALIBRATION * ratio;

                updateCalibration();
                scale.set_scale(compensatedCalibration);
                saveSettings();
                filter.reset();

                Serial.printf("✓ BLE Calibrated: Target=%.1f, Factor=%.1f (saved)\n",
                    knownWeight, BASE_CALIBRATION);
            }
        } else {
            Serial.printf("❌ BLE Calibration error: Expected 4 bytes, got %d\n", value.length());
        }
    }
};

class CornerCB : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) {
        // ✅ UPDATED: Changed from String "LF"/"RF"/etc to UInt8 (0-3)
        std::string value = c->getValue();
        if (value.length() > 0) {
            uint8_t cornerInt = value[0];
            if (cornerInt >= CORNER_LF && cornerInt <= CORNER_RR) {
                cornerIDInt = cornerInt;
                cornerID = cornerUInt8ToString(cornerInt);
                saveSettings();
                Serial.printf("✓ BLE Corner Set: %s (%d) (saved, restart to apply to device name)\n",
                    cornerID.c_str(), cornerInt);
            } else {
                Serial.printf("❌ BLE Corner error: Invalid value %d (expected 0-3)\n", cornerInt);
            }
        }
    }

    void onRead(BLECharacteristic* c) {
        // ✅ UPDATED: Return UInt8 instead of String
        c->setValue(&cornerIDInt, 1);
    }
};

// ================================================================
// SERIAL COMMAND HANDLER
// ================================================================

void handleSerialCommands() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.startsWith("cal ")) {
            float knownWeight = input.substring(4).toFloat();
            if (knownWeight > 0) {
                float currentReading = scale.get_units(10);
                float ratio = currentReading / knownWeight;
                BASE_CALIBRATION = BASE_CALIBRATION * ratio;
                updateCalibration();
                scale.set_scale(compensatedCalibration);
                saveSettings();
                filter.reset();
                Serial.printf("✓ Calibrated! New factor=%.1f (saved to NVS)\n", BASE_CALIBRATION);
            } else {
                Serial.println("✗ Invalid weight. Usage: cal 25");
            }
        } else if (input == "tare") {
            performPrecisionTare();
        } else if (input.startsWith("corner ")) {
            String newCorner = input.substring(7);
            newCorner.trim();
            newCorner.toUpperCase();
            if (newCorner.length() >= 2) {
                setCornerID(newCorner);
                Serial.printf("✓ Corner set to: %s (restart to apply to device name)\n",
                    cornerID.c_str());
            } else {
                Serial.println("✗ Invalid corner. Usage: corner LF (or RF, LR, RR, 01, 02, etc.)");
            }
        } else if (input == "info") {
            Serial.println("\n=== SCALE INFO ===");
            Serial.printf("Corner: %s\n", cornerID.c_str());
            Serial.printf("Device Name: %s\n", deviceName.c_str());
            Serial.printf("Calibration: %.1f\n", BASE_CALIBRATION);
            Serial.printf("Compensated: %.1f\n", compensatedCalibration);
            Serial.printf("Temperature: %.1fF\n", temperature);
            Serial.printf("Weight: %.2f lbs\n", displayWeight);
            Serial.printf("Stable: %s\n", isStable ? "YES" : "NO");
            Serial.printf("BLE: %s\n", deviceConnected ? "Connected" : "Waiting");
            Serial.println("==================\n");
        } else if (input == "raw") {
            float raw = scale.get_units(10);
            Serial.printf("Raw reading (10 samples): %.3f lbs\n", raw);
        } else if (input == "reset") {
            preferences.begin(NVS_NAMESPACE, false);
            preferences.clear();
            preferences.end();
            Serial.println("✓ NVS cleared! Restart to use defaults.");
        } else if (input == "help") {
            Serial.println("\n=== SERIAL COMMANDS ===");
            Serial.println("cal <weight>  - Calibrate (e.g., 'cal 25')");
            Serial.println("tare          - Zero the scale");
            Serial.println("corner <ID>   - Set corner (e.g., 'corner LF' or 'corner 01')");
            Serial.println("info          - Show current settings");
            Serial.println("raw           - Show raw reading");
            Serial.println("reset         - Clear NVS, restore defaults");
            Serial.println("help          - Show this help");
            Serial.println("========================\n");
        } else if (input.length() > 0) {
            Serial.println("Unknown command. Type 'help' for options.");
        }
    }
}

// ================================================================
// CORNER ID MANAGEMENT
// ================================================================

void setCornerID(const String& newCorner) {
    cornerID = newCorner;
    cornerIDInt = cornerStringToUInt8(newCorner);  // ✅ NEW: Convert to UInt8
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putString(NVS_CORNER_KEY, cornerID);
    preferences.end();

    // ✅ UPDATED: Update BLE characteristic with UInt8 value
    if (pCornerChar) {
        pCornerChar->setValue(&cornerIDInt, 1);
    }
}

// ================================================================
// SETUP
// ================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);  // Longer delay for USB CDC enumeration
    Serial.flush();

    // Release JTAG pins for GPIO use
    gpio_reset_pin(GPIO_NUM_42);
    gpio_reset_pin(GPIO_NUM_41);

    Serial.println("\n\n===================================");
    Serial.println("=== ESP32 Race Scale V4.0 (S3)  ===");
    Serial.println("=== CrewChiefSteve Standard     ===");
    Serial.println("=== Configurable Corner ID      ===");
    Serial.println("===================================\n");
    Serial.flush();

    // Load settings from NVS FIRST (to get corner ID)
    Serial.println("✓ Loading settings from NVS...");
    loadSettings();

    // Build device name with corner ID
    deviceName = "RaceScale_" + cornerID;
    Serial.printf("✓ Device: %s (Corner: %s)\n", deviceName.c_str(), cornerID.c_str());

    // Initialize OLED with ESP32-S3 custom I2C pins (SDA=8, SCL=9)
    // Skip display initialization if not connected (prevents hanging)
    Serial.println("⚠ OLED disabled - running without display");
    displayAvailable = false;

    // Uncomment below to enable display if connected:
    /*
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setTimeout(100);  // 100ms timeout for I2C operations
    displayAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

    if (!displayAvailable) {
        Serial.println("⚠ OLED not detected - running without display (SDA=8, SCL=9)");
    } else {
        Serial.println("✓ OLED initialized (0.96in SSD1306)");
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("RaceScale V4.0");
        display.printf("Corner: %s\n", cornerID.c_str());
        display.println("ESP32-S3 Init...");
        display.display();
        delay(1000);
    }
    */

    // Initialize temperature sensor (DS18B20 on GPIO 6)
    Serial.println("✓ Initializing DS18B20 (GPIO 6)...");
    tempSensor.begin();
    tempSensor.setWaitForConversion(false);  // Async mode
    tempSensor.requestTemperatures();
    tempRequested = true;
    tempRequestTime = millis();

    // Initialize HX711 scale (DOUT=42, CLK=41, 128 gain)
    Serial.println("✓ Initializing HX711 (DOUT=42, CLK=41)...");
    scale.begin(DOUT, CLK);
    scale.set_gain(128);  // Channel A, gain 128 (±20mV/V)
    Serial.println("  - Scale powering up...");

    // Wait for scale to stabilize
    delay(1000);
    scale.power_up();
    delay(500);

    // Set temperature-compensated calibration
    Serial.printf("  - Cal factor: %.1f (T-compensated)\n", compensatedCalibration);
    updateCalibration();
    scale.set_scale(compensatedCalibration);

    // Skip auto-tare on startup (scale can be loaded during boot)
    Serial.println("⚠ Auto-tare DISABLED - use button or BLE to tare manually");
    // performPrecisionTare();  // Commented out - no auto-tare required

    // Start BLE stack
    Serial.printf("✓ Starting BLE (%s)...\n", deviceName.c_str());
    initializeBLE();

    Serial.println("\n🎉 RACE SCALE V4.0 READY!");
    Serial.println("──────────────────────────────");
    Serial.println("HARDWARE:");
    Serial.println("• Short button press = TARE");
    Serial.println("• 3s button hold = CAL MODE");
    Serial.println("──────────────────────────────");
    Serial.println("SERIAL COMMANDS (type 'help'):");
    Serial.println("• cal 25      = Calibrate to 25 lbs");
    Serial.println("• tare        = Zero the scale");
    Serial.println("• corner LF   = Set corner ID");
    Serial.println("• info        = Show settings");
    Serial.println("──────────────────────────────");
    Serial.printf("Current corner: %s\n", cornerID.c_str());
    Serial.printf("Current cal factor: %.1f\n", BASE_CALIBRATION);
    Serial.println("===================================\n");
}

// ================================================================
// MAIN LOOP (Non-blocking, 40Hz display, 4Hz BLE)
// ================================================================

void loop() {
    unsigned long currentMillis = millis();

    // === SERIAL COMMAND HANDLER ===
    handleSerialCommands();

    // === BUTTON HANDLING ===
    ButtonHandler::ButtonEvent btnEvent = tareButton.update();
    if (btnEvent == ButtonHandler::SHORT_PRESS) {
        Serial.println("🔘 Button: PRECISION TARE");
        performPrecisionTare();
    } else if (btnEvent == ButtonHandler::LONG_PRESS) {
        Serial.println("🔘 Button: CALIBRATION MODE");
        performCalibration();
    }

    // === ASYNC TEMP SENSOR ===
    handleAsyncTemp();

    // === WEIGHT ACQUISITION (80Hz capable) ===
    if (scale.is_ready()) {
        float raw = scale.get_units(ScaleConfig::HX711_SAMPLES);
        currentWeight = filter.update(raw);
        isStable = filter.isStable();

        // ZERO DEADBAND - snap to zero when under threshold
        if (abs(currentWeight) < ScaleConfig::ZERO_DEADBAND) {
            displayWeight = 0.0f;
        }
        // Normal update: only if change > threshold OR stable
        else if (abs(currentWeight - displayWeight) > ScaleConfig::NOISE_THRESHOLD || isStable) {
            displayWeight = currentWeight;
        }

        // Debug print (500ms rate)
        static unsigned long debugTimer = 0;
        if (currentMillis - debugTimer > ScaleConfig::DEBUG_OUTPUT_MS) {
            Serial.printf("Raw: %6.3f | Filt: %5.2f | Disp: %5.2f lbs | %s | T:%.1fF | Cal:%.0f\n",
                raw, currentWeight, displayWeight,
                isStable ? "✅ STABLE" : "⏳ MEASURING",
                temperature, compensatedCalibration);
            debugTimer = currentMillis;
        }
    }

    // === 40Hz OLED UPDATE ===
    if (currentMillis - lastDisplayUpdate >= ScaleConfig::UPDATE_RATE_MS) {
        updateDisplay();
        lastDisplayUpdate = currentMillis;
    }

    // === 4Hz BLE UPDATE (when connected) ===
    if (deviceConnected && (currentMillis - lastBLEUpdate >= ScaleConfig::BLE_UPDATE_MS)) {
        updateBLE();
        lastBLEUpdate = currentMillis;
    }
}

// ================================================================
// ASYNC TEMPERATURE (Non-blocking DS18B20)
// ================================================================

void handleAsyncTemp() {
    unsigned long now = millis();

    // Trigger new reading every 5s
    if (!tempRequested && (now - lastTempUpdate >= ScaleConfig::TEMP_UPDATE_MS)) {
        tempSensor.requestTemperatures();
        tempRequested = true;
        tempRequestTime = now;
    }

    // Read after 800ms conversion (12-bit mode)
    if (tempRequested && (now - tempRequestTime >= 800)) {
        float newTemp = tempSensor.getTempFByIndex(0);

        // Robust validation
        if (newTemp > -50 && newTemp < 150 && newTemp != DEVICE_DISCONNECTED_C) {
            if (abs(newTemp - temperature) > 10.0f) {
                Serial.printf("⚠️ Temp warning: %.1fF (filtered)\n", newTemp);
            } else {
                temperature = newTemp;
                updateCalibration();
                scale.set_scale(compensatedCalibration);

                // ✅ UPDATED: Notify BLE with Float32LE instead of String
                if (deviceConnected && pTempChar) {
                    pTempChar->setValue((uint8_t*)&temperature, sizeof(float));
                    pTempChar->notify();
                }
            }
        } else {
            Serial.println("❌ DS18B20 error - retrying...");
        }

        tempRequested = false;
        lastTempUpdate = now;
    }
}

// ================================================================
// TEMPERATURE COMPENSATION
// ================================================================

void updateCalibration() {
    float tempDiff = temperature - ScaleConfig::REFERENCE_TEMP;
    float correction = 1.0f + (ScaleConfig::TEMP_COEFFICIENT * tempDiff);
    compensatedCalibration = BASE_CALIBRATION * correction;
}

// ================================================================
// PRECISION TARE (10 samples, visual feedback)
// ================================================================

void performPrecisionTare() {
    Serial.println("\n=== 🔄 PRECISION TARE (10x avg) ===");

    // OLED feedback
    if (displayAvailable) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(10, 20);
        display.println("TARING...");
        display.setCursor(20, 45);
        display.setTextSize(1);
        display.println("Stay still...");
        display.display();
    }

    float before = scale.get_units(5);
    Serial.printf("Before tare: %.3f lbs\n", before);

    scale.tare(10);  // 10-sample average

    float after = scale.get_units(5);
    Serial.printf("After tare:  %.3f lbs ✓\n", after);

    // Reset filter state
    filter.reset();

    Serial.println("Tare complete!\n");

    // Brief confirmation
    if (displayAvailable) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(30, 20);
        display.println("TARED");
        display.setTextSize(1);
        display.setCursor(40, 45);
        display.println("0.00 lbs");
        display.display();
        delay(800);
    }
}

// ================================================================
// CALIBRATION MODE (BLE + Display)
// ================================================================

void performCalibration() {
    Serial.println("\n=== ⚙️ CALIBRATION MODE ===");
    Serial.println("Place known weight, then:");
    Serial.println("  Serial: 'cal 25' (for 25 lbs)");
    Serial.println("  BLE: Send '25'");

    if (displayAvailable) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("CALIBRATION");
        display.println("MODE ACTIVE");
        display.setCursor(0, 25);
        display.println("1. Place known");
        display.println("   weight (lbs)");
        display.println("2. Serial: cal 25");
        display.println("   or BLE: 25");
        display.setCursor(0, 55);
        display.printf("Current: %.2f\n", displayWeight);
        display.display();
    }

    // Show live weight during cal
    unsigned long calStart = millis();
    while (millis() - calStart < 10000) {  // 10s timeout
        if (scale.is_ready()) {
            float raw = scale.get_units(3);
            if (displayAvailable) {
                display.fillRect(0, 55, 128, 9, SSD1306_BLACK);
                display.setCursor(0, 55);
                display.printf("Live: %.2f lbs", raw);
                display.display();
            }
        }

        // Check for serial calibration during this time
        handleSerialCommands();

        delay(100);
    }
}

// ================================================================
// OLED DISPLAY (40Hz, Adaptive Precision)
// ================================================================

void updateDisplay() {
    if (!displayAvailable) return;  // Skip if no display

    display.clearDisplay();

    // Intelligent rounding by weight range
    float weightToShow = displayWeight;
    float rounded;
    if (abs(weightToShow) < 1.0f) {
        rounded = round(weightToShow * 100.0f) / 100.0f;  // 0.01lb
    } else if (abs(weightToShow) < 10.0f) {
        rounded = round(weightToShow * 20.0f) / 20.0f;   // 0.05lb
    } else {
        rounded = round(weightToShow * 10.0f) / 10.0f;   // 0.10lb
    }

    // BIG NUMBERS (size 3)
    display.setTextSize(3);
    display.setCursor(0, 2);
    char weightStr[12];
    if (abs(rounded) < 10.0f) {
        snprintf(weightStr, sizeof(weightStr), "%.2f", rounded);
    } else if (abs(rounded) < 100.0f) {
        snprintf(weightStr, sizeof(weightStr), "%.1f", rounded);
    } else {
        snprintf(weightStr, sizeof(weightStr), "%.0f", rounded);
    }
    display.print(weightStr);

    // Status line 1 (size 1)
    display.setTextSize(1);
    display.setCursor(0, 32);
    display.print("lbs ");
    if (isStable) {
        display.print("LOCKED");
    } else {
        static uint8_t anim = 0;
        const char* dots[] = {"[~] ", "[~~]", "[~~~]", "[~~] "};
        display.print(dots[anim++ / 5 % 4]);
        display.print("MEASURING");
    }

    // Temp + Corner (line 2)
    display.setCursor(0, 44);
    display.print("T:");
    display.print(temperature, 1);
    display.print("F ");
    display.print(cornerID);

    // BLE Status (line 3)
    display.setCursor(0, 55);
    if (deviceConnected) {
        display.print("BLE Connected ");
        display.print((int)(millis()/1000));
    } else {
        display.print("BLE Waiting...");
    }

    display.display();
}

// ================================================================
// BLE NOTIFICATIONS (4Hz when connected)
// ================================================================

void updateBLE() {
    if (!deviceConnected || !pWeightChar) return;

    // FIXED: Changed from string to Float32LE binary format to match mobile-racescale app
    float weightValue = displayWeight;
    pWeightChar->setValue((uint8_t*)&weightValue, sizeof(float));
    pWeightChar->notify();

    // Battery percentage (0-100) - placeholder, add actual battery monitoring if available
    if (pBatteryChar) {
        uint8_t batteryPct = 100;  // TODO: Replace with actual battery reading
        pBatteryChar->setValue(&batteryPct, 1);
        pBatteryChar->notify();
    }

    // ✅ UPDATED: Status change only - send as JSON
    static bool lastStableState = false;
    if (isStable != lastStableState && pStatusChar) {
        StaticJsonDocument<128> doc;
        doc["zeroed"] = true;  // Assume tared if running
        doc["calibrated"] = (BASE_CALIBRATION > 0);
        doc["error"] = "";  // Add error message if needed
        String json;
        serializeJson(doc, json);
        pStatusChar->setValue(json.c_str());
        pStatusChar->notify();
        lastStableState = isStable;
    }
}

// ================================================================
// BLE STACK (CrewChiefSteve Standard UUIDs)
// ================================================================

void initializeBLE() {
    BLEDevice::init(deviceName.c_str());

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCB());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Weight (read+notify) - Primary data, Float32LE binary format
    pWeightChar = pService->createCharacteristic(
        WEIGHT_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pWeightChar->addDescriptor(new BLE2902());
    float initialWeight = 0.0f;
    pWeightChar->setValue((uint8_t*)&initialWeight, sizeof(float));

    // Tare command (write)
    pTareChar = pService->createCharacteristic(
        TARE_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pTareChar->setCallbacks(new TareCB());

    // Calibration (write: send known weight, e.g., "25")
    pCalibrationChar = pService->createCharacteristic(
        CALIBRATION_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCalibrationChar->setCallbacks(new CalibrationCB());

    // Temperature (read+notify) - ✅ UPDATED: Float32LE instead of String
    pTempChar = pService->createCharacteristic(
        TEMP_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pTempChar->addDescriptor(new BLE2902());
    float initialTemp = 70.0f;
    pTempChar->setValue((uint8_t*)&initialTemp, sizeof(float));

    // Status (read+notify) - ✅ UPDATED: JSON instead of simple String
    pStatusChar = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pStatusChar->addDescriptor(new BLE2902());
    StaticJsonDocument<128> statusDoc;
    statusDoc["zeroed"] = false;
    statusDoc["calibrated"] = (BASE_CALIBRATION > 0);
    statusDoc["error"] = "";
    String statusJson;
    serializeJson(statusDoc, statusJson);
    pStatusChar->setValue(statusJson.c_str());

    // Corner ID (read+write+notify) - ✅ UPDATED: UInt8 (0-3) instead of String
    pCornerChar = pService->createCharacteristic(
        CORNER_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCornerChar->addDescriptor(new BLE2902());
    pCornerChar->setValue(&cornerIDInt, 1);
    pCornerChar->setCallbacks(new CornerCB());

    // Battery percentage (read+notify) - ADDED to match mobile-racescale app
    pBatteryChar = pService->createCharacteristic(
        BATTERY_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pBatteryChar->addDescriptor(new BLE2902());
    uint8_t initialBattery = 100;
    pBatteryChar->setValue(&initialBattery, 1);

    pService->start();

    // Optimized advertising
    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    pAdv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("📶 BLE Ready: %s\n", deviceName.c_str());
    Serial.println("   Connect from iOS/Android app");
}

// ================================================================
// NVS PERSISTENCE
// ================================================================

void loadSettings() {
    preferences.begin(NVS_NAMESPACE, false);

    // Load calibration factor
    BASE_CALIBRATION = preferences.getFloat(NVS_CAL_KEY, DEFAULT_CALIBRATION);
    Serial.printf("📥 NVS: Cal=%.1f (default=%.1f)\n", BASE_CALIBRATION, DEFAULT_CALIBRATION);

    // Load corner ID (String) and convert to UInt8
    cornerID = preferences.getString(NVS_CORNER_KEY, DEFAULT_CORNER);
    cornerIDInt = cornerStringToUInt8(cornerID);  // ✅ NEW: Convert to UInt8 for BLE
    Serial.printf("📥 NVS: Corner=%s (%d) (default=%s)\n", cornerID.c_str(), cornerIDInt, DEFAULT_CORNER);

    preferences.end();
}

void saveSettings() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putFloat(NVS_CAL_KEY, BASE_CALIBRATION);
    preferences.putString(NVS_CORNER_KEY, cornerID);
    preferences.end();

    Serial.printf("💾 NVS: Saved cal=%.1f, corner=%s\n", BASE_CALIBRATION, cornerID.c_str());
}

// ================================================================
// END OF RACE SCALE V4.0 - PRODUCTION READY (CONFIGURABLE)
// ================================================================

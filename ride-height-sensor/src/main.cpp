/*
 * Laser Ride Height Sensor - ESP32-C3 with dual VL53L1X ToF sensors
 *
 * Hardware:
 * - ESP32-C3 Mini (esp32-c3-devkitm-1)
 * - 2x VL53L1X Time-of-Flight sensors on shared I2C bus
 * - Button for manual trigger, LED for status
 * - Battery voltage monitoring via ADC
 *
 * Features:
 * - Dual sensor measurement with address assignment via XSHUT
 * - BLE interface for wireless data transmission
 * - Continuous and single-shot reading modes
 * - Outlier rejection and averaging
 * - Zero calibration support
 */

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

VL53L1X sensor1;  // First ToF sensor (address 0x30)
VL53L1X sensor2;  // Second ToF sensor (address 0x31)

Preferences preferences;

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pHeightCharacteristic = nullptr;
NimBLECharacteristic* pCommandCharacteristic = nullptr;
NimBLECharacteristic* pStatusCharacteristic = nullptr;
NimBLECharacteristic* pCornerCharacteristic = nullptr;

// ============================================================================
// STATE VARIABLES
// ============================================================================

// Sensor state
bool sensorsInitialized = false;
float sensor1Distance = 0.0;  // mm
float sensor2Distance = 0.0;  // mm
float averageDistance = 0.0;  // mm
float zeroOffset = 0.0;       // mm (calibration offset)

// Battery monitoring
float batteryVoltage = 0.0;  // Volts

// Operating mode
bool continuousMode = false;
bool bleConnected = false;
unsigned long lastContinuousUpdate = 0;

// Device configuration (loaded from NVS)
String cornerID = DEFAULT_CORNER;
String deviceName = String(BLE_DEVICE_NAME_BASE) + "_" + DEFAULT_CORNER;
String deviceStatus = "idle";  // "idle", "measuring", "stable", "error"

// Button handling
volatile bool buttonPressed = false;
volatile unsigned long lastButtonPress = 0;

// LED state
unsigned long lastLedToggle = 0;
bool ledState = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void loadSettings();
void saveZeroOffset();
void performZeroCalibration();
void handleSerialCommands();

// ============================================================================
// BLE SERVER CALLBACKS
// ============================================================================

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        bleConnected = true;
        Serial.println("BLE Client connected");
        digitalWrite(PIN_LED, HIGH);
    }

    void onDisconnect(NimBLEServer* pServer) {
        bleConnected = false;
        continuousMode = false;  // Stop continuous mode on disconnect
        Serial.println("BLE Client disconnected");
        digitalWrite(PIN_LED, LOW);

        // Restart advertising
        NimBLEDevice::startAdvertising();
        Serial.println("Restarted BLE advertising");
    }
};

// ============================================================================
// BLE CORNER CHARACTERISTIC CALLBACKS
// ============================================================================

class CornerCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        String newCorner = String(value.c_str());
        newCorner.trim();
        newCorner.toUpperCase();

        // Validate corner ID (LF, RF, LR, RR, or 01-99)
        if (newCorner == "LF" || newCorner == "RF" || newCorner == "LR" || newCorner == "RR" ||
            (newCorner.length() == 2 && isDigit(newCorner[0]) && isDigit(newCorner[1]))) {

            cornerID = newCorner;

            // Save to NVS
            preferences.begin(NVS_NAMESPACE, false);
            preferences.putString(NVS_CORNER_KEY, cornerID);
            preferences.end();

            // Update characteristic value
            pCharacteristic->setValue(cornerID.c_str());
            pCharacteristic->notify();

            Serial.printf("✓ Corner ID set to: %s (saved to NVS)\n", cornerID.c_str());
            Serial.println("  Restart device to update BLE name");
        } else {
            Serial.printf("✗ Invalid corner ID: %s (use LF/RF/LR/RR or 01-99)\n", newCorner.c_str());
        }
    }

    void onRead(NimBLECharacteristic* pCharacteristic) {
        pCharacteristic->setValue(cornerID.c_str());
    }
};

// ============================================================================
// BLE COMMAND CHARACTERISTIC CALLBACKS
// ============================================================================

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0) {
            char cmd = value[0];
            Serial.printf("BLE Command received: %c\n", cmd);

            switch (cmd) {
                case CMD_SINGLE_READING:
                    Serial.println("Single reading triggered via BLE");
                    buttonPressed = true;  // Trigger reading
                    break;

                case CMD_CONTINUOUS_START:
                    Serial.println("Continuous mode started");
                    continuousMode = true;
                    lastContinuousUpdate = 0;  // Force immediate update
                    deviceStatus = "continuous";
                    if (pStatusCharacteristic != nullptr) {
                        pStatusCharacteristic->setValue(deviceStatus.c_str());
                        pStatusCharacteristic->notify();
                    }
                    break;

                case CMD_CONTINUOUS_STOP:
                    Serial.println("Continuous mode stopped");
                    continuousMode = false;
                    deviceStatus = "idle";
                    if (pStatusCharacteristic != nullptr) {
                        pStatusCharacteristic->setValue(deviceStatus.c_str());
                        pStatusCharacteristic->notify();
                    }
                    break;

                case CMD_ZERO_CALIBRATION:
                    Serial.println("Zero calibration requested");
                    if (sensorsInitialized) {
                        performZeroCalibration();
                    } else {
                        Serial.println("ERROR: Sensors not initialized!");
                    }
                    break;

                default:
                    Serial.printf("Unknown command: %c\n", cmd);
                    break;
            }
        }
    }
};

// ============================================================================
// NVS SETTINGS FUNCTIONS
// ============================================================================

void loadSettings() {
    preferences.begin(NVS_NAMESPACE, false);
    cornerID = preferences.getString(NVS_CORNER_KEY, DEFAULT_CORNER);
    zeroOffset = preferences.getFloat(NVS_ZERO_OFFSET_KEY, 0.0);
    preferences.end();

    deviceName = String(BLE_DEVICE_NAME_BASE) + "_" + cornerID;

    Serial.println("=== Settings loaded from NVS ===");
    Serial.printf("Corner ID: %s\n", cornerID.c_str());
    Serial.printf("Zero offset: %.1f mm\n", zeroOffset);
    Serial.printf("Device name: %s\n", deviceName.c_str());
}

void saveZeroOffset() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putFloat(NVS_ZERO_OFFSET_KEY, zeroOffset);
    preferences.end();
    Serial.printf("Zero offset saved to NVS: %.1f mm\n", zeroOffset);
}

// ============================================================================
// SERIAL COMMAND HANDLER
// ============================================================================

void handleSerialCommands() {
    if (!Serial.available()) {
        return;
    }

    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command.startsWith("corner ")) {
        // Set corner ID
        String newCorner = command.substring(7);
        newCorner.trim();
        newCorner.toUpperCase();

        // Validate corner ID
        if (newCorner == "LF" || newCorner == "RF" || newCorner == "LR" || newCorner == "RR" ||
            (newCorner.length() == 2 && isDigit(newCorner[0]) && isDigit(newCorner[1]))) {

            cornerID = newCorner;

            // Save to NVS
            preferences.begin(NVS_NAMESPACE, false);
            preferences.putString(NVS_CORNER_KEY, cornerID);
            preferences.end();

            deviceName = String(BLE_DEVICE_NAME_BASE) + "_" + cornerID;

            Serial.printf("\n✓ Corner ID set to: %s\n", cornerID.c_str());
            Serial.printf("  Device name: %s\n", deviceName.c_str());
            Serial.println("  ⚠ Restart device to update BLE name\n");

            // Update BLE characteristic if available
            if (pCornerCharacteristic != nullptr) {
                pCornerCharacteristic->setValue(cornerID.c_str());
                pCornerCharacteristic->notify();
            }
        } else {
            Serial.printf("\n✗ Invalid corner ID: %s\n", newCorner.c_str());
            Serial.println("  Valid formats: LF, RF, LR, RR, or 01-99\n");
        }
    }
    else if (command == "info") {
        // Display current settings
        Serial.println("\n=== Device Information ===");
        Serial.printf("Device name: %s\n", deviceName.c_str());
        Serial.printf("Corner ID: %s\n", cornerID.c_str());
        Serial.printf("Zero offset: %.1f mm\n", zeroOffset);
        Serial.printf("BLE connected: %s\n", bleConnected ? "Yes" : "No");
        Serial.printf("Continuous mode: %s\n", continuousMode ? "Yes" : "No");
        Serial.printf("Status: %s\n", deviceStatus.c_str());
        Serial.println();
    }
    else if (command == "zero") {
        // Perform zero calibration
        Serial.println("\n✓ Starting zero calibration...");
        performZeroCalibration();
    }
    else if (command == "help") {
        // Show help
        Serial.println("\n=== Available Commands ===");
        Serial.println("corner <ID>  - Set corner identity (LF, RF, LR, RR, or 01-99)");
        Serial.println("               Example: corner LF");
        Serial.println("info         - Display current settings");
        Serial.println("zero         - Perform zero calibration");
        Serial.println("help         - Show this help message");
        Serial.println();
    }
    else if (command.length() > 0) {
        Serial.printf("\n✗ Unknown command: %s\n", command.c_str());
        Serial.println("  Type 'help' for available commands\n");
    }
}

// ============================================================================
// BUTTON INTERRUPT HANDLER
// ============================================================================

void IRAM_ATTR buttonISR() {
    unsigned long currentTime = millis();

    // Debounce: ignore if pressed within debounce period
    if (currentTime - lastButtonPress > BUTTON_DEBOUNCE_MS) {
        buttonPressed = true;
        lastButtonPress = currentTime;
    }
}

// ============================================================================
// SENSOR INITIALIZATION
// ============================================================================

bool initializeSensors() {
    Serial.println("\n=== Initializing VL53L1X Sensors ===");

    // Configure I2C bus
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);  // 400kHz I2C speed

    // Configure XSHUT pins as outputs
    pinMode(PIN_XSHUT_SENSOR1, OUTPUT);
    pinMode(PIN_XSHUT_SENSOR2, OUTPUT);

    // Hold both sensors in reset (XSHUT LOW)
    digitalWrite(PIN_XSHUT_SENSOR1, LOW);
    digitalWrite(PIN_XSHUT_SENSOR2, LOW);
    delay(10);

    // === Initialize Sensor 1 ===
    Serial.println("Initializing Sensor 1...");
    digitalWrite(PIN_XSHUT_SENSOR1, HIGH);  // Release sensor 1 from reset
    delay(10);

    sensor1.setBus(&Wire);
    sensor1.setTimeout(500);

    if (!sensor1.init()) {
        Serial.println("ERROR: Failed to initialize Sensor 1!");
        return false;
    }

    // Change sensor 1 address from default 0x29 to 0x30
    sensor1.setAddress(SENSOR1_ADDRESS);
    Serial.printf("Sensor 1 address set to 0x%02X\n", SENSOR1_ADDRESS);

    // === Initialize Sensor 2 ===
    Serial.println("Initializing Sensor 2...");
    digitalWrite(PIN_XSHUT_SENSOR2, HIGH);  // Release sensor 2 from reset
    delay(10);

    sensor2.setBus(&Wire);
    sensor2.setTimeout(500);

    if (!sensor2.init()) {
        Serial.println("ERROR: Failed to initialize Sensor 2!");
        return false;
    }

    // Change sensor 2 address from default 0x29 to 0x31
    sensor2.setAddress(SENSOR2_ADDRESS);
    Serial.printf("Sensor 2 address set to 0x%02X\n", SENSOR2_ADDRESS);

    // === Configure both sensors ===
    // Set distance mode (short=1.3m, long=4m)
    if (DISTANCE_MODE_LONG) {
        sensor1.setDistanceMode(VL53L1X::Long);
        sensor2.setDistanceMode(VL53L1X::Long);
        Serial.println("Distance mode: LONG (4m range)");
    } else {
        sensor1.setDistanceMode(VL53L1X::Short);
        sensor2.setDistanceMode(VL53L1X::Short);
        Serial.println("Distance mode: SHORT (1.3m range)");
    }

    // Set timing budget (measurement time)
    sensor1.setMeasurementTimingBudget(TIMING_BUDGET_MS * 1000);  // Convert ms to us
    sensor2.setMeasurementTimingBudget(TIMING_BUDGET_MS * 1000);
    Serial.printf("Timing budget: %d ms (~%d Hz)\n", TIMING_BUDGET_MS, 1000 / TIMING_BUDGET_MS);

    // Start continuous ranging
    sensor1.startContinuous(TIMING_BUDGET_MS);
    sensor2.startContinuous(TIMING_BUDGET_MS);
    Serial.println("Continuous ranging started on both sensors");

    Serial.println("=== Sensor initialization complete ===\n");
    return true;
}

// ============================================================================
// BLE INITIALIZATION
// ============================================================================

void initializeBLE() {
    Serial.println("\n=== Initializing BLE ===");

    // Initialize NimBLE with dynamic device name
    NimBLEDevice::init(deviceName.c_str());
    NimBLEDevice::setMTU(BLE_MTU_SIZE);

    // Create BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create BLE Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Create Height Characteristic (READ + NOTIFY)
    pHeightCharacteristic = pService->createCharacteristic(
        CHAR_HEIGHT_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Create Command Characteristic (WRITE)
    pCommandCharacteristic = pService->createCharacteristic(
        CHAR_COMMAND_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    // Create Status Characteristic (READ + NOTIFY)
    pStatusCharacteristic = pService->createCharacteristic(
        CHAR_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pStatusCharacteristic->setValue(deviceStatus.c_str());

    // Create Corner ID Characteristic (READ + WRITE + NOTIFY)
    pCornerCharacteristic = pService->createCharacteristic(
        CHAR_CORNER_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    pCornerCharacteristic->setCallbacks(new CornerCallbacks());
    pCornerCharacteristic->setValue(cornerID.c_str());

    // Start the service
    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.printf("BLE Device: %s\n", deviceName.c_str());
    Serial.printf("Corner ID: %s\n", cornerID.c_str());
    Serial.printf("Service UUID: %s\n", SERVICE_UUID);
    Serial.println("BLE advertising started");
    Serial.println("=== BLE initialization complete ===\n");
}

// ============================================================================
// SENSOR READING
// ============================================================================

void readSensors() {
    if (!sensorsInitialized) {
        Serial.println("ERROR: Sensors not initialized!");
        return;
    }

    // Blink LED during reading
    digitalWrite(PIN_LED, HIGH);

    // Read sensor 1
    uint16_t distance1 = sensor1.read(false);  // false = non-blocking
    if (sensor1.timeoutOccurred()) {
        Serial.println("WARNING: Sensor 1 timeout!");
        sensor1Distance = -1.0;
    } else {
        sensor1Distance = distance1;
    }

    // Read sensor 2
    uint16_t distance2 = sensor2.read(false);
    if (sensor2.timeoutOccurred()) {
        Serial.println("WARNING: Sensor 2 timeout!");
        sensor2Distance = -1.0;
    } else {
        sensor2Distance = distance2;
    }

    // Calculate average with outlier rejection
    if (sensor1Distance > 0 && sensor2Distance > 0) {
        float delta = abs(sensor1Distance - sensor2Distance);

        if (delta > OUTLIER_THRESHOLD_MM) {
            // Use lower value (closer reading is more reliable)
            averageDistance = min(sensor1Distance, sensor2Distance);
            Serial.printf("Outlier detected (delta: %.1f mm), using lower value\n", delta);
        } else {
            // Both readings are close, use average
            averageDistance = (sensor1Distance + sensor2Distance) / 2.0;
        }

        // Apply zero calibration offset
        averageDistance -= zeroOffset;

    } else if (sensor1Distance > 0) {
        averageDistance = sensor1Distance - zeroOffset;
        Serial.println("Using only Sensor 1 (Sensor 2 failed)");
    } else if (sensor2Distance > 0) {
        averageDistance = sensor2Distance - zeroOffset;
        Serial.println("Using only Sensor 2 (Sensor 1 failed)");
    } else {
        averageDistance = -1.0;  // Both sensors failed
        Serial.println("ERROR: Both sensors failed!");
    }

    digitalWrite(PIN_LED, LOW);
}

// ============================================================================
// BATTERY VOLTAGE READING
// ============================================================================

void readBatteryVoltage() {
    int adcValue = analogRead(PIN_BATTERY_ADC);

    // Convert ADC value to voltage
    float voltage = (adcValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE;

    // Apply voltage divider ratio
    batteryVoltage = voltage * VOLTAGE_DIVIDER_RATIO;
}

// ============================================================================
// DATA TRANSMISSION
// ============================================================================

void transmitData() {
    // Read battery voltage
    readBatteryVoltage();

    // Convert mm to inches
    float averageInches = averageDistance * 0.03937008f;

    // Format data string: "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"
    char dataBuffer[DATA_STRING_MAX_LENGTH];
    snprintf(dataBuffer, DATA_STRING_MAX_LENGTH,
             "S1:%.1f,S2:%.1f,AVG:%.1f,IN:%.2f,BAT:%.2f",
             sensor1Distance, sensor2Distance, averageDistance, averageInches, batteryVoltage);

    // Print to serial
    Serial.printf("Data: %s\n", dataBuffer);

    // Send via BLE if connected
    if (bleConnected && pHeightCharacteristic != nullptr) {
        pHeightCharacteristic->setValue(dataBuffer);
        pHeightCharacteristic->notify();
    }
}

// ============================================================================
// ZERO CALIBRATION
// ============================================================================

void performZeroCalibration() {
    Serial.println("\n=== Zero Calibration ===");

    // Update status
    deviceStatus = "calibrating";
    if (pStatusCharacteristic != nullptr) {
        pStatusCharacteristic->setValue(deviceStatus.c_str());
        pStatusCharacteristic->notify();
    }

    // Take a fresh reading
    readSensors();

    if (averageDistance > 0 && averageDistance < ZERO_OFFSET_MAX_MM) {
        zeroOffset = averageDistance + zeroOffset;  // Add to existing offset
        Serial.printf("Zero offset set to: %.1f mm\n", zeroOffset);

        // Save to NVS
        saveZeroOffset();

        // Blink LED to confirm
        for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_LED, HIGH);
            delay(100);
            digitalWrite(PIN_LED, LOW);
            delay(100);
        }
    } else {
        Serial.println("ERROR: Invalid reading for zero calibration!");
        // Blink LED rapidly to indicate error
        for (int i = 0; i < 5; i++) {
            digitalWrite(PIN_LED, HIGH);
            delay(50);
            digitalWrite(PIN_LED, LOW);
            delay(50);
        }
    }

    // Return to idle status
    deviceStatus = "idle";
    if (pStatusCharacteristic != nullptr) {
        pStatusCharacteristic->setValue(deviceStatus.c_str());
        pStatusCharacteristic->notify();
    }

    Serial.println("=== Calibration complete ===\n");
}

// ============================================================================
// LED HEARTBEAT
// ============================================================================

void updateLED() {
    if (bleConnected && !continuousMode) {
        // Heartbeat pattern when connected but idle
        unsigned long currentTime = millis();
        if (currentTime - lastLedToggle > LED_BLINK_CONNECTED) {
            ledState = !ledState;
            digitalWrite(PIN_LED, ledState);
            lastLedToggle = currentTime;
        }
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n");
    Serial.println("================================================");
    Serial.println("  Laser Ride Height Sensor - ESP32-C3");
    Serial.println("  Dual VL53L1X ToF + BLE Interface");
    Serial.println("================================================");

    // Configure GPIO pins
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BATTERY_ADC, INPUT);

    digitalWrite(PIN_LED, LOW);

    // Attach button interrupt
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), buttonISR, FALLING);
    Serial.println("Button interrupt attached (GPIO9)");

    // Initialize sensors
    sensorsInitialized = initializeSensors();

    if (!sensorsInitialized) {
        Serial.println("\n*** SENSOR INITIALIZATION FAILED ***");
        Serial.println("System halted. Please check wiring and reset.");

        // Blink LED rapidly to indicate error
        while (true) {
            digitalWrite(PIN_LED, HIGH);
            delay(LED_BLINK_ERROR);
            digitalWrite(PIN_LED, LOW);
            delay(LED_BLINK_ERROR);
        }
    }

    // Load settings from NVS (corner ID, zero offset)
    loadSettings();

    // Initialize BLE
    initializeBLE();

    Serial.println("\n=== System Ready ===");
    Serial.println("Press button or send BLE command to start reading");
    Serial.println("BLE Commands: R=single, C=continuous, S=stop, Z=zero\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    unsigned long currentTime = millis();

    // Handle serial commands
    handleSerialCommands();

    // Handle button press (single reading)
    if (buttonPressed) {
        buttonPressed = false;
        Serial.println("\n--- Button pressed: Single reading ---");

        readSensors();
        transmitData();

        // Check if this was part of zero calibration
        // (User should send 'Z' command, then system takes reading and calibrates)
        // For simplicity, zero calibration is a separate flow
    }

    // Handle continuous mode
    if (continuousMode && (currentTime - lastContinuousUpdate >= CONTINUOUS_UPDATE_INTERVAL_MS)) {
        readSensors();
        transmitData();
        lastContinuousUpdate = currentTime;
    }

    // Update LED heartbeat
    updateLED();

    // Small delay to prevent watchdog issues
    delay(1);
}

/**
 * RaceScale BLE Mock Firmware
 *
 * Single corner scale - configurable via serial as LF, RF, LR, or RR
 * Mobile app connects to 4 separate ESP32 instances simultaneously
 *
 * UUIDs MUST MATCH: packages/ble/src/constants/uuids.ts
 * See: SERVICE_UUIDS.RACESCALE, RACESCALE_CHARS
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// BLE UUIDs - packages/ble: SERVICE_UUIDS.RACESCALE, RACESCALE_CHARS.*
#define SERVICE_UUID_RACESCALE   "4fafc201-0002-459e-8fcc-c5c9c331914b"
#define CHAR_SCALE_WEIGHT        "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // WEIGHT - Float32LE
#define CHAR_SCALE_TARE          "beb5483e-36e1-4688-b7f5-ea07361b26ad"  // TARE - Write UInt8
#define CHAR_SCALE_CALIB         "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // CALIBRATION - Float32LE
#define CHAR_SCALE_TEMP          "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // TEMPERATURE - Float32LE
#define CHAR_SCALE_STATUS        "beb5483e-36e1-4688-b7f5-ea07361b26ac"  // STATUS - JSON string
#define CHAR_SCALE_BATTERY       "beb5483e-36e1-4688-b7f5-ea07361b26ae"  // BATTERY - UInt8 0-100
#define CHAR_SCALE_CORNER        "beb5483e-36e1-4688-b7f5-ea07361b26af"  // CORNER_ID - UInt8 (0=LF, 1=RF, 2=LR, 3=RR)

// Button pin for tare/config
#define BUTTON_PIN 0  // BOOT button on ESP32 dev board

// Global state
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pWeightChar = nullptr;
NimBLECharacteristic* pTareChar = nullptr;
NimBLECharacteristic* pCalibChar = nullptr;
NimBLECharacteristic* pTempChar = nullptr;
NimBLECharacteristic* pStatusChar = nullptr;
NimBLECharacteristic* pBatteryChar = nullptr;
NimBLECharacteristic* pCornerChar = nullptr;

Preferences prefs;
String cornerID = "LF";  // Default corner
float baseWeight = 285.5;  // Base weight in lbs
float currentWeight = 285.5;
float variance = 0.5;  // Weight variance during settling
float scaleTemp = 72.0;  // Scale temperature in °F
uint8_t battery = 85;  // Battery percentage
bool deviceConnected = false;
unsigned long lastUpdate = 0;
unsigned long tareTime = 0;
bool isSettling = false;

// Weight simulation
const float CORNER_WEIGHTS[] = {285.5, 292.3, 278.1, 295.8};  // LF, RF, LR, RR typical
int cornerIndex = 0;

class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
        delay(500);
        NimBLEDevice::startAdvertising();
        Serial.println("Advertising restarted");
    }
};

class TareCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            uint8_t cmd = value[0];
            Serial.printf("TARE command received: 0x%02X\n", cmd);

            // Reset to zero weight
            baseWeight = 0.0;
            currentWeight = 0.0;
            tareTime = millis();
            isSettling = true;

            Serial.println("Weight TARED to 0.0 lbs");
        }
    }
};

void loadCornerConfig() {
    prefs.begin("racescale", false);
    cornerID = prefs.getString("corner", "LF");
    prefs.end();

    // Set base weight based on corner
    if (cornerID == "LF") cornerIndex = 0;
    else if (cornerID == "RF") cornerIndex = 1;
    else if (cornerID == "LR") cornerIndex = 2;
    else if (cornerID == "RR") cornerIndex = 3;

    baseWeight = CORNER_WEIGHTS[cornerIndex];
    currentWeight = baseWeight;

    Serial.printf("Loaded corner: %s, base weight: %.1f lbs\n", cornerID.c_str(), baseWeight);
}

void saveCornerConfig(String corner) {
    prefs.begin("racescale", false);
    prefs.putString("corner", corner);
    prefs.end();
    Serial.printf("Saved corner: %s to NVS\n", corner.c_str());
}

void setupBLE() {
    String deviceName = "RaceScale_" + cornerID;
    NimBLEDevice::init(deviceName.c_str());
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID_RACESCALE);

    // Weight characteristic - Float32LE binary, notify
    pWeightChar = pService->createCharacteristic(
        CHAR_SCALE_WEIGHT,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Tare characteristic - Write only
    pTareChar = pService->createCharacteristic(
        CHAR_SCALE_TARE,
        NIMBLE_PROPERTY::WRITE
    );
    pTareChar->setCallbacks(new TareCallbacks());

    // Calibration characteristic - Float32LE
    pCalibChar = pService->createCharacteristic(
        CHAR_SCALE_CALIB,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Temperature characteristic - Float32LE
    pTempChar = pService->createCharacteristic(
        CHAR_SCALE_TEMP,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Status characteristic - JSON string
    pStatusChar = pService->createCharacteristic(
        CHAR_SCALE_STATUS,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Battery characteristic - UInt8
    pBatteryChar = pService->createCharacteristic(
        CHAR_SCALE_BATTERY,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Corner ID characteristic - UInt8 (0=LF, 1=RF, 2=LR, 3=RR)
    pCornerChar = pService->createCharacteristic(
        CHAR_SCALE_CORNER,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );

    // Set initial values
    uint8_t cornerIdInt = cornerIndex;  // Use numeric index (0-3)
    pCornerChar->setValue(&cornerIdInt, 1);

    float calibValue = 1.0;
    pCalibChar->setValue((uint8_t*)&calibValue, sizeof(float));

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_RACESCALE);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.printf("BLE started: RaceScale_%s\n", cornerID.c_str());
    Serial.println("Waiting for mobile app connection...");
}

void updateWeight() {
    // Simulate weight settling after tare
    if (isSettling) {
        unsigned long elapsed = millis() - tareTime;
        if (elapsed < 3000) {
            // Settling phase: variance decreases over 3 seconds
            variance = 0.5 - (elapsed / 3000.0) * 0.4;  // 0.5 -> 0.1
        } else {
            isSettling = false;
            variance = 0.1;
        }
    }

    // Add realistic variance
    float noise = (random(-100, 100) / 100.0) * variance;
    currentWeight = baseWeight + noise;

    // Slowly drift battery down
    if (random(0, 100) < 1) {  // 1% chance per update
        battery = max(0, battery - 1);
    }

    // Slight temperature drift
    scaleTemp += (random(-10, 10) / 100.0);
    scaleTemp = constrain(scaleTemp, 65.0, 80.0);
}

void sendBLEUpdates() {
    if (!deviceConnected) return;

    // Weight - Float32LE binary (CRITICAL: NOT a string!)
    pWeightChar->setValue((uint8_t*)&currentWeight, sizeof(float));
    pWeightChar->notify();

    // Temperature - Float32LE
    pTempChar->setValue((uint8_t*)&scaleTemp, sizeof(float));
    pTempChar->notify();

    // Battery - UInt8
    pBatteryChar->setValue(&battery, 1);
    pBatteryChar->notify();

    // Status - JSON string
    JsonDocument doc;
    doc["weight"] = currentWeight;
    doc["stable"] = !isSettling;
    doc["battery"] = battery;

    String statusJson;
    serializeJson(doc, statusJson);
    pStatusChar->setValue(statusJson.c_str());
    pStatusChar->notify();

    Serial.printf("[%s] Weight: %.2f lbs | Temp: %.1f°F | Bat: %d%% | Stable: %s\n",
                  cornerID.c_str(), currentWeight, scaleTemp, battery,
                  isSettling ? "NO" : "YES");
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toUpperCase();

        if (input == "LF" || input == "RF" || input == "LR" || input == "RR") {
            cornerID = input;
            saveCornerConfig(cornerID);

            // Update base weight
            if (cornerID == "LF") cornerIndex = 0;
            else if (cornerID == "RF") cornerIndex = 1;
            else if (cornerID == "LR") cornerIndex = 2;
            else if (cornerID == "RR") cornerIndex = 3;

            baseWeight = CORNER_WEIGHTS[cornerIndex];
            currentWeight = baseWeight;

            // Update BLE corner characteristic (UInt8)
            if (pCornerChar) {
                uint8_t cornerIdInt = cornerIndex;
                pCornerChar->setValue(&cornerIdInt, 1);
                pCornerChar->notify();
            }

            Serial.printf("Corner changed to: %s (%.1f lbs)\n", cornerID.c_str(), baseWeight);
            Serial.println("Restart ESP32 to update BLE device name");
        }
        else if (input == "TARE") {
            baseWeight = 0.0;
            currentWeight = 0.0;
            tareTime = millis();
            isSettling = true;
            Serial.println("Manual TARE via serial");
        }
        else if (input == "STATUS") {
            Serial.printf("Corner: %s\n", cornerID.c_str());
            Serial.printf("Weight: %.2f lbs\n", currentWeight);
            Serial.printf("Battery: %d%%\n", battery);
            Serial.printf("Temperature: %.1f°F\n", scaleTemp);
            Serial.printf("Connected: %s\n", deviceConnected ? "YES" : "NO");
        }
        else {
            Serial.println("Commands: LF, RF, LR, RR, TARE, STATUS");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== RaceScale BLE Mock Firmware ===");
    Serial.println("Single corner scale - configurable");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    loadCornerConfig();
    setupBLE();

    Serial.println("\nSerial Commands:");
    Serial.println("  LF, RF, LR, RR - Set corner identity");
    Serial.println("  TARE - Zero the weight");
    Serial.println("  STATUS - Show current state");
    Serial.println("\nButton: Press BOOT to tare");
}

void loop() {
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;

    // Button handling for tare
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH &&
        (millis() - lastButtonPress > 500)) {
        lastButtonPress = millis();
        baseWeight = 0.0;
        currentWeight = 0.0;
        tareTime = millis();
        isSettling = true;
        Serial.println("BUTTON TARE");
    }
    lastButtonState = buttonState;

    // Update weight simulation and send BLE notifications every 500ms
    if (millis() - lastUpdate >= 500) {
        lastUpdate = millis();
        updateWeight();
        sendBLEUpdates();
    }

    handleSerial();
    delay(10);
}

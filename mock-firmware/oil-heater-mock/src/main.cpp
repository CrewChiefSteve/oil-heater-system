/**
 * Oil Heater BLE Mock Firmware
 *
 * Simulates oil heater controller with temperature, setpoint, and status
 *
 * UUIDs MUST MATCH: packages/ble/src/constants/uuids.ts
 * See: SERVICE_UUIDS.OIL_HEATER, OIL_HEATER_CHARS
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

// BLE UUIDs - packages/ble: SERVICE_UUIDS.OIL_HEATER, OIL_HEATER_CHARS.*
#define SERVICE_UUID_OIL_HEATER  "4fafc201-0001-459e-8fcc-c5c9c331914b"
#define CHAR_HEATER_TEMP         "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // TEMPERATURE - ASCII string
#define CHAR_HEATER_SETPOINT     "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // SETPOINT - ASCII string
#define CHAR_HEATER_STATUS       "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // STATUS - JSON

// Button pin for fault simulation
#define BUTTON_PIN 0  // BOOT button on ESP32 dev board

// Global state
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTempChar = nullptr;
NimBLECharacteristic* pSetpointChar = nullptr;
NimBLECharacteristic* pStatusChar = nullptr;

float currentTemp = 70.0;  // °F
float setpointTemp = 180.0;  // °F
bool heaterOn = false;
bool safetyShutdown = false;
bool sensorError = false;
bool deviceConnected = false;
unsigned long lastUpdate = 0;

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

class SetpointCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            float newSetpoint = atof(value.c_str());
            if (newSetpoint >= 100.0 && newSetpoint <= 250.0) {
                setpointTemp = newSetpoint;
                safetyShutdown = false;  // Clear safety on setpoint change
                Serial.printf("Setpoint changed to: %.1f°F\n", setpointTemp);
            } else {
                Serial.printf("Invalid setpoint: %.1f (must be 100-250°F)\n", newSetpoint);
            }
        }
    }
};

void setupBLE() {
    NimBLEDevice::init("Heater_Mock");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID_OIL_HEATER);

    // Temperature characteristic - ASCII string (e.g., "185.5")
    pTempChar = pService->createCharacteristic(
        CHAR_HEATER_TEMP,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Setpoint characteristic - ASCII string, read/write
    pSetpointChar = pService->createCharacteristic(
        CHAR_HEATER_SETPOINT,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    pSetpointChar->setCallbacks(new SetpointCallbacks());

    // Status characteristic - JSON string
    pStatusChar = pService->createCharacteristic(
        CHAR_HEATER_STATUS,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Set initial values
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f", currentTemp);
    pTempChar->setValue(tempStr);

    char setpointStr[16];
    snprintf(setpointStr, sizeof(setpointStr), "%.1f", setpointTemp);
    pSetpointChar->setValue(setpointStr);

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_OIL_HEATER);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE started: Heater_Mock");
    Serial.println("Waiting for mobile app connection...");
}

void updateHeaterSimulation() {
    // Heater logic
    if (safetyShutdown || sensorError) {
        heaterOn = false;
    } else {
        // Simple bang-bang control with hysteresis
        if (currentTemp < setpointTemp - 5.0) {
            heaterOn = true;
        } else if (currentTemp > setpointTemp + 2.0) {
            heaterOn = false;
        }
    }

    // Temperature dynamics
    if (heaterOn) {
        // Heat up: ~2°F per second
        currentTemp += 2.0;
    } else {
        // Cool down: ~0.5°F per second
        currentTemp -= 0.5;
    }

    // Clamp temperature
    currentTemp = constrain(currentTemp, 50.0, 300.0);

    // Safety check
    if (currentTemp > 290.0) {
        safetyShutdown = true;
        Serial.println("SAFETY SHUTDOWN: Over-temperature!");
    }
}

void sendBLEUpdates() {
    if (!deviceConnected) return;

    // Temperature - ASCII string
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f", currentTemp);
    pTempChar->setValue(tempStr);
    pTempChar->notify();

    // Setpoint - ASCII string
    char setpointStr[16];
    snprintf(setpointStr, sizeof(setpointStr), "%.1f", setpointTemp);
    pSetpointChar->setValue(setpointStr);
    pSetpointChar->notify();

    // Status - JSON (EXACT format)
    StaticJsonDocument<128> doc;
    doc["heater"] = heaterOn;
    doc["safetyShutdown"] = safetyShutdown;
    doc["sensorError"] = sensorError;

    String statusJson;
    serializeJson(doc, statusJson);
    pStatusChar->setValue(statusJson.c_str());
    pStatusChar->notify();

    Serial.printf("Temp: %.1f°F | Setpoint: %.1f°F | Heater: %s | Safety: %s\n",
                  currentTemp, setpointTemp,
                  heaterOn ? "ON" : "OFF",
                  safetyShutdown ? "SHUTDOWN" : "OK");
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toUpperCase();

        if (input.startsWith("SET ")) {
            float newSetpoint = input.substring(4).toFloat();
            if (newSetpoint >= 100.0 && newSetpoint <= 250.0) {
                setpointTemp = newSetpoint;
                safetyShutdown = false;
                Serial.printf("Setpoint set to: %.1f°F\n", setpointTemp);
            } else {
                Serial.println("Setpoint must be 100-250°F");
            }
        }
        else if (input == "FAULT") {
            safetyShutdown = !safetyShutdown;
            Serial.printf("Safety shutdown: %s\n", safetyShutdown ? "ACTIVE" : "CLEARED");
        }
        else if (input == "SENSOR") {
            sensorError = !sensorError;
            Serial.printf("Sensor error: %s\n", sensorError ? "ACTIVE" : "CLEARED");
        }
        else if (input == "STATUS") {
            Serial.printf("Current Temp: %.1f°F\n", currentTemp);
            Serial.printf("Setpoint: %.1f°F\n", setpointTemp);
            Serial.printf("Heater: %s\n", heaterOn ? "ON" : "OFF");
            Serial.printf("Safety Shutdown: %s\n", safetyShutdown ? "YES" : "NO");
            Serial.printf("Sensor Error: %s\n", sensorError ? "YES" : "NO");
            Serial.printf("Connected: %s\n", deviceConnected ? "YES" : "NO");
        }
        else {
            Serial.println("Commands: SET <temp>, FAULT, SENSOR, STATUS");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== Oil Heater BLE Mock Firmware ===");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    setupBLE();

    Serial.println("\nSerial Commands:");
    Serial.println("  SET <temp> - Set target temperature (100-250°F)");
    Serial.println("  FAULT - Toggle safety shutdown");
    Serial.println("  SENSOR - Toggle sensor error");
    Serial.println("  STATUS - Show current state");
    Serial.println("\nButton: Press BOOT to toggle fault");
}

void loop() {
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;

    // Button handling for fault simulation
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH &&
        (millis() - lastButtonPress > 500)) {
        lastButtonPress = millis();
        safetyShutdown = !safetyShutdown;
        Serial.printf("BUTTON: Safety shutdown %s\n", safetyShutdown ? "ACTIVE" : "CLEARED");
    }
    lastButtonState = buttonState;

    // Update simulation and send BLE notifications every 1 second
    if (millis() - lastUpdate >= 1000) {
        lastUpdate = millis();
        updateHeaterSimulation();
        sendBLEUpdates();
    }

    handleSerial();
    delay(10);
}

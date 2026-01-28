/**
 * Tire Temperature Gun BLE Mock Firmware
 *
 * Simulates IR temperature gun with spot/continuous modes
 *
 * UUIDs MUST MATCH: packages/ble/src/constants/uuids.ts
 * See: SERVICE_UUIDS.TIRE_TEMP_GUN, TIRE_TEMP_GUN_CHARS
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

// BLE UUIDs - packages/ble: SERVICE_UUIDS.TIRE_TEMP_GUN, TIRE_TEMP_GUN_CHARS.*
#define SERVICE_UUID_TIRE_TEMP   "4fafc201-0005-459e-8fcc-c5c9c331914b"
#define CHAR_GUN_TEMP            "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // TEMPERATURE - JSON
#define CHAR_GUN_CMD             "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // COMMAND - Write string

// Button pin for triggering readings
#define BUTTON_PIN 0  // BOOT button on ESP32 dev board

// Global state
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTempChar = nullptr;
NimBLECharacteristic* pCmdChar = nullptr;

float currentTemp = 185.5;
float ambientTemp = 72.3;
float sessionMax = 185.5;
float sessionMin = 185.5;
float emissivity = 0.95;
uint8_t battery = 85;
uint8_t mode = 0;  // 0=spot, 1=continuous
char unit = 'F';   // 'F' or 'C'
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

class CommandCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String cmd = String(value.c_str());
            cmd.trim();
            Serial.printf("Command received: '%s'\n", cmd.c_str());

            if (cmd.startsWith("EMIT:")) {
                float newEmit = cmd.substring(5).toFloat();
                if (newEmit >= 0.1 && newEmit <= 1.0) {
                    emissivity = newEmit;
                    Serial.printf("Emissivity set to: %.2f\n", emissivity);
                }
            }
            else if (cmd.startsWith("UNIT:")) {
                char newUnit = cmd.charAt(5);
                if (newUnit == 'F' || newUnit == 'C') {
                    unit = newUnit;
                    Serial.printf("Unit set to: %c\n", unit);
                }
            }
            else if (cmd == "RESET") {
                sessionMax = currentTemp;
                sessionMin = currentTemp;
                Serial.println("Session max/min RESET");
            }
            else if (cmd == "MODE:SPOT") {
                mode = 0;
                Serial.println("Mode: SPOT");
            }
            else if (cmd == "MODE:CONTINUOUS") {
                mode = 1;
                Serial.println("Mode: CONTINUOUS");
            }
            else {
                Serial.printf("Unknown command: '%s'\n", cmd.c_str());
            }
        }
    }
};

void setupBLE() {
    NimBLEDevice::init("TireTempGun");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID_TIRE_TEMP);

    // Temperature characteristic - JSON
    pTempChar = pService->createCharacteristic(
        CHAR_GUN_TEMP,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Command characteristic - Write string
    pCmdChar = pService->createCharacteristic(
        CHAR_GUN_CMD,
        NIMBLE_PROPERTY::WRITE
    );
    pCmdChar->setCallbacks(new CommandCallbacks());

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_TIRE_TEMP);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE started: TireTempGun");
    Serial.println("Waiting for mobile app connection...");
}

void generateNewReading() {
    // Simulate new reading: 150-220°F (tire temps)
    currentTemp = random(1500, 2200) / 10.0;

    // Add emissivity effect
    currentTemp *= emissivity;

    // Update session max/min
    if (currentTemp > sessionMax) {
        sessionMax = currentTemp;
    }
    if (currentTemp < sessionMin) {
        sessionMin = currentTemp;
    }

    // Ambient temp drifts slowly
    ambientTemp += (random(-10, 10) / 100.0);
    ambientTemp = constrain(ambientTemp, 65.0, 85.0);

    // Drain battery
    if (random(0, 100) < 2) {
        battery = max(0, battery - 1);
    }
}

void sendTemperature() {
    if (!deviceConnected) return;

    // Convert to Celsius if needed
    float displayTemp = currentTemp;
    float displayAmb = ambientTemp;
    float displayMax = sessionMax;
    float displayMin = sessionMin;

    if (unit == 'C') {
        displayTemp = (currentTemp - 32.0) * 5.0 / 9.0;
        displayAmb = (ambientTemp - 32.0) * 5.0 / 9.0;
        displayMax = (sessionMax - 32.0) * 5.0 / 9.0;
        displayMin = (sessionMin - 32.0) * 5.0 / 9.0;
    }

    // JSON format - EXACT match to spec
    StaticJsonDocument<256> doc;
    doc["temp"] = displayTemp;
    doc["amb"] = displayAmb;
    doc["max"] = displayMax;
    doc["min"] = displayMin;
    doc["bat"] = battery;
    doc["mode"] = mode;

    String tempJson;
    serializeJson(doc, tempJson);
    pTempChar->setValue(tempJson.c_str());
    pTempChar->notify();

    Serial.printf("Temp: %.1f%c | Amb: %.1f%c | Max: %.1f%c | Min: %.1f%c | Bat: %d%% | Mode: %s\n",
                  displayTemp, unit, displayAmb, unit, displayMax, unit, displayMin, unit,
                  battery, mode == 0 ? "SPOT" : "CONTINUOUS");
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toUpperCase();

        if (input == "READ" || input == "R") {
            Serial.println("New reading triggered");
            generateNewReading();
            sendTemperature();
        }
        else if (input == "RESET") {
            sessionMax = currentTemp;
            sessionMin = currentTemp;
            Serial.println("Session max/min RESET");
        }
        else if (input.startsWith("EMIT ")) {
            float newEmit = input.substring(5).toFloat();
            if (newEmit >= 0.1 && newEmit <= 1.0) {
                emissivity = newEmit;
                Serial.printf("Emissivity: %.2f\n", emissivity);
            }
        }
        else if (input == "F") {
            unit = 'F';
            Serial.println("Unit: Fahrenheit");
        }
        else if (input == "C") {
            unit = 'C';
            Serial.println("Unit: Celsius");
        }
        else if (input == "SPOT") {
            mode = 0;
            Serial.println("Mode: SPOT");
        }
        else if (input == "CONT") {
            mode = 1;
            Serial.println("Mode: CONTINUOUS");
        }
        else if (input == "STATUS") {
            Serial.printf("Current: %.1f°%c\n", currentTemp, unit);
            Serial.printf("Ambient: %.1f°%c\n", ambientTemp, unit);
            Serial.printf("Max: %.1f°%c\n", sessionMax, unit);
            Serial.printf("Min: %.1f°%c\n", sessionMin, unit);
            Serial.printf("Emissivity: %.2f\n", emissivity);
            Serial.printf("Battery: %d%%\n", battery);
            Serial.printf("Mode: %s\n", mode == 0 ? "SPOT" : "CONTINUOUS");
            Serial.printf("Unit: %c\n", unit);
            Serial.printf("Connected: %s\n", deviceConnected ? "YES" : "NO");
        }
        else {
            Serial.println("Commands: READ/R, RESET, EMIT <val>, F, C, SPOT, CONT, STATUS");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== Tire Temperature Gun BLE Mock Firmware ===");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    setupBLE();

    Serial.println("\nSerial Commands:");
    Serial.println("  READ/R - Take new reading");
    Serial.println("  RESET - Reset session max/min");
    Serial.println("  EMIT <val> - Set emissivity (0.1-1.0)");
    Serial.println("  F/C - Set unit");
    Serial.println("  SPOT/CONT - Set mode");
    Serial.println("  STATUS - Show current state");
    Serial.println("\nButton: Press BOOT for new reading");
}

void loop() {
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;

    // Button handling for new reading
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH &&
        (millis() - lastButtonPress > 500)) {
        lastButtonPress = millis();
        Serial.println("BUTTON: New reading");
        generateNewReading();
        sendTemperature();
    }
    lastButtonState = buttonState;

    // Send updates every 2 seconds (or continuous mode)
    unsigned long interval = (mode == 1) ? 1000 : 2000;
    if (millis() - lastUpdate >= interval) {
        lastUpdate = millis();

        if (mode == 1) {
            // Continuous mode: generate new reading each time
            generateNewReading();
        }

        sendTemperature();
    }

    handleSerial();
    delay(10);
}

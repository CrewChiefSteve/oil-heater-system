/**
 * Tire Temperature Probe BLE Mock Firmware
 *
 * Simulates sequential corner workflow for tire and brake temps
 *
 * UUIDs MUST MATCH: packages/ble/src/constants/uuids.ts
 * See: SERVICE_UUIDS.TIRE_PROBE, TIRE_PROBE_CHARS
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

// BLE UUIDs - packages/ble: SERVICE_UUIDS.TIRE_PROBE, TIRE_PROBE_CHARS.*
#define SERVICE_UUID_TIRE_PROBE  "4fafc201-0004-459e-8fcc-c5c9c331914b"
#define CHAR_PROBE_TIRE          "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // TIRE_DATA - 3x Float32LE
#define CHAR_PROBE_BRAKE         "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // BRAKE_DATA - Float32LE
#define CHAR_PROBE_STATUS        "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // SYSTEM_STATUS - JSON
#define CHAR_PROBE_CONFIG        "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // DEVICE_CONFIG - JSON
#define CHAR_PROBE_CORNER        "beb5483e-36e1-4688-b7f5-ea07361b26ac"  // CORNER_READING - JSON

// Button pin for triggering corner readings
#define BUTTON_PIN 0  // BOOT button on ESP32 dev board

// Global state
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTireChar = nullptr;
NimBLECharacteristic* pBrakeChar = nullptr;
NimBLECharacteristic* pStatusChar = nullptr;
NimBLECharacteristic* pConfigChar = nullptr;
NimBLECharacteristic* pCornerChar = nullptr;

const char* CORNERS[] = {"RF", "LF", "LR", "RR"};
int currentCornerIndex = 0;
uint8_t battery = 85;
bool deviceConnected = false;
unsigned long lastUpdate = 0;

// Temperature data
struct TireTemps {
    float inside;
    float middle;
    float outside;
} tireTemps;

float brakeTemp = 0.0;

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

void setupBLE() {
    NimBLEDevice::init("TireProbe_Mock");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID_TIRE_PROBE);

    // Tire data - 3x Float32LE (12 bytes total)
    pTireChar = pService->createCharacteristic(
        CHAR_PROBE_TIRE,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Brake data - Float32LE (4 bytes)
    pBrakeChar = pService->createCharacteristic(
        CHAR_PROBE_BRAKE,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // System status - JSON
    pStatusChar = pService->createCharacteristic(
        CHAR_PROBE_STATUS,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Config - JSON
    pConfigChar = pService->createCharacteristic(
        CHAR_PROBE_CONFIG,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );

    // Corner reading - Complete corner JSON
    pCornerChar = pService->createCharacteristic(
        CHAR_PROBE_CORNER,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_TIRE_PROBE);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE started: TireProbe_Mock");
    Serial.println("Waiting for mobile app connection...");
}

void generateMockTemps() {
    // Realistic tire temps: 150-220°F (65-105°C)
    // Inside typically hottest due to camber
    tireTemps.inside = random(1800, 2100) / 10.0;   // 180-210°F
    tireTemps.middle = random(1850, 2050) / 10.0;   // 185-205°F
    tireTemps.outside = random(1750, 1950) / 10.0;  // 175-195°F

    // Brake temp: 300-600°F (150-315°C)
    brakeTemp = random(3000, 6000) / 10.0;

    // Add slight variance for realism
    float variance = random(-30, 30) / 10.0;
    tireTemps.inside += variance;
    tireTemps.middle += variance;
    tireTemps.outside += variance;
}

void sendCornerReading() {
    if (!deviceConnected) return;

    const char* corner = CORNERS[currentCornerIndex];
    generateMockTemps();

    // Send tire temps as 3x Float32LE binary
    float tireData[3] = {tireTemps.inside, tireTemps.middle, tireTemps.outside};
    pTireChar->setValue((uint8_t*)tireData, 12);
    pTireChar->notify();

    // Send brake temp as Float32LE binary
    pBrakeChar->setValue((uint8_t*)&brakeTemp, 4);
    pBrakeChar->notify();

    // Send complete corner reading as JSON
    StaticJsonDocument<256> cornerDoc;
    cornerDoc["corner"] = corner;
    cornerDoc["tireInside"] = tireTemps.inside;
    cornerDoc["tireMiddle"] = tireTemps.middle;
    cornerDoc["tireOutside"] = tireTemps.outside;
    cornerDoc["brakeTemp"] = brakeTemp;

    String cornerJson;
    serializeJson(cornerDoc, cornerJson);
    pCornerChar->setValue(cornerJson.c_str());
    pCornerChar->notify();

    Serial.printf("\n[%s] Tire: I=%.1f°F M=%.1f°F O=%.1f°F | Brake: %.1f°F\n",
                  corner, tireTemps.inside, tireTemps.middle, tireTemps.outside, brakeTemp);

    // Auto-advance to next corner after 3 seconds
    currentCornerIndex = (currentCornerIndex + 1) % 4;

    if (currentCornerIndex == 0) {
        Serial.println("\n=== Session complete! Starting over at RF ===\n");
    } else {
        Serial.printf("Next corner in 3 seconds: %s\n", CORNERS[currentCornerIndex]);
    }

    // Drain battery slightly
    if (random(0, 100) < 10) {
        battery = max(0, battery - 1);
    }
}

void sendStatus() {
    if (!deviceConnected) return;

    StaticJsonDocument<128> statusDoc;
    statusDoc["battery"] = battery;
    statusDoc["isCharging"] = false;
    statusDoc["firmware"] = "1.0.0";

    String statusJson;
    serializeJson(statusDoc, statusJson);
    pStatusChar->setValue(statusJson.c_str());
    pStatusChar->notify();
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toUpperCase();

        if (input == "READ" || input == "R") {
            Serial.println("Triggered corner reading");
            sendCornerReading();
            sendStatus();
        }
        else if (input == "RESET") {
            currentCornerIndex = 0;
            Serial.println("Reset to RF corner");
        }
        else if (input == "STATUS") {
            Serial.printf("Current corner: %s\n", CORNERS[currentCornerIndex]);
            Serial.printf("Battery: %d%%\n", battery);
            Serial.printf("Connected: %s\n", deviceConnected ? "YES" : "NO");
        }
        else {
            Serial.println("Commands: READ/R, RESET, STATUS");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== Tire Temperature Probe BLE Mock Firmware ===");
    Serial.println("Sequential corner workflow: RF -> LF -> LR -> RR");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    setupBLE();

    Serial.println("\nSerial Commands:");
    Serial.println("  READ/R - Take corner reading and advance");
    Serial.println("  RESET - Reset to RF corner");
    Serial.println("  STATUS - Show current state");
    Serial.println("\nButton: Press BOOT to trigger corner reading");
    Serial.printf("\nReady for corner: %s\n", CORNERS[currentCornerIndex]);
}

void loop() {
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;
    static unsigned long autoAdvanceTime = 0;

    // Button handling for corner readings
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH &&
        (millis() - lastButtonPress > 500)) {
        lastButtonPress = millis();
        Serial.println("\nBUTTON: Corner reading triggered");
        sendCornerReading();
        sendStatus();
        autoAdvanceTime = millis();
    }
    lastButtonState = buttonState;

    // Send status update every 5 seconds
    if (millis() - lastUpdate >= 5000) {
        lastUpdate = millis();
        sendStatus();
    }

    handleSerial();
    delay(10);
}

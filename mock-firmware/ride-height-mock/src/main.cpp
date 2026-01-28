/**
 * Ride Height BLE Mock Firmware
 *
 * Simulates dual ultrasonic sensor ride height measurement system
 *
 * UUIDs MUST MATCH: packages/ble/src/constants/uuids.ts
 * See: SERVICE_UUIDS.RIDE_HEIGHT, RIDE_HEIGHT_CHARS
 */

#include <Arduino.h>
#include <NimBLEDevice.h>

// BLE UUIDs - packages/ble: SERVICE_UUIDS.RIDE_HEIGHT, RIDE_HEIGHT_CHARS.*
#define SERVICE_UUID_RIDE_HEIGHT "4fafc201-0003-459e-8fcc-c5c9c331914b"
#define CHAR_HEIGHT_DATA         "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // HEIGHT - CSV string
#define CHAR_HEIGHT_CMD          "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // COMMAND - Write

// Button pin for triggering readings
#define BUTTON_PIN 0  // BOOT button on ESP32 dev board

// Global state
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pDataChar = nullptr;
NimBLECharacteristic* pCmdChar = nullptr;

float sensor1 = 123.4;  // mm
float sensor2 = 125.1;  // mm
float batteryVoltage = 3.85;  // V
bool continuousMode = false;
bool deviceConnected = false;
unsigned long lastUpdate = 0;

class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        continuousMode = false;
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
            char cmd = value[0];
            Serial.printf("Command received: '%c'\n", cmd);

            switch (cmd) {
                case 'R':
                case 'r':
                    Serial.println("Single reading requested");
                    // Will send one reading on next update
                    break;

                case 'C':
                case 'c':
                    continuousMode = true;
                    Serial.println("Continuous mode STARTED");
                    break;

                case 'S':
                case 's':
                    continuousMode = false;
                    Serial.println("Continuous mode STOPPED");
                    break;

                case 'Z':
                case 'z':
                    sensor1 = 0.0;
                    sensor2 = 0.0;
                    Serial.println("Sensors ZEROED");
                    break;

                default:
                    Serial.printf("Unknown command: '%c'\n", cmd);
                    break;
            }
        }
    }
};

void setupBLE() {
    NimBLEDevice::init("RH-Sensor");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID_RIDE_HEIGHT);

    // Data characteristic - CSV string, notify
    pDataChar = pService->createCharacteristic(
        CHAR_HEIGHT_DATA,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Command characteristic - Write only
    pCmdChar = pService->createCharacteristic(
        CHAR_HEIGHT_CMD,
        NIMBLE_PROPERTY::WRITE
    );
    pCmdChar->setCallbacks(new CommandCallbacks());

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_RIDE_HEIGHT);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE started: RH-Sensor");
    Serial.println("Waiting for mobile app connection...");
}

void updateSensors() {
    // Add realistic variance
    float noise1 = (random(-50, 50) / 100.0);  // ±0.5mm
    float noise2 = (random(-50, 50) / 100.0);

    sensor1 += noise1;
    sensor2 += noise2;

    // Clamp to realistic range
    sensor1 = constrain(sensor1, 100.0, 150.0);
    sensor2 = constrain(sensor2, 100.0, 150.0);

    // Ensure sensors stay within 5mm of each other (realistic)
    if (abs(sensor1 - sensor2) > 5.0) {
        float avg = (sensor1 + sensor2) / 2.0;
        sensor1 = avg + random(-25, 25) / 10.0;
        sensor2 = avg + random(-25, 25) / 10.0;
    }

    // Slowly drain battery
    if (random(0, 100) < 1) {
        batteryVoltage -= 0.01;
        batteryVoltage = constrain(batteryVoltage, 3.0, 4.2);
    }
}

void sendReading() {
    if (!deviceConnected) return;

    float avgMM = (sensor1 + sensor2) / 2.0;
    float avgInches = avgMM / 25.4;

    // CSV format: S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85
    char dataStr[128];
    snprintf(dataStr, sizeof(dataStr),
             "S1:%.1f,S2:%.1f,AVG:%.1f,IN:%.2f,BAT:%.2f",
             sensor1, sensor2, avgMM, avgInches, batteryVoltage);

    pDataChar->setValue(dataStr);
    pDataChar->notify();

    Serial.printf("Sent: %s\n", dataStr);

    // Check for sensor delta warning
    float delta = abs(sensor1 - sensor2);
    if (delta > 5.0) {
        Serial.printf("WARNING: Sensor delta %.1fmm exceeds 5mm threshold!\n", delta);
    }
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        input.toUpperCase();

        if (input == "R") {
            Serial.println("Single reading");
            updateSensors();
            sendReading();
        }
        else if (input == "C") {
            continuousMode = true;
            Serial.println("Continuous mode STARTED");
        }
        else if (input == "S") {
            continuousMode = false;
            Serial.println("Continuous mode STOPPED");
        }
        else if (input == "Z") {
            sensor1 = 0.0;
            sensor2 = 0.0;
            Serial.println("Sensors ZEROED");
        }
        else if (input == "STATUS") {
            Serial.printf("Sensor 1: %.1fmm\n", sensor1);
            Serial.printf("Sensor 2: %.1fmm\n", sensor2);
            Serial.printf("Average: %.1fmm (%.2f in)\n", (sensor1+sensor2)/2.0, (sensor1+sensor2)/2.0/25.4);
            Serial.printf("Delta: %.1fmm\n", abs(sensor1-sensor2));
            Serial.printf("Battery: %.2fV\n", batteryVoltage);
            Serial.printf("Mode: %s\n", continuousMode ? "CONTINUOUS" : "MANUAL");
            Serial.printf("Connected: %s\n", deviceConnected ? "YES" : "NO");
        }
        else {
            Serial.println("Commands: R (read), C (continuous), S (stop), Z (zero), STATUS");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== Ride Height BLE Mock Firmware ===");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    setupBLE();

    Serial.println("\nSerial Commands:");
    Serial.println("  R - Single reading");
    Serial.println("  C - Start continuous mode (500ms)");
    Serial.println("  S - Stop continuous mode");
    Serial.println("  Z - Zero sensors");
    Serial.println("  STATUS - Show current state");
    Serial.println("\nButton: Press BOOT for single reading");
}

void loop() {
    static unsigned long lastButtonPress = 0;
    static bool lastButtonState = HIGH;

    // Button handling for single reading
    bool buttonState = digitalRead(BUTTON_PIN);
    if (buttonState == LOW && lastButtonState == HIGH &&
        (millis() - lastButtonPress > 500)) {
        lastButtonPress = millis();
        Serial.println("BUTTON: Single reading");
        updateSensors();
        sendReading();
    }
    lastButtonState = buttonState;

    // Continuous mode updates every 500ms
    if (continuousMode && millis() - lastUpdate >= 500) {
        lastUpdate = millis();
        updateSensors();
        sendReading();
    }

    handleSerial();
    delay(10);
}

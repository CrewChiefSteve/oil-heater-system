#include "ble_service.h"
#include "ble_protocol.h"
#include "config.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <Preferences.h>

static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* cornerReadingChar = nullptr;
static NimBLECharacteristic* statusChar = nullptr;
static NimBLECharacteristic* cornerIDChar = nullptr;

static bool deviceConnected = false;
static uint8_t currentCornerID = DEFAULT_CORNER_ID;  // v2: 0=LF, 1=RF, 2=LR, 3=RR
static Preferences preferences;

// v2: Helper function to get corner string from UInt8
const char* getCornerString(uint8_t cornerID) {
    switch (cornerID) {
        case 0: return "LF";
        case 1: return "RF";
        case 2: return "LR";
        case 3: return "RR";
        default: return "LF";
    }
}

// v2: CORNER_ID characteristic callbacks
class CornerIDCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        uint8_t* data = pCharacteristic->getData();
        uint8_t newCornerID = data[0];

        // Validate corner ID (0-3)
        if (newCornerID <= 3) {
            currentCornerID = newCornerID;

            // Save to NVS
            preferences.begin(NVS_NAMESPACE, false);
            preferences.putUChar(NVS_CORNER_KEY, currentCornerID);
            preferences.end();

            Serial.printf("[BLE] Corner ID updated: %d (%s)\n", currentCornerID, getCornerString(currentCornerID));
            Serial.println("      Restart required to update device name");

            // Notify updated value
            pCharacteristic->notify();
        } else {
            Serial.printf("[BLE] Invalid corner ID: %d (must be 0-3)\n", newCornerID);
        }
    }
};

// Server callbacks for connection events
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLE] Client disconnected");
        bleStartAdvertising();  // Resume advertising
    }
};

void bleInit(const char* deviceName, uint8_t cornerID) {
    Serial.println("[BLE] Initializing v2 protocol...");

    // Store corner ID
    currentCornerID = cornerID;

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(BLE_TX_POWER);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pService = pServer->createService(SERVICE_UUID);

    // v2: CORNER_READING characteristic (26a8) - READ + NOTIFY, JSON format
    cornerReadingChar = pService->createCharacteristic(
        CORNER_READING_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    cornerReadingChar->addDescriptor(new NimBLE2902());  // v2: iOS compatibility

    // v2: STATUS characteristic (26aa) - READ + NOTIFY, JSON format
    statusChar = pService->createCharacteristic(
        STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    statusChar->addDescriptor(new NimBLE2902());  // v2: iOS compatibility

    // v2: CORNER_ID characteristic (26af) - READ + WRITE + NOTIFY, UInt8
    cornerIDChar = pService->createCharacteristic(
        CORNER_ID_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    cornerIDChar->setCallbacks(new CornerIDCallbacks());
    cornerIDChar->addDescriptor(new NimBLE2902());  // v2: iOS compatibility
    cornerIDChar->setValue(&currentCornerID, 1);  // Set initial UInt8 value

    pService->start();

    Serial.printf("[BLE] Service initialized (v2 protocol)\n");
    Serial.printf("      Device: %s\n", deviceName);
    Serial.printf("      Corner ID: %d (%s)\n", currentCornerID, getCornerString(currentCornerID));
}

void bleStartAdvertising() {
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Functions that help with iPhone connections issue
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();
    Serial.println("[BLE] Advertising started");
}

bool bleIsConnected() {
    return deviceConnected;
}

void bleTransmitCornerReading(const CornerReading& reading) {
    if (!deviceConnected || cornerReadingChar == nullptr) {
        return;
    }

    // v2: Build JSON document with temp1/temp2/temp3 format
    StaticJsonDocument<200> doc;

    // v2: Corner name from Corner enum (LF=0, RF=1, LR=2, RR=3)
    doc["corner"] = getCornerString(reading.corner);

    // v2: temp1 = outer, temp2 = middle, temp3 = inner
    // Note: tireOutside maps to temp1, tireMiddle to temp2, tireInside to temp3
    float temp1, temp2, temp3;
    const char* unit;

    if (USE_FAHRENHEIT) {
        temp1 = (reading.tireOutside * 9.0f / 5.0f) + 32.0f;  // Outer
        temp2 = (reading.tireMiddle * 9.0f / 5.0f) + 32.0f;   // Middle
        temp3 = (reading.tireInside * 9.0f / 5.0f) + 32.0f;   // Inner
        unit = "F";
    } else {
        temp1 = reading.tireOutside;
        temp2 = reading.tireMiddle;
        temp3 = reading.tireInside;
        unit = "C";
    }

    doc["temp1"] = temp1;
    doc["temp2"] = temp2;
    doc["temp3"] = temp3;
    doc["unit"] = unit;

    // Serialize to string
    char jsonBuffer[200];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    cornerReadingChar->setValue((uint8_t*)jsonBuffer, len);
    cornerReadingChar->notify();

    Serial.printf("[BLE] TX Corner: %s | temp1:%.1f temp2:%.1f temp3:%.1f %s\n",
                  getCornerString(reading.corner),
                  temp1, temp2, temp3, unit);
}

void bleTransmitSystemStatus(const SystemStatus& status) {
    if (!deviceConnected || statusChar == nullptr) {
        return;
    }

    // v2: Build JSON document per v2 spec: {"batteryLow":bool,"sensorError":bool,"probeConnected":bool}
    StaticJsonDocument<128> doc;

    // v2: batteryLow - check if battery is below threshold
    doc["batteryLow"] = (status.batteryPercent < BATTERY_LOW_THRESHOLD);

    // v2: sensorError - check for any sensor faults (placeholder - implement based on probe errors)
    doc["sensorError"] = false;  // TODO: Implement actual sensor error detection

    // v2: probeConnected - check if probes are responding (placeholder)
    doc["probeConnected"] = true;  // TODO: Implement actual probe connection check

    // Serialize to string
    char jsonBuffer[128];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    statusChar->setValue((uint8_t*)jsonBuffer, len);
    statusChar->notify();

    Serial.printf("[BLE] TX Status: BatteryLow:%s SensorError:%s ProbeConnected:%s\n",
                  doc["batteryLow"].as<bool>() ? "true" : "false",
                  doc["sensorError"].as<bool>() ? "true" : "false",
                  doc["probeConnected"].as<bool>() ? "true" : "false");
}

void bleUpdate() {
    // Placeholder for future BLE tasks
}

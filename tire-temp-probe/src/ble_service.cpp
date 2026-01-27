#include "ble_service.h"
#include "ble_protocol.h"
#include "config.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* cornerReadingChar = nullptr;
static NimBLECharacteristic* systemStatusChar = nullptr;

static bool deviceConnected = false;

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

void bleInit(const char* deviceName) {
    Serial.println("[BLE] Initializing...");

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(BLE_TX_POWER);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pService = pServer->createService(SERVICE_UUID);

    // Corner reading characteristic (notify only - JSON)
    cornerReadingChar = pService->createCharacteristic(
        CORNER_READING_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // System status characteristic (notify only - binary)
    systemStatusChar = pService->createCharacteristic(
        SYSTEM_STATUS_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    pService->start();

    Serial.println("[BLE] Service initialized");
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

    // Build JSON document
    StaticJsonDocument<200> doc;

    // Corner name mapping
    const char* cornerNames[] = {"RF", "LF", "LR", "RR"};
    doc["corner"] = cornerNames[reading.corner];

    // Convert Celsius to Fahrenheit (mobile app expects Fahrenheit)
    float tireInsideF = (reading.tireInside * 9.0f / 5.0f) + 32.0f;
    float tireMiddleF = (reading.tireMiddle * 9.0f / 5.0f) + 32.0f;
    float tireOutsideF = (reading.tireOutside * 9.0f / 5.0f) + 32.0f;
    float brakeTempF = (reading.brakeTemp * 9.0f / 5.0f) + 32.0f;

    doc["tireInside"] = tireInsideF;
    doc["tireMiddle"] = tireMiddleF;
    doc["tireOutside"] = tireOutsideF;
    doc["brakeTemp"] = brakeTempF;

    // Serialize to string
    char jsonBuffer[200];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    cornerReadingChar->setValue((uint8_t*)jsonBuffer, len);
    cornerReadingChar->notify();

    Serial.printf("[BLE] TX Corner: %s | In:%.1fF Mid:%.1fF Out:%.1fF Brake:%.1fF\n",
                  cornerNames[reading.corner],
                  tireInsideF, tireMiddleF,
                  tireOutsideF, brakeTempF);
}

void bleTransmitSystemStatus(const SystemStatus& status, uint8_t capturedCount) {
    if (!deviceConnected || systemStatusChar == nullptr) {
        return;
    }

    // Build JSON document
    // Mobile app expects: { battery: number, isCharging: boolean, firmware: string }
    StaticJsonDocument<128> doc;
    doc["battery"] = status.batteryPercent;
    doc["isCharging"] = status.charging;
    doc["firmware"] = DEVICE_MODEL;  // "TTP-4CH-v1"

    // Serialize to string
    char jsonBuffer[128];
    size_t len = serializeJson(doc, jsonBuffer);

    // Transmit via BLE
    systemStatusChar->setValue((uint8_t*)jsonBuffer, len);
    systemStatusChar->notify();

    Serial.printf("[BLE] TX Status: Bat:%d%% Charging:%s FW:%s\n",
                  status.batteryPercent,
                  status.charging ? "Yes" : "No",
                  DEVICE_MODEL);
}

void bleUpdate() {
    // Placeholder for future BLE tasks
}

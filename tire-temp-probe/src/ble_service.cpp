#include "ble_service.h"
#include "ble_protocol.h"
#include "config.h"
#include <NimBLEDevice.h>

static NimBLEServer* pServer = nullptr;
static NimBLEService* pService = nullptr;
static NimBLECharacteristic* pCharTireData = nullptr;
static NimBLECharacteristic* pCharBrakeData = nullptr;
static NimBLECharacteristic* pCharSystemStatus = nullptr;
static NimBLECharacteristic* pCharDeviceConfig = nullptr;

static bool deviceConnected = false;
static Corner currentCorner = DEFAULT_CORNER;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE client disconnected");
        bleStartAdvertising();
    }
};

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() >= 2) {
            uint8_t cornerValue = (uint8_t)value[1];
            if (cornerValue <= CORNER_RR) {
                currentCorner = (Corner)cornerValue;
                Serial.print("Corner assignment changed to: ");
                Serial.println(cornerValue);
            }
        }
    }
};

void bleInit(const char* deviceName) {
    Serial.println("Initializing BLE...");

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(BLE_TX_POWER);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    pService = pServer->createService(SERVICE_UUID_TIRE_TEMP);

    // Tire data characteristic (notify)
    pCharTireData = pService->createCharacteristic(
        CHAR_UUID_TIRE_DATA,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Brake data characteristic (notify)
    pCharBrakeData = pService->createCharacteristic(
        CHAR_UUID_BRAKE_DATA,
        NIMBLE_PROPERTY::NOTIFY
    );

    // System status characteristic (notify)
    pCharSystemStatus = pService->createCharacteristic(
        CHAR_UUID_SYSTEM_STATUS,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Device config characteristic (read/write)
    pCharDeviceConfig = pService->createCharacteristic(
        CHAR_UUID_DEVICE_CONFIG,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pCharDeviceConfig->setCallbacks(new ConfigCallbacks());

    // Set initial config value
    uint8_t configData[2] = {PKT_TYPE_DEVICE_CONFIG, (uint8_t)currentCorner};
    pCharDeviceConfig->setValue(configData, 2);

    pService->start();

    Serial.println("BLE service started");
}

void bleStartAdvertising() {
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID_TIRE_TEMP);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
    Serial.println("BLE advertising started");
}

void bleStopAdvertising() {
    NimBLEDevice::getAdvertising()->stop();
    Serial.println("BLE advertising stopped");
}

bool bleIsConnected() {
    return deviceConnected;
}

void bleTransmitTireData(const TireChannel &tire, Corner corner, uint32_t timestamp) {
    if (!deviceConnected || !pCharTireData) return;

    uint8_t packet[PKT_SIZE_TIRE_DATA];
    uint8_t idx = 0;

    packet[idx++] = PKT_TYPE_TIRE_TEMPS;

    memcpy(&packet[idx], &tire.inside.temperature, sizeof(float));
    idx += sizeof(float);

    memcpy(&packet[idx], &tire.middle.temperature, sizeof(float));
    idx += sizeof(float);

    memcpy(&packet[idx], &tire.outside.temperature, sizeof(float));
    idx += sizeof(float);

    memcpy(&packet[idx], &timestamp, sizeof(uint32_t));
    idx += sizeof(uint32_t);

    packet[idx++] = (uint8_t)corner;

    pCharTireData->setValue(packet, PKT_SIZE_TIRE_DATA);
    pCharTireData->notify();
}

void bleTransmitBrakeData(const BrakeChannel &brake, Corner corner, uint32_t timestamp) {
    if (!deviceConnected || !pCharBrakeData) return;

    uint8_t packet[PKT_SIZE_BRAKE_DATA];
    uint8_t idx = 0;

    packet[idx++] = PKT_TYPE_BRAKE_TEMP;

    memcpy(&packet[idx], &brake.rotor.temperature, sizeof(float));
    idx += sizeof(float);

    memcpy(&packet[idx], &timestamp, sizeof(uint32_t));
    idx += sizeof(uint32_t);

    packet[idx++] = (uint8_t)corner;

    pCharBrakeData->setValue(packet, PKT_SIZE_BRAKE_DATA);
    pCharBrakeData->notify();
}

void bleTransmitSystemStatus(const SystemStatus &status) {
    if (!deviceConnected || !pCharSystemStatus) return;

    uint8_t packet[PKT_SIZE_SYSTEM_STATUS];
    uint8_t idx = 0;

    packet[idx++] = PKT_TYPE_SYSTEM_STATUS;
    packet[idx++] = (uint8_t)status.state;
    packet[idx++] = status.batteryPercent;

    memcpy(&packet[idx], &status.batteryVoltage, sizeof(float));
    idx += sizeof(float);

    packet[idx++] = status.charging ? 1 : 0;

    memcpy(&packet[idx], &status.uptimeMs, sizeof(uint32_t));
    idx += sizeof(uint32_t);

    pCharSystemStatus->setValue(packet, PKT_SIZE_SYSTEM_STATUS);
    pCharSystemStatus->notify();
}

Corner bleGetCorner() {
    return currentCorner;
}

void bleUpdate() {
    // Placeholder for future BLE tasks
}

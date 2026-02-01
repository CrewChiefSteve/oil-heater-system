// =================================================================
// CrewChiefSteve Mock Firmware — BLE Protocol v2
//
// Single ESP32-C3 that simulates any of the 5 BLE devices for
// mobile app development and testing without real hardware.
//
// Serial commands:
//   device 0–4  Switch active device (tears down BLE, rebuilds)
//   corner 0–3  Set corner ID (LF/RF/LR/RR) for multi-corner devices
//   status      Print current simulation state
//   help        Show available commands
//   reset       Restart current device simulation
//
// Target: ESP32-C3 (single-core RISC-V) with NimBLE
// =================================================================

#include <NimBLEDevice.h>
#include "config.h"
#include "simulator.h"
#include "mock_oil_heater.h"
#include "mock_racescale.h"
#include "mock_ride_height.h"
#include "mock_tire_probe.h"
#include "mock_tire_temp_gun.h"

// ═══════════════════════════════════════════════════════════════
// Global State
// ═══════════════════════════════════════════════════════════════

NimBLEServer* pServer = nullptr;
volatile bool deviceConnected = false;

// Active device type and corner
DeviceType activeDevice = DEV_OIL_HEATER;
uint8_t activeCorner = CORNER_LF;

// Device instances (only one active at a time)
MockOilHeater*    heater   = nullptr;
MockRaceScale*    scale    = nullptr;
MockRideHeight*   rh       = nullptr;
MockTireProbe*    probe    = nullptr;
MockTireTempGun*  gun      = nullptr;

// Compile-time override (set via build_flags)
#ifndef MOCK_DEFAULT_DEVICE
#define MOCK_DEFAULT_DEVICE 0
#endif
#ifndef MOCK_DEFAULT_CORNER
#define MOCK_DEFAULT_CORNER 0
#endif

// ═══════════════════════════════════════════════════════════════
// BLE Server Callbacks
// ═══════════════════════════════════════════════════════════════

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        deviceConnected = true;
        Serial.println("\n✓ Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) override {
        deviceConnected = false;
        Serial.println("\n✗ Client disconnected — restarting advertising");
        NimBLEDevice::startAdvertising();
    }
};

// ═══════════════════════════════════════════════════════════════
// Device Lifecycle
// ═══════════════════════════════════════════════════════════════

String getDeviceBLEName(DeviceType dev, uint8_t corner) {
    switch (dev) {
        case DEV_OIL_HEATER:    return "Heater_MOCK";
        case DEV_RACESCALE:     return String("RaceScale_") + CORNER_NAMES[corner];
        case DEV_RIDE_HEIGHT:   return String("RH-Sensor_") + CORNER_NAMES[corner];
        case DEV_TIRE_PROBE:    return String("TireProbe_") + CORNER_NAMES[corner];
        case DEV_TIRE_TEMP_GUN: return "TireTempGun";
        default:                return "CCS_Mock";
    }
}

const char* getServiceUUID(DeviceType dev) {
    switch (dev) {
        case DEV_OIL_HEATER:    return SVC_OIL_HEATER;
        case DEV_RACESCALE:     return SVC_RACESCALE;
        case DEV_RIDE_HEIGHT:   return SVC_RIDE_HEIGHT;
        case DEV_TIRE_PROBE:    return SVC_TIRE_PROBE;
        case DEV_TIRE_TEMP_GUN: return SVC_TIRE_TEMP_GUN;
        default:                return SVC_OIL_HEATER;
    }
}

void destroyActiveDevice() {
    // Free device instance
    if (heater)  { delete heater;  heater = nullptr; }
    if (scale)   { delete scale;   scale  = nullptr; }
    if (rh)      { delete rh;      rh     = nullptr; }
    if (probe)   { delete probe;   probe  = nullptr; }
    if (gun)     { delete gun;     gun    = nullptr; }
}

void teardownBLE() {
    Serial.println("Tearing down BLE...");
    destroyActiveDevice();
    NimBLEDevice::deinit(true);  // true = release all resources
    pServer = nullptr;
    deviceConnected = false;
    delay(500);  // Let BLE stack settle
}

void initDevice(DeviceType dev, uint8_t corner) {
    activeDevice = dev;
    activeCorner = corner;

    String bleName = getDeviceBLEName(dev, corner);

    Serial.println("\n════════════════════════════════════════");
    Serial.printf("Starting: %s\n", DEVICE_TYPE_NAMES[dev]);
    Serial.printf("BLE Name: %s\n", bleName.c_str());
    Serial.printf("Service:  %s\n", getServiceUUID(dev));
    if (dev != DEV_OIL_HEATER && dev != DEV_TIRE_TEMP_GUN) {
        Serial.printf("Corner:   %s (%d)\n", CORNER_NAMES[corner], corner);
    }
    Serial.println("════════════════════════════════════════\n");

    // Initialize NimBLE
    NimBLEDevice::init(bleName.c_str());
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max power for testing
    NimBLEDevice::setMTU(BLE_MTU_SIZE);

    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create the selected device's service
    switch (dev) {
        case DEV_OIL_HEATER:
            heater = new MockOilHeater();
            heater->createService(pServer);
            break;

        case DEV_RACESCALE:
            scale = new MockRaceScale(corner);
            scale->createService(pServer);
            break;

        case DEV_RIDE_HEIGHT:
            rh = new MockRideHeight(corner);
            rh->createService(pServer);
            break;

        case DEV_TIRE_PROBE:
            probe = new MockTireProbe(corner);
            probe->createService(pServer);
            break;

        case DEV_TIRE_TEMP_GUN:
            gun = new MockTireTempGun();
            gun->createService(pServer);
            break;

        default:
            Serial.println("ERROR: Unknown device type!");
            return;
    }

    // Configure advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(getServiceUUID(dev));
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(ADV_MIN_INTERVAL);  // iOS workaround
    pAdvertising->setMaxPreferred(ADV_MAX_INTERVAL);
    pAdvertising->start();

    Serial.println("Advertising started. Waiting for connection...\n");
}

void switchDevice(DeviceType dev, uint8_t corner) {
    teardownBLE();
    initDevice(dev, corner);
}

// ═══════════════════════════════════════════════════════════════
// Serial Command Handler
// ═══════════════════════════════════════════════════════════════

void printHelp() {
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║  CrewChiefSteve Mock Firmware v2.0    ║");
    Serial.println("╠═══════════════════════════════════════╣");
    Serial.println("║  device 0  Oil Heater (Heater_MOCK)  ║");
    Serial.println("║  device 1  RaceScale (RaceScale_XX)  ║");
    Serial.println("║  device 2  Ride Height (RH-Sensor_XX)║");
    Serial.println("║  device 3  Tire Probe (TireProbe_XX) ║");
    Serial.println("║  device 4  Temp Gun (TireTempGun)    ║");
    Serial.println("║                                       ║");
    Serial.println("║  corner 0  LF (Left Front)            ║");
    Serial.println("║  corner 1  RF (Right Front)           ║");
    Serial.println("║  corner 2  LR (Left Rear)             ║");
    Serial.println("║  corner 3  RR (Right Rear)            ║");
    Serial.println("║                                       ║");
    Serial.println("║  status    Print current state         ║");
    Serial.println("║  reset     Restart current device      ║");
    Serial.println("║  help      Show this menu              ║");
    Serial.println("║  heap      Show free heap memory       ║");
    Serial.println("╚═══════════════════════════════════════╝\n");
}

void printCurrentStatus() {
    Serial.printf("\nActive: %s  Connected: %s\n",
        DEVICE_TYPE_NAMES[activeDevice],
        deviceConnected ? "YES" : "NO");

    if (heater)  heater->printStatus();
    if (scale)   scale->printStatus();
    if (rh)      rh->printStatus();
    if (probe)   probe->printStatus();
    if (gun)     gun->printStatus();

    Serial.printf("Free heap: %d bytes\n\n", ESP.getFreeHeap());
}

void handleSerial() {
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) return;

    if (line.startsWith("device ")) {
        int dev = line.substring(7).toInt();
        if (dev >= 0 && dev < DEV_COUNT) {
            switchDevice((DeviceType)dev, activeCorner);
        } else {
            Serial.printf("Invalid device %d (range: 0–%d)\n", dev, DEV_COUNT - 1);
        }
    }
    else if (line.startsWith("corner ")) {
        int corner = line.substring(7).toInt();
        if (corner >= 0 && corner <= CORNER_RR) {
            activeCorner = (uint8_t)corner;
            Serial.printf("Corner set to %s. Use 'reset' to restart with new corner.\n",
                CORNER_NAMES[corner]);
            // If it's a corner-aware device, restart it
            if (activeDevice != DEV_OIL_HEATER && activeDevice != DEV_TIRE_TEMP_GUN) {
                switchDevice(activeDevice, activeCorner);
            }
        } else {
            Serial.printf("Invalid corner %d (range: 0–3)\n", corner);
        }
    }
    else if (line == "status") {
        printCurrentStatus();
    }
    else if (line == "reset") {
        switchDevice(activeDevice, activeCorner);
    }
    else if (line == "help") {
        printHelp();
    }
    else if (line == "heap") {
        Serial.printf("Free heap: %d bytes (min: %d)\n",
            ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
    else {
        Serial.printf("Unknown command: '%s'. Type 'help' for options.\n", line.c_str());
    }
}

// ═══════════════════════════════════════════════════════════════
// Arduino Entry Points
// ═══════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(1000);  // Let serial settle

    randomSeed(analogRead(0) ^ micros());  // Seed RNG

    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║  CrewChiefSteve Mock Firmware v2.0    ║");
    Serial.println("║  BLE Protocol v2 — ESP32-C3 NimBLE   ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.printf("\nFree heap at boot: %d bytes\n", ESP.getFreeHeap());

    // Initialize with default device
    activeDevice = (DeviceType)MOCK_DEFAULT_DEVICE;
    activeCorner = (uint8_t)MOCK_DEFAULT_CORNER;
    initDevice(activeDevice, activeCorner);

    printHelp();
    Serial.printf("Free heap after init: %d bytes\n\n", ESP.getFreeHeap());
}

// Status print timer (every 10 seconds in serial)
unsigned long lastSerialStatus = 0;
#define SERIAL_STATUS_INTERVAL 10000

void loop() {
    unsigned long now = millis();

    // Handle serial commands
    handleSerial();

    // Update active device simulation
    if (heater)  heater->update(now);
    if (scale)   scale->update(now);
    if (rh)      rh->update(now);
    if (probe)   probe->update(now);
    if (gun)     gun->update(now);

    // Periodic status to serial (so you can see it's working)
    if (deviceConnected && (now - lastSerialStatus >= SERIAL_STATUS_INTERVAL)) {
        lastSerialStatus = now;
        printCurrentStatus();
    }

    // Main loop tick — ~100ms gives decent resolution for all update rates
    delay(100);
}

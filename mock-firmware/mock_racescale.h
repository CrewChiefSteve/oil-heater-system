#pragma once
// =================================================================
// RaceScale Mock Device (0002)
// Service UUID: 4fafc201-0002-459e-8fcc-c5c9c331914b
// BLE Name: RaceScale_XX (XX = LF/RF/LR/RR)
//
// Characteristics (per BLE_PROTOCOL_REFERENCE.md):
//   WEIGHT      (26a8) — READ, NOTIFY    — Float32LE (lbs)
//   CALIBRATION (26aa) — WRITE           — Float32LE (known weight)
//   TEMPERATURE (26ab) — READ, NOTIFY    — Float32LE (load cell °F)
//   STATUS      (26ac) — READ, NOTIFY    — JSON {zeroed, calibrated, error}
//   TARE        (26ad) — WRITE           — UInt8 (0x01 to zero)
//   BATTERY     (26ae) — READ, NOTIFY    — UInt8 (0-100%)
//   CORNER_ID   (26af) — READ, WRITE, NOTIFY — UInt8 (0-3)
// =================================================================

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"
#include "simulator.h"

class MockRaceScale {
public:
    // ── Simulation state ────────────────────────────────────────
    DampedOscillator weight;
    SimValue loadCellTemp;
    SimBattery battery;
    uint8_t cornerId;
    bool zeroed;
    bool calibrated;
    String errorStr;
    bool carOnScale;
    bool statusDirty;
    float tareOffset;

    // ── BLE handles ─────────────────────────────────────────────
    NimBLEService* pService             = nullptr;
    NimBLECharacteristic* pWeightChar   = nullptr;
    NimBLECharacteristic* pCalChar      = nullptr;
    NimBLECharacteristic* pTempChar     = nullptr;
    NimBLECharacteristic* pStatusChar   = nullptr;
    NimBLECharacteristic* pTareChar     = nullptr;
    NimBLECharacteristic* pBattChar     = nullptr;
    NimBLECharacteristic* pCornerChar   = nullptr;

    // ── Timing ──────────────────────────────────────────────────
    unsigned long lastWeightNotify  = 0;
    unsigned long lastTempNotify    = 0;
    unsigned long lastBattNotify    = 0;
    unsigned long lastStatusNotify  = 0;

    // ── Constructor ─────────────────────────────────────────────
    MockRaceScale(uint8_t corner = CORNER_LF)
        : weight(0.0f),
          loadCellTemp(SIM_SCALE_LOAD_CELL_TEMP, 0.1f, 0.2f),
          battery(3.95f, 0.00003f),
          cornerId(corner),
          zeroed(true),
          calibrated(true),
          errorStr(""),
          carOnScale(false),
          statusDirty(true),
          tareOffset(0.0f) {

        // Start with car on scale after 3 seconds (simulated in update)
        float targets[] = {SIM_SCALE_LF, SIM_SCALE_RF, SIM_SCALE_LR, SIM_SCALE_RR};
        weight.target = targets[corner % 4];
    }

    // ── BLE Setup ───────────────────────────────────────────────
    void createService(NimBLEServer* pServer) {
        pService = pServer->createService(SVC_RACESCALE);

        // WEIGHT (26a8): READ + NOTIFY — Float32LE
        pWeightChar = pService->createCharacteristic(
            CHR_26A8,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // CALIBRATION (26aa): WRITE — Float32LE
        pCalChar = pService->createCharacteristic(
            CHR_26AA,
            NIMBLE_PROPERTY::WRITE
        );
        pCalChar->setCallbacks(new CalibrationCallback(this));

        // TEMPERATURE (26ab): READ + NOTIFY — Float32LE
        pTempChar = pService->createCharacteristic(
            CHR_26AB,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // STATUS (26ac): READ + NOTIFY — JSON
        pStatusChar = pService->createCharacteristic(
            CHR_26AC,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // TARE (26ad): WRITE — UInt8
        pTareChar = pService->createCharacteristic(
            CHR_26AD,
            NIMBLE_PROPERTY::WRITE
        );
        pTareChar->setCallbacks(new TareCallback(this));

        // BATTERY (26ae): READ + NOTIFY — UInt8
        pBattChar = pService->createCharacteristic(
            CHR_26AE,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // CORNER_ID (26af): READ + WRITE + NOTIFY — UInt8
        pCornerChar = pService->createCharacteristic(
            CHR_26AF,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
        );
        pCornerChar->setCallbacks(new CornerIdCallback(this));

        // Set initial values
        setWeightValue(0.0f);
        setFloatValue(pTempChar, loadCellTemp.current);
        updateStatusValue();
        pBattChar->setValue(&battery.percent, 1);
        pCornerChar->setValue(&cornerId, 1);

        pService->start();
        Serial.printf("[Scale-%s] Service started. Target=%.1f lbs\n",
            CORNER_NAMES[cornerId], weight.target);
    }

    // ── Main update loop ────────────────────────────────────────
    void update(unsigned long now) {
        float dt = 0.1f;

        // Simulate car being placed on scale after 5 seconds
        static unsigned long bootTime = millis();
        if (!carOnScale && (now - bootTime > 5000)) {
            carOnScale = true;
            float targets[] = {SIM_SCALE_LF, SIM_SCALE_RF, SIM_SCALE_LR, SIM_SCALE_RR};
            weight.triggerSettle(targets[cornerId % 4]);
            Serial.printf("[Scale-%s] Car placed on scale — settling to %.1f lbs\n",
                CORNER_NAMES[cornerId], weight.target);
        }

        // Update simulators
        weight.update(dt, SIM_SCALE_NOISE);
        loadCellTemp.update(dt);
        battery.update(dt);

        // Notify WEIGHT at 4 Hz
        if (now - lastWeightNotify >= UPD_SCALE_WEIGHT) {
            lastWeightNotify = now;
            float w = weight.current - tareOffset;
            setWeightValue(w);
            pWeightChar->notify();
        }

        // Notify TEMPERATURE at 0.2 Hz
        if (now - lastTempNotify >= UPD_SCALE_TEMP) {
            lastTempNotify = now;
            setFloatValue(pTempChar, loadCellTemp.current);
            pTempChar->notify();
        }

        // Notify BATTERY at 0.1 Hz
        if (now - lastBattNotify >= UPD_SCALE_BATTERY) {
            lastBattNotify = now;
            pBattChar->setValue(&battery.percent, 1);
            pBattChar->notify();
        }

        // Notify STATUS periodically or on change
        if (statusDirty || (now - lastStatusNotify >= UPD_SCALE_STATUS)) {
            lastStatusNotify = now;
            statusDirty = false;
            updateStatusValue();
            pStatusChar->notify();
        }
    }

    void printStatus() {
        Serial.printf("[Scale-%s] W=%.1f lbs  Tare=%.1f  Temp=%.1f°F  Batt=%d%%  %s\n",
            CORNER_NAMES[cornerId],
            weight.current - tareOffset,
            tareOffset,
            loadCellTemp.current,
            battery.percent,
            carOnScale ? "LOADED" : "EMPTY");
    }

private:
    // ── Value setters ───────────────────────────────────────────
    void setWeightValue(float w) {
        // Float32LE: 4 bytes, little-endian (native on ESP32)
        pWeightChar->setValue((uint8_t*)&w, sizeof(float));
    }

    void setFloatValue(NimBLECharacteristic* pChar, float v) {
        pChar->setValue((uint8_t*)&v, sizeof(float));
    }

    void updateStatusValue() {
        StaticJsonDocument<128> doc;
        doc["zeroed"] = zeroed;
        doc["calibrated"] = calibrated;
        doc["error"] = errorStr;
        String json;
        serializeJson(doc, json);
        pStatusChar->setValue(json.c_str());
    }

    // ── Write callbacks ─────────────────────────────────────────
    class CalibrationCallback : public NimBLECharacteristicCallbacks {
        MockRaceScale* dev;
    public:
        CalibrationCallback(MockRaceScale* d) : dev(d) {}
        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            if (raw.size() >= 4) {
                float calWeight;
                memcpy(&calWeight, raw.data(), 4);
                dev->calibrated = true;
                dev->statusDirty = true;
                Serial.printf("[Scale-%s] Calibrated with %.1f lbs\n",
                    CORNER_NAMES[dev->cornerId], calWeight);
            }
        }
    };

    class TareCallback : public NimBLECharacteristicCallbacks {
        MockRaceScale* dev;
    public:
        TareCallback(MockRaceScale* d) : dev(d) {}
        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            if (raw.size() >= 1 && (uint8_t)raw[0] == 0x01) {
                dev->tareOffset = dev->weight.current;
                dev->zeroed = true;
                dev->statusDirty = true;
                Serial.printf("[Scale-%s] Tared at %.1f lbs\n",
                    CORNER_NAMES[dev->cornerId], dev->tareOffset);
            }
        }
    };

    class CornerIdCallback : public NimBLECharacteristicCallbacks {
        MockRaceScale* dev;
    public:
        CornerIdCallback(MockRaceScale* d) : dev(d) {}
        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            if (raw.size() >= 1) {
                uint8_t newCorner = (uint8_t)raw[0];
                if (newCorner <= CORNER_RR) {
                    dev->cornerId = newCorner;
                    // Update target weight for new corner
                    float targets[] = {SIM_SCALE_LF, SIM_SCALE_RF, SIM_SCALE_LR, SIM_SCALE_RR};
                    dev->weight.triggerSettle(targets[newCorner]);
                    dev->statusDirty = true;
                    Serial.printf("[Scale] Corner changed to %s\n", CORNER_NAMES[newCorner]);
                    pChar->setValue(&dev->cornerId, 1);
                    pChar->notify();
                }
            }
        }
    };
};

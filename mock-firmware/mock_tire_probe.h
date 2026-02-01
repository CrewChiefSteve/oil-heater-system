#pragma once
// =================================================================
// Tire Temperature Probe Mock Device (0004)
// Service UUID: 4fafc201-0004-459e-8fcc-c5c9c331914b
// BLE Name: TireProbe_XX (XX = LF/RF/LR/RR)
//
// *** v2: JSON-ONLY protocol — binary TIRE_DATA/BRAKE_DATA removed ***
//
// In v2, the mobile app uses CORNER_READING at 26a8 (replacing binary
// TIRE_DATA). The BLE_PROTOCOL_REFERENCE.md documents CORNER_READING
// at 26ac for backwards compatibility, but the v2 mobile app expects
// it at 26a8 per CLAUDE.md.
//
// Characteristics (v2):
//   CORNER_READING (26a8) — NOTIFY — JSON {corner, tireInside, tireMiddle, tireOutside, brakeTemp}
//   STATUS         (26aa) — READ, NOTIFY — JSON {battery, isCharging, firmware}
//   CORNER_ID      (26af) — READ, WRITE — UInt8 (0-3)
//
// Note: CORNER_READING JSON uses integer corner ID (0-3), matching
// the BLE_PROTOCOL_REFERENCE.md format.
// =================================================================

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"
#include "simulator.h"

class MockTireProbe {
public:
    // ── Simulation state ────────────────────────────────────────
    TempDrifter tireInner;    // Inner edge (hotter w/ negative camber)
    TempDrifter tireMiddle;   // Middle tread
    TempDrifter tireOuter;    // Outer edge
    TempDrifter brakeTemp;    // Brake rotor
    SimBattery battery;
    uint8_t cornerId;
    bool sensorError;
    bool probeConnected;

    // ── BLE handles ─────────────────────────────────────────────
    NimBLEService* pService              = nullptr;
    NimBLECharacteristic* pReadingChar   = nullptr;
    NimBLECharacteristic* pStatusChar    = nullptr;
    NimBLECharacteristic* pCornerChar    = nullptr;

    // ── Timing ──────────────────────────────────────────────────
    unsigned long lastReadingNotify = 0;
    unsigned long lastStatusNotify  = 0;
    bool statusDirty                = true;

    // ── Constructor ─────────────────────────────────────────────
    MockTireProbe(uint8_t corner = CORNER_LF)
        : tireInner(SIM_TIRE_INNER, 4.0f, 0.08f),
          tireMiddle(SIM_TIRE_MIDDLE, 3.5f, 0.07f),
          tireOuter(SIM_TIRE_OUTER, 4.5f, 0.09f),
          brakeTemp(SIM_BRAKE_TEMP, 30.0f, 0.05f),
          battery(4.1f, 0.00004f),
          cornerId(corner),
          sensorError(false),
          probeConnected(true) {

        // Vary temps per corner for realism
        // LF/RF fronts run slightly hotter from braking
        // Inner/outer spread indicates camber
        float cornerOffsets[] = {0.0f, -2.0f, -8.0f, -10.0f};
        float brakeOffsets[] = {0.0f, 5.0f, -50.0f, -45.0f};  // Rear brakes cooler
        float offset = cornerOffsets[corner % 4];

        tireInner.setBase(SIM_TIRE_INNER + offset);
        tireMiddle.setBase(SIM_TIRE_MIDDLE + offset);
        tireOuter.setBase(SIM_TIRE_OUTER + offset);
        brakeTemp.setBase(SIM_BRAKE_TEMP + brakeOffsets[corner % 4]);
    }

    // ── BLE Setup ───────────────────────────────────────────────
    void createService(NimBLEServer* pServer) {
        pService = pServer->createService(SVC_TIRE_PROBE);

        // CORNER_READING (26a8): NOTIFY — JSON (v2 slot)
        pReadingChar = pService->createCharacteristic(
            CHR_26A8,
            NIMBLE_PROPERTY::NOTIFY
        );

        // STATUS (26aa): READ + NOTIFY — JSON
        pStatusChar = pService->createCharacteristic(
            CHR_26AA,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // CORNER_ID (26af): READ + WRITE — UInt8
        pCornerChar = pService->createCharacteristic(
            CHR_26AF,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
        );
        pCornerChar->setCallbacks(new CornerIdCallback(this));

        // Set initial values
        updateReadingValue();
        updateStatusValue();
        pCornerChar->setValue(&cornerId, 1);

        pService->start();
        Serial.printf("[Probe-%s] Service started. JSON-only v2 mode\n",
            CORNER_NAMES[cornerId]);
    }

    // ── Main update loop ────────────────────────────────────────
    void update(unsigned long now) {
        float dt = 0.1f;

        // Update temperature drifters
        tireInner.update(dt, SIM_TIRE_NOISE);
        tireMiddle.update(dt, SIM_TIRE_NOISE);
        tireOuter.update(dt, SIM_TIRE_NOISE);
        brakeTemp.update(dt, SIM_TIRE_NOISE * 3.0f);  // Brake temps noisier
        battery.update(dt);

        // Notify CORNER_READING at 1 Hz
        if (now - lastReadingNotify >= UPD_PROBE_READING) {
            lastReadingNotify = now;
            updateReadingValue();
            pReadingChar->notify();
        }

        // Notify STATUS
        if (statusDirty || (now - lastStatusNotify >= UPD_PROBE_STATUS)) {
            lastStatusNotify = now;
            statusDirty = false;
            updateStatusValue();
            pStatusChar->notify();
        }
    }

    void printStatus() {
        Serial.printf("[Probe-%s] Inner=%.1f Mid=%.1f Outer=%.1f Brake=%.1f  Batt=%d%%\n",
            CORNER_NAMES[cornerId],
            tireInner.current, tireMiddle.current, tireOuter.current,
            brakeTemp.current, battery.percent);
    }

private:
    // ── Value formatters ────────────────────────────────────────
    void updateReadingValue() {
        // Per BLE_PROTOCOL_REFERENCE.md CORNER_READING format:
        // {corner: 0, tireInside: 185.2, tireMiddle: 188.5, tireOutside: 182.3, brakeTemp: 425.7}
        StaticJsonDocument<200> doc;
        doc["corner"] = (int)cornerId;
        doc["tireInside"] = roundf(tireInner.current * 10.0f) / 10.0f;
        doc["tireMiddle"] = roundf(tireMiddle.current * 10.0f) / 10.0f;
        doc["tireOutside"] = roundf(tireOuter.current * 10.0f) / 10.0f;
        doc["brakeTemp"] = roundf(brakeTemp.current * 10.0f) / 10.0f;
        String json;
        serializeJson(doc, json);
        pReadingChar->setValue(json.c_str());
    }

    void updateStatusValue() {
        // Per BLE_PROTOCOL_REFERENCE.md SYSTEM_STATUS:
        // {battery: 85, isCharging: false, firmware: "2.0.0"}
        StaticJsonDocument<128> doc;
        doc["battery"] = (int)battery.percent;
        doc["isCharging"] = false;
        doc["firmware"] = "2.0.0";
        String json;
        serializeJson(doc, json);
        pStatusChar->setValue(json.c_str());
    }

    // ── Write callbacks ─────────────────────────────────────────
    class CornerIdCallback : public NimBLECharacteristicCallbacks {
        MockTireProbe* dev;
    public:
        CornerIdCallback(MockTireProbe* d) : dev(d) {}

        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            if (raw.size() >= 1) {
                uint8_t newCorner = (uint8_t)raw[0];
                if (newCorner <= CORNER_RR) {
                    dev->cornerId = newCorner;
                    dev->statusDirty = true;
                    Serial.printf("[Probe] Corner → %s\n", CORNER_NAMES[newCorner]);

                    // Shift temps for new corner
                    float cornerOffsets[] = {0.0f, -2.0f, -8.0f, -10.0f};
                    float brakeOffsets[] = {0.0f, 5.0f, -50.0f, -45.0f};
                    float offset = cornerOffsets[newCorner];
                    dev->tireInner.setBase(SIM_TIRE_INNER + offset);
                    dev->tireMiddle.setBase(SIM_TIRE_MIDDLE + offset);
                    dev->tireOuter.setBase(SIM_TIRE_OUTER + offset);
                    dev->brakeTemp.setBase(SIM_BRAKE_TEMP + brakeOffsets[newCorner]);
                }
            }
        }
    };
};

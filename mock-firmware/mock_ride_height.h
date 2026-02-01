#pragma once
// =================================================================
// Ride Height Sensor Mock Device (0003)
// Service UUID: 4fafc201-0003-459e-8fcc-c5c9c331914b
// BLE Name: RH-Sensor_XX (XX = LF/RF/LR/RR)
//
// v2 BREAKING CHANGE: Now 4-corner (one device per corner)
//
// Characteristics (per BLE_PROTOCOL_REFERENCE.md):
//   HEIGHT    (26a8) — READ, NOTIFY  — CSV "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"
//   CMD       (26a9) — WRITE         — Single ASCII char: R/C/S/Z
//   STATUS    (26aa) — READ, NOTIFY  — JSON {zeroed, batteryLow, sensorError}
//   CORNER_ID (26af) — READ, WRITE, NOTIFY — UInt8 (0-3)
// =================================================================

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"
#include "simulator.h"

class MockRideHeight {
public:
    // ── Simulation state ────────────────────────────────────────
    SimValue sensor1;       // mm
    SimValue sensor2;       // mm
    SimBattery battery;
    uint8_t cornerId;
    bool zeroed;
    bool sensorError;
    bool continuousMode;
    float zeroOffset;       // mm offset from zero calibration

    // ── BLE handles ─────────────────────────────────────────────
    NimBLEService* pService             = nullptr;
    NimBLECharacteristic* pHeightChar   = nullptr;
    NimBLECharacteristic* pCmdChar      = nullptr;
    NimBLECharacteristic* pStatusChar   = nullptr;
    NimBLECharacteristic* pCornerChar   = nullptr;

    // ── Timing ──────────────────────────────────────────────────
    unsigned long lastHeightNotify  = 0;
    unsigned long lastStatusNotify  = 0;
    bool statusDirty                = true;
    bool singleReadPending          = false;

    // ── Constructor ─────────────────────────────────────────────
    MockRideHeight(uint8_t corner = CORNER_LF)
        : sensor1(SIM_RH_BASE_MM + SIM_RH_S1_OFFSET, 2.0f, SIM_RH_JITTER),
          sensor2(SIM_RH_BASE_MM + SIM_RH_S2_OFFSET, 2.0f, SIM_RH_JITTER),
          battery(SIM_RH_BATTERY_V, 0.00004f),
          cornerId(corner),
          zeroed(false),
          sensorError(false),
          continuousMode(true),   // Start in continuous mode
          zeroOffset(0.0f) {

        // Vary base height slightly per corner
        float offsets[] = {0.0f, 1.2f, -2.5f, -1.3f};
        float cornerOffset = offsets[corner % 4];
        sensor1.jumpTo(SIM_RH_BASE_MM + SIM_RH_S1_OFFSET + cornerOffset);
        sensor1.setTarget(sensor1.current);
        sensor2.jumpTo(SIM_RH_BASE_MM + SIM_RH_S2_OFFSET + cornerOffset);
        sensor2.setTarget(sensor2.current);
    }

    // ── BLE Setup ───────────────────────────────────────────────
    void createService(NimBLEServer* pServer) {
        pService = pServer->createService(SVC_RIDE_HEIGHT);

        // HEIGHT (26a8): READ + NOTIFY — CSV string
        pHeightChar = pService->createCharacteristic(
            CHR_26A8,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // CMD (26a9): WRITE — single ASCII char
        pCmdChar = pService->createCharacteristic(
            CHR_26A9,
            NIMBLE_PROPERTY::WRITE
        );
        pCmdChar->setCallbacks(new CmdCallback(this));

        // STATUS (26aa): READ + NOTIFY — JSON
        pStatusChar = pService->createCharacteristic(
            CHR_26AA,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // CORNER_ID (26af): READ + WRITE + NOTIFY — UInt8
        pCornerChar = pService->createCharacteristic(
            CHR_26AF,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
        );
        pCornerChar->setCallbacks(new CornerIdCallback(this));

        // Set initial values
        updateHeightValue();
        updateStatusValue();
        pCornerChar->setValue(&cornerId, 1);

        pService->start();
        Serial.printf("[RH-%s] Service started. Continuous mode ON\n",
            CORNER_NAMES[cornerId]);
    }

    // ── Main update loop ────────────────────────────────────────
    void update(unsigned long now) {
        float dt = 0.1f;

        // Update sensor simulation (slow drift + jitter)
        sensor1.update(dt);
        sensor2.update(dt);
        battery.update(dt);

        // Check battery low threshold (3.3V) — mark status dirty on transition
        static bool prevBattLow = false;
        bool battLow = battery.voltage < 3.3f;
        if (battLow != prevBattLow) {
            prevBattLow = battLow;
            statusDirty = true;
        }

        // Notify HEIGHT in continuous mode or for single read
        if (continuousMode && (now - lastHeightNotify >= UPD_RH_HEIGHT)) {
            lastHeightNotify = now;
            updateHeightValue();
            pHeightChar->notify();
        }

        if (singleReadPending) {
            singleReadPending = false;
            updateHeightValue();
            pHeightChar->notify();
        }

        // Notify STATUS
        if (statusDirty || (now - lastStatusNotify >= UPD_RH_STATUS)) {
            lastStatusNotify = now;
            statusDirty = false;
            updateStatusValue();
            pStatusChar->notify();
        }
    }

    void printStatus() {
        float s1 = sensor1.current - zeroOffset;
        float s2 = sensor2.current - zeroOffset;
        float avg = (s1 + s2) / 2.0f;
        float inches = avg / 25.4f;
        Serial.printf("[RH-%s] S1=%.1f S2=%.1f AVG=%.1f mm (%.2f\") Batt=%.2fV  Mode=%s\n",
            CORNER_NAMES[cornerId], s1, s2, avg, inches,
            battery.voltage,
            continuousMode ? "CONTINUOUS" : "STOPPED");
    }

private:
    // ── Value formatters ────────────────────────────────────────
    void updateHeightValue() {
        // CSV: "S1:123.4,S2:125.1,AVG:124.2,IN:4.89,BAT:3.85"
        float s1 = sensor1.current - zeroOffset;
        float s2 = sensor2.current - zeroOffset;
        float avg = (s1 + s2) / 2.0f;
        float inches = avg / 25.4f;

        char buf[80];
        snprintf(buf, sizeof(buf), "S1:%.1f,S2:%.1f,AVG:%.1f,IN:%.2f,BAT:%.2f",
            s1, s2, avg, inches, battery.voltage);
        pHeightChar->setValue(buf);
    }

    void updateStatusValue() {
        StaticJsonDocument<128> doc;
        doc["zeroed"] = zeroed;
        doc["batteryLow"] = battery.voltage < 3.3f;
        doc["sensorError"] = sensorError;
        String json;
        serializeJson(doc, json);
        pStatusChar->setValue(json.c_str());
    }

    // ── Write callbacks ─────────────────────────────────────────
    class CmdCallback : public NimBLECharacteristicCallbacks {
        MockRideHeight* dev;
    public:
        CmdCallback(MockRideHeight* d) : dev(d) {}

        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            if (raw.empty()) return;
            char cmd = raw[0];

            switch (cmd) {
                case 'R':  // Single reading
                    dev->singleReadPending = true;
                    Serial.printf("[RH-%s] Single reading requested\n",
                        CORNER_NAMES[dev->cornerId]);
                    break;

                case 'C':  // Start continuous
                    dev->continuousMode = true;
                    Serial.printf("[RH-%s] Continuous mode ON\n",
                        CORNER_NAMES[dev->cornerId]);
                    break;

                case 'S':  // Stop continuous
                    dev->continuousMode = false;
                    Serial.printf("[RH-%s] Continuous mode OFF\n",
                        CORNER_NAMES[dev->cornerId]);
                    break;

                case 'Z':  // Zero calibration
                    dev->zeroOffset = (dev->sensor1.current + dev->sensor2.current) / 2.0f;
                    dev->zeroed = true;
                    dev->statusDirty = true;
                    Serial.printf("[RH-%s] Zeroed at offset %.1f mm\n",
                        CORNER_NAMES[dev->cornerId], dev->zeroOffset);
                    break;

                default:
                    Serial.printf("[RH-%s] Unknown command: '%c'\n",
                        CORNER_NAMES[dev->cornerId], cmd);
                    break;
            }
        }
    };

    class CornerIdCallback : public NimBLECharacteristicCallbacks {
        MockRideHeight* dev;
    public:
        CornerIdCallback(MockRideHeight* d) : dev(d) {}

        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            if (raw.size() >= 1) {
                uint8_t newCorner = (uint8_t)raw[0];
                if (newCorner <= CORNER_RR) {
                    dev->cornerId = newCorner;
                    dev->statusDirty = true;
                    Serial.printf("[RH] Corner → %s\n", CORNER_NAMES[newCorner]);
                    pChar->setValue(&dev->cornerId, 1);
                    pChar->notify();
                }
            }
        }
    };
};

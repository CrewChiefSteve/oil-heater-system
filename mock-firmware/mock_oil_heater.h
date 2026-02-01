#pragma once
// =================================================================
// Oil Heater Mock Device (0001)
// Service UUID: 4fafc201-0001-459e-8fcc-c5c9c331914b
// BLE Name: Heater_MOCK
//
// Characteristics (per BLE_PROTOCOL_REFERENCE.md):
//   TEMPERATURE (26a8) — READ, NOTIFY — String "180.5"
//   SETPOINT    (26a9) — READ, WRITE, NOTIFY — String "180.0"
//   STATUS      (26aa) — READ, NOTIFY — JSON {heater, safetyShutdown, sensorError}
//
// NOTE: No ENABLE characteristic in v2 — heater state is in STATUS JSON.
// =================================================================

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"
#include "simulator.h"

class MockOilHeater {
public:
    // ── Simulation state ────────────────────────────────────────
    SimValue temperature;
    float setpoint;
    bool heaterOn;
    bool safetyShutdown;
    bool sensorError;
    bool statusDirty;

    // ── BLE handles ─────────────────────────────────────────────
    NimBLEService* pService          = nullptr;
    NimBLECharacteristic* pTempChar  = nullptr;
    NimBLECharacteristic* pSpChar    = nullptr;
    NimBLECharacteristic* pStatChar  = nullptr;

    // ── Timing ──────────────────────────────────────────────────
    unsigned long lastTempNotify   = 0;
    unsigned long lastStatusNotify = 0;

    // ── Constructor ─────────────────────────────────────────────
    MockOilHeater()
        : temperature(SIM_AMBIENT_TEMP, SIM_HEATER_HEAT_RATE, SIM_HEATER_NOISE),
          setpoint(SIM_HEATER_SETPOINT),
          heaterOn(true),
          safetyShutdown(false),
          sensorError(false),
          statusDirty(true) {
        temperature.setTarget(setpoint);
    }

    // ── BLE Setup ───────────────────────────────────────────────
    void createService(NimBLEServer* pServer) {
        pService = pServer->createService(SVC_OIL_HEATER);

        // TEMPERATURE (26a8): READ + NOTIFY
        pTempChar = pService->createCharacteristic(
            CHR_26A8,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // SETPOINT (26a9): READ + WRITE + NOTIFY
        pSpChar = pService->createCharacteristic(
            CHR_26A9,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
        );
        pSpChar->setCallbacks(new SetpointCallback(this));

        // STATUS (26aa): READ + NOTIFY
        pStatChar = pService->createCharacteristic(
            CHR_26AA,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
        );

        // Set initial values
        updateTempValue();
        updateSetpointValue();
        updateStatusValue();

        pService->start();
        Serial.printf("[Heater] Service started. Setpoint=%.1f°F\n", setpoint);
    }

    // ── Main update loop ────────────────────────────────────────
    void update(unsigned long now) {
        float dt = 0.1f;  // ~100ms update cycle

        // Simulation: heat toward setpoint or cool toward ambient
        if (heaterOn && !safetyShutdown) {
            temperature.setRate(SIM_HEATER_HEAT_RATE);
            temperature.setTarget(setpoint);
        } else {
            temperature.setRate(SIM_HEATER_COOL_RATE);
            temperature.setTarget(SIM_AMBIENT_TEMP);
        }
        temperature.update(dt);

        // Safety shutdown check
        if (temperature.current > SIM_HEATER_SAFETY_TEMP && !safetyShutdown) {
            safetyShutdown = true;
            heaterOn = false;
            statusDirty = true;
            Serial.println("[Heater] ⚠ SAFETY SHUTDOWN — temp exceeded limit!");
        }

        // PID-like cycling near setpoint
        if (!safetyShutdown) {
            if (heaterOn && temperature.current >= setpoint + 2.0f) {
                heaterOn = false;
                statusDirty = true;
            } else if (!heaterOn && temperature.current <= setpoint - 5.0f) {
                heaterOn = true;
                statusDirty = true;
            }
        }

        // Notify TEMPERATURE at 2 Hz
        if (now - lastTempNotify >= UPD_HEATER_TEMP) {
            lastTempNotify = now;
            updateTempValue();
            pTempChar->notify();
        }

        // Notify STATUS on change or every 2s
        if (statusDirty || (now - lastStatusNotify >= UPD_HEATER_STATUS)) {
            lastStatusNotify = now;
            statusDirty = false;
            updateStatusValue();
            pStatChar->notify();
        }
    }

    // ── Print info ──────────────────────────────────────────────
    void printStatus() {
        Serial.printf("[Heater] Temp=%.1f°F  SP=%.1f°F  Heater=%s  Safety=%s\n",
            temperature.current, setpoint,
            heaterOn ? "ON" : "OFF",
            safetyShutdown ? "SHUTDOWN" : "OK");
    }

private:
    // ── Value formatters ────────────────────────────────────────
    void updateTempValue() {
        // Per spec: plain string, e.g. "180.5"
        String s = String(temperature.current, 1);
        pTempChar->setValue(s.c_str());
    }

    void updateSetpointValue() {
        // Per spec: plain string, e.g. "180.0"
        String s = String(setpoint, 1);
        pSpChar->setValue(s.c_str());
    }

    void updateStatusValue() {
        // Per spec: JSON {heater, safetyShutdown, sensorError}
        StaticJsonDocument<128> doc;
        doc["heater"] = heaterOn;
        doc["safetyShutdown"] = safetyShutdown;
        doc["sensorError"] = sensorError;
        String json;
        serializeJson(doc, json);
        pStatChar->setValue(json.c_str());
    }

    // ── Write callback for SETPOINT ─────────────────────────────
    class SetpointCallback : public NimBLECharacteristicCallbacks {
        MockOilHeater* dev;
    public:
        SetpointCallback(MockOilHeater* d) : dev(d) {}

        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            float newSp = String(raw.c_str()).toFloat();

            if (newSp >= SIM_HEATER_MIN_SP && newSp <= SIM_HEATER_MAX_SP) {
                dev->setpoint = newSp;
                dev->temperature.setTarget(newSp);
                Serial.printf("[Heater] Setpoint → %.1f°F\n", newSp);

                // Reset safety if new setpoint is safe
                if (dev->safetyShutdown && newSp < SIM_HEATER_SAFETY_TEMP - 20.0f) {
                    dev->safetyShutdown = false;
                    dev->heaterOn = true;
                    Serial.println("[Heater] Safety reset, heater restarted");
                }

                // Echo back
                dev->updateSetpointValue();
                dev->pSpChar->notify();
                dev->statusDirty = true;
            } else {
                Serial.printf("[Heater] Rejected setpoint %.1f (range: %.0f–%.0f)\n",
                    newSp, SIM_HEATER_MIN_SP, SIM_HEATER_MAX_SP);
            }
        }
    };
};

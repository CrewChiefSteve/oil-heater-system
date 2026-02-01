#pragma once
// =================================================================
// Tire Temperature Gun Mock Device (0005)
// Service UUID: 4fafc201-0005-459e-8fcc-c5c9c331914b
// BLE Name: TireTempGun
//
// Handheld IR temperature gun — single device, no corner suffix.
//
// Characteristics (per BLE_PROTOCOL_REFERENCE.md):
//   TEMPERATURE (26a8) — NOTIFY — JSON {temp, amb, max, min, bat, mode}
//   COMMAND     (26a9) — WRITE  — String (EMIT:0.95, UNIT:F/C, RESET, LASER:ON/OFF)
//
// NOTE: No STATUS characteristic for this device.
// Battery is embedded in the TEMPERATURE JSON payload.
// =================================================================

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"
#include "simulator.h"

class MockTireTempGun {
public:
    // ── Simulation state ────────────────────────────────────────
    SimValue surfaceTemp;
    float ambientTemp;
    float maxTemp;
    float minTemp;
    SimBattery battery;
    float emissivity;
    bool useFahrenheit;
    bool laserOn;
    uint8_t mode;           // 0 = continuous, 1 = single shot
    bool triggerPending;    // For single-shot mode

    // ── BLE handles ─────────────────────────────────────────────
    NimBLEService* pService             = nullptr;
    NimBLECharacteristic* pTempChar     = nullptr;
    NimBLECharacteristic* pCmdChar      = nullptr;

    // ── Timing ──────────────────────────────────────────────────
    unsigned long lastTempNotify = 0;

    // ── Constructor ─────────────────────────────────────────────
    MockTireTempGun()
        : surfaceTemp(SIM_GUN_TEMP, 10.0f, SIM_GUN_NOISE),
          ambientTemp(SIM_GUN_AMBIENT),
          maxTemp(SIM_GUN_TEMP),
          minTemp(SIM_GUN_TEMP),
          battery(4.0f, 0.00003f),
          emissivity(SIM_GUN_DEFAULT_EMISSIVITY),
          useFahrenheit(true),
          laserOn(true),
          mode(0),
          triggerPending(false) {

        // Simulate pointing at different tire areas
        surfaceTemp.setTarget(SIM_GUN_TEMP);
    }

    // ── BLE Setup ───────────────────────────────────────────────
    void createService(NimBLEServer* pServer) {
        pService = pServer->createService(SVC_TIRE_TEMP_GUN);

        // TEMPERATURE (26a8): NOTIFY — JSON
        pTempChar = pService->createCharacteristic(
            CHR_26A8,
            NIMBLE_PROPERTY::NOTIFY
        );

        // COMMAND (26a9): WRITE — String
        pCmdChar = pService->createCharacteristic(
            CHR_26A9,
            NIMBLE_PROPERTY::WRITE
        );
        pCmdChar->setCallbacks(new CommandCallback(this));

        // Set initial value
        updateTempValue();

        pService->start();
        Serial.println("[Gun] Service started. Continuous mode, laser ON");
    }

    // ── Main update loop ────────────────────────────────────────
    void update(unsigned long now) {
        float dt = 0.1f;

        // Simulate "scanning" — temp drifts and occasionally jumps
        surfaceTemp.update(dt);
        battery.update(dt);

        // Periodically shift the target to simulate moving the gun
        static unsigned long lastTargetChange = 0;
        if (now - lastTargetChange > 4000 + random(6000)) {
            lastTargetChange = now;
            float newTarget = SIM_GUN_TEMP + randomFloat(-15.0f, 25.0f);
            surfaceTemp.setTarget(newTarget);
        }

        // Track min/max
        float currentReading = getDisplayTemp(surfaceTemp.current);
        if (currentReading > maxTemp) maxTemp = currentReading;
        if (currentReading < minTemp) minTemp = currentReading;

        // Notify in continuous mode at 4 Hz, or on single-shot trigger
        bool shouldNotify = false;
        if (mode == 0 && (now - lastTempNotify >= UPD_GUN_TEMP)) {
            shouldNotify = true;
        }
        if (mode == 1 && triggerPending) {
            shouldNotify = true;
            triggerPending = false;
        }

        if (shouldNotify) {
            lastTempNotify = now;
            updateTempValue();
            pTempChar->notify();
        }
    }

    void printStatus() {
        float t = getDisplayTemp(surfaceTemp.current);
        const char* unit = useFahrenheit ? "F" : "C";
        Serial.printf("[Gun] Temp=%.1f°%s  Min=%.1f Max=%.1f  Batt=%d%%  Emit=%.2f  Laser=%s  Mode=%s\n",
            t, unit, minTemp, maxTemp, battery.percent,
            emissivity,
            laserOn ? "ON" : "OFF",
            mode == 0 ? "CONTINUOUS" : "SINGLE");
    }

private:
    float getDisplayTemp(float tempF) {
        if (useFahrenheit) return tempF;
        return (tempF - 32.0f) * 5.0f / 9.0f;  // Convert to Celsius
    }

    // ── Value formatters ────────────────────────────────────────
    void updateTempValue() {
        // Per BLE_PROTOCOL_REFERENCE.md:
        // {temp: 185.5, amb: 72.3, max: 195.2, min: 175.8, bat: 85, mode: 0}
        float displayTemp = getDisplayTemp(surfaceTemp.current);
        float displayAmb = getDisplayTemp(ambientTemp);

        StaticJsonDocument<200> doc;
        doc["temp"] = roundf(displayTemp * 10.0f) / 10.0f;
        doc["amb"] = roundf(displayAmb * 10.0f) / 10.0f;
        doc["max"] = roundf(maxTemp * 10.0f) / 10.0f;
        doc["min"] = roundf(minTemp * 10.0f) / 10.0f;
        doc["bat"] = (int)battery.percent;
        doc["mode"] = (int)mode;
        String json;
        serializeJson(doc, json);
        pTempChar->setValue(json.c_str());
    }

    // ── Write callbacks ─────────────────────────────────────────
    class CommandCallback : public NimBLECharacteristicCallbacks {
        MockTireTempGun* dev;
    public:
        CommandCallback(MockTireTempGun* d) : dev(d) {}

        void onWrite(NimBLECharacteristic* pChar) override {
            std::string raw = pChar->getValue();
            String cmd = String(raw.c_str());
            cmd.trim();

            if (cmd.startsWith("EMIT:")) {
                float em = cmd.substring(5).toFloat();
                if (em >= 0.10f && em <= 1.0f) {
                    dev->emissivity = em;
                    Serial.printf("[Gun] Emissivity → %.2f\n", em);
                } else {
                    Serial.printf("[Gun] Rejected emissivity %.2f (range: 0.10–1.00)\n", em);
                }
            }
            else if (cmd == "UNIT:F") {
                dev->useFahrenheit = true;
                // Reset min/max for new unit
                dev->maxTemp = dev->getDisplayTemp(dev->surfaceTemp.current);
                dev->minTemp = dev->maxTemp;
                Serial.println("[Gun] Unit → Fahrenheit");
            }
            else if (cmd == "UNIT:C") {
                dev->useFahrenheit = false;
                dev->maxTemp = dev->getDisplayTemp(dev->surfaceTemp.current);
                dev->minTemp = dev->maxTemp;
                Serial.println("[Gun] Unit → Celsius");
            }
            else if (cmd == "RESET") {
                float t = dev->getDisplayTemp(dev->surfaceTemp.current);
                dev->maxTemp = t;
                dev->minTemp = t;
                Serial.println("[Gun] Min/Max reset");
            }
            else if (cmd == "LASER:ON") {
                dev->laserOn = true;
                Serial.println("[Gun] Laser ON");
            }
            else if (cmd == "LASER:OFF") {
                dev->laserOn = false;
                Serial.println("[Gun] Laser OFF");
            }
            else {
                Serial.printf("[Gun] Unknown command: '%s'\n", cmd.c_str());
            }
        }
    };
};

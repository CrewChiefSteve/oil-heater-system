#include <Arduino.h>
#include "config.h"
#include "pins.h"
#include "types.h"
#include "probes.h"
#include "ble_service.h"
#include "led.h"
#include "power.h"

// Global state
MeasurementData measurement;
SystemStatus systemStatus;

// Timing variables
uint32_t lastTempRead = 0;
uint32_t lastBleTx = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("Tire Temperature Probe");
    Serial.println("Model: " DEVICE_MODEL);
    Serial.println("=================================\n");

    // Initialize subsystems
    systemStatus.state = STATE_INITIALIZING;

    ledInit();
    powerInit();
    probesInit();
    bleInit(BLE_DEVICE_NAME);

    // Set initial corner
    measurement.corner = bleGetCorner();

    // Start BLE advertising
    bleStartAdvertising();

    Serial.println("Initialization complete\n");

    systemStatus.state = STATE_IDLE;
}

void loop() {
    uint32_t now = millis();

    // Update power status
    powerUpdate(systemStatus);

    // Read temperature probes
    if (now - lastTempRead >= TEMP_READ_INTERVAL_MS) {
        lastTempRead = now;

        probesUpdate(measurement);

        // Check for probe errors
        bool anyErrors = !measurement.tire.inside.isValid ||
                        !measurement.tire.middle.isValid ||
                        !measurement.tire.outside.isValid ||
                        !measurement.brake.rotor.isValid;

        if (anyErrors && systemStatus.state != STATE_LOW_BATTERY) {
            systemStatus.state = STATE_ERROR;
        } else if (systemStatus.state == STATE_ERROR && !anyErrors) {
            systemStatus.state = STATE_MEASURING;
        } else if (systemStatus.state == STATE_IDLE && !anyErrors) {
            systemStatus.state = STATE_MEASURING;
        }

        // Print temperature data to serial
        Serial.print("Tire [IN/MID/OUT]: ");
        Serial.print(measurement.tire.inside.temperature, 1);
        Serial.print(" / ");
        Serial.print(measurement.tire.middle.temperature, 1);
        Serial.print(" / ");
        Serial.print(measurement.tire.outside.temperature, 1);
        Serial.print(" C  |  Brake: ");
        Serial.print(measurement.brake.rotor.temperature, 1);
        Serial.print(" C  |  Bat: ");
        Serial.print(systemStatus.batteryPercent);
        Serial.print("%");
        if (systemStatus.charging) Serial.print(" (CHG)");
        Serial.println();
    }

    // Transmit BLE data
    if (bleIsConnected() && (now - lastBleTx >= BLE_TX_INTERVAL_MS)) {
        lastBleTx = now;

        systemStatus.state = STATE_TRANSMITTING;

        bleTransmitTireData(measurement.tire, measurement.corner, measurement.timestamp);
        bleTransmitBrakeData(measurement.brake, measurement.corner, measurement.timestamp);
        bleTransmitSystemStatus(systemStatus);

        systemStatus.state = STATE_MEASURING;
    }

    // Update LED status
    ledUpdate(systemStatus.state, bleIsConnected());

    // Update BLE
    bleUpdate();

    delay(10);  // Small delay to prevent watchdog issues
}

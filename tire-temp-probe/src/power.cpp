#include "power.h"
#include "pins.h"
#include "config.h"

static uint32_t lastBatteryRead = 0;

void powerInit() {
    pinMode(CHRG_STAT, INPUT);
    analogSetAttenuation(ADC_11db);  // For reading up to ~3.3V
    Serial.println("Power management initialized");
}

float powerReadVoltage() {
    uint32_t adcSum = 0;

    // Average multiple ADC readings
    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
        adcSum += analogRead(VBAT_ADC);
        delay(1);
    }

    uint32_t adcValue = adcSum / BATTERY_ADC_SAMPLES;

    // Convert ADC value to voltage
    // ESP32-S3 ADC: 12-bit (0-4095) at 11dB attenuation (~0-3.3V)
    float voltage = (adcValue / 4095.0) * 3.3 * BATTERY_VOLTAGE_DIVIDER;

    return voltage;
}

uint8_t powerCalculatePercent(float voltage) {
    if (voltage >= BATTERY_MAX_VOLTAGE) {
        return 100;
    } else if (voltage <= BATTERY_MIN_VOLTAGE) {
        return 0;
    }

    float range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
    float percent = ((voltage - BATTERY_MIN_VOLTAGE) / range) * 100.0;

    return (uint8_t)constrain(percent, 0, 100);
}

bool powerIsCharging() {
    // TP4056 CHRG pin: LOW when charging, HIGH when done/not charging
    return digitalRead(CHRG_STAT) == LOW;
}

void powerUpdate(SystemStatus &status) {
    uint32_t now = millis();

    if (now - lastBatteryRead >= BATTERY_READ_INTERVAL_MS) {
        lastBatteryRead = now;

        status.batteryVoltage = powerReadVoltage();
        status.batteryPercent = powerCalculatePercent(status.batteryVoltage);
        status.charging = powerIsCharging();

        if (status.batteryPercent <= BATTERY_LOW_THRESHOLD && !status.charging) {
            if (status.state != STATE_ERROR) {
                status.state = STATE_LOW_BATTERY;
            }
        }
    }

    status.uptimeMs = now;
}

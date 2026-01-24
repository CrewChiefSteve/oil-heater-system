#include "probes.h"
#include "pins.h"
#include "config.h"
#include <Adafruit_MAX31855.h>

// Thermocouple objects for each probe
Adafruit_MAX31855 tireInside(SPI_SCK, CS_TIRE_IN, SPI_MISO);
Adafruit_MAX31855 tireMiddle(SPI_SCK, CS_TIRE_MID, SPI_MISO);
Adafruit_MAX31855 tireOutside(SPI_SCK, CS_TIRE_OUT, SPI_MISO);
Adafruit_MAX31855 brakeRotor(SPI_SCK, CS_BRAKE, SPI_MISO);

// Moving average buffers
static float insideBuffer[TEMP_SMOOTHING_SAMPLES] = {0};
static float middleBuffer[TEMP_SMOOTHING_SAMPLES] = {0};
static float outsideBuffer[TEMP_SMOOTHING_SAMPLES] = {0};
static float brakeBuffer[TEMP_SMOOTHING_SAMPLES] = {0};
static uint8_t bufferIndex = 0;

void probesInit() {
    Serial.println("Initializing thermocouple probes...");

    delay(500);  // Allow MAX31855 chips to stabilize

    Serial.println("Probes initialized");
}

bool isTemperatureValid(float temp) {
    return !isnan(temp) && temp >= MIN_TEMP_C && temp <= MAX_TEMP_C;
}

bool readProbe(uint8_t csPin, ProbeData &probe) {
    Adafruit_MAX31855* thermocouplePtr = nullptr;

    // Select the correct thermocouple object
    if (csPin == CS_TIRE_IN) thermocouplePtr = &tireInside;
    else if (csPin == CS_TIRE_MID) thermocouplePtr = &tireMiddle;
    else if (csPin == CS_TIRE_OUT) thermocouplePtr = &tireOutside;
    else if (csPin == CS_BRAKE) thermocouplePtr = &brakeRotor;
    else return false;

    float temp = thermocouplePtr->readCelsius();

    if (isTemperatureValid(temp)) {
        probe.temperature = temp;
        probe.isValid = true;
        probe.errorCount = 0;
        probe.lastReadTime = millis();

        updateMovingAverage(probe);
        probe.isStable = checkStability(probe);

        return true;
    } else {
        probe.isValid = false;
        probe.errorCount++;

        if (probe.errorCount > 3) {
            Serial.print("Probe error on CS pin ");
            Serial.println(csPin);
        }

        return false;
    }
}

void updateMovingAverage(ProbeData &probe) {
    static float* currentBuffer = nullptr;

    // This is a simplified implementation
    // In a real system, you'd maintain separate buffers per probe
    probe.avgTemperature = probe.avgTemperature * 0.875 + probe.temperature * 0.125;
}

bool checkStability(ProbeData &probe) {
    float delta = abs(probe.temperature - probe.avgTemperature);
    return delta < TEMP_STABLE_THRESHOLD;
}

void probesUpdate(MeasurementData &data) {
    // Read all tire probes
    readProbe(CS_TIRE_IN, data.tire.inside);
    readProbe(CS_TIRE_MID, data.tire.middle);
    readProbe(CS_TIRE_OUT, data.tire.outside);

    // Calculate tire average
    data.tire.averageTemp = calculateTireAverage(data.tire);

    // Read brake probe
    readProbe(CS_BRAKE, data.brake.rotor);

    // Update timestamp
    data.timestamp = millis();
}

float calculateTireAverage(const TireChannel &tire) {
    uint8_t validCount = 0;
    float sum = 0.0;

    if (tire.inside.isValid) {
        sum += tire.inside.temperature;
        validCount++;
    }

    if (tire.middle.isValid) {
        sum += tire.middle.temperature;
        validCount++;
    }

    if (tire.outside.isValid) {
        sum += tire.outside.temperature;
        validCount++;
    }

    return (validCount > 0) ? (sum / validCount) : 0.0;
}

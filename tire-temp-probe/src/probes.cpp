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

// Stability tracking for auto-capture
#define STABILITY_SAMPLES 10          // ~1 second at 100ms reads
#define AMBIENT_THRESHOLD 40.0f       // °C - temps above this = contact detected

static float tempHistory[4][STABILITY_SAMPLES];  // Rolling history per probe [0=in, 1=mid, 2=out, 3=brake]
static uint8_t historyIndex = 0;
static uint32_t stableStartTime = 0;
static bool isCurrentlyStable = false;
static MeasurementData lastMeasurement;  // Store for capture snapshot

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

    // Store for capture snapshot
    lastMeasurement = data;

    // Update history for stability detection
    tempHistory[0][historyIndex] = data.tire.inside.temperature;
    tempHistory[1][historyIndex] = data.tire.middle.temperature;
    tempHistory[2][historyIndex] = data.tire.outside.temperature;
    tempHistory[3][historyIndex] = data.brake.rotor.temperature;
    historyIndex = (historyIndex + 1) % STABILITY_SAMPLES;
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

// ========== Stability Detection Functions ==========

void probesResetStability() {
    // Clear history arrays
    for (int probe = 0; probe < 4; probe++) {
        for (int sample = 0; sample < STABILITY_SAMPLES; sample++) {
            tempHistory[probe][sample] = 0.0f;
        }
    }

    historyIndex = 0;
    stableStartTime = 0;
    isCurrentlyStable = false;

    Serial.println("[PROBES] Stability reset");
}

bool probesDetectContact() {
    // Return true if ALL 4 probes read above ambient threshold
    // Indicates user has placed probes on tire/brake

    if (!lastMeasurement.tire.inside.isValid ||
        !lastMeasurement.tire.middle.isValid ||
        !lastMeasurement.tire.outside.isValid ||
        !lastMeasurement.brake.rotor.isValid) {
        return false;  // Need all valid readings
    }

    bool allAboveAmbient =
        (lastMeasurement.tire.inside.temperature > AMBIENT_THRESHOLD) &&
        (lastMeasurement.tire.middle.temperature > AMBIENT_THRESHOLD) &&
        (lastMeasurement.tire.outside.temperature > AMBIENT_THRESHOLD) &&
        (lastMeasurement.brake.rotor.temperature > AMBIENT_THRESHOLD);

    return allAboveAmbient;
}

bool probesAreStable() {
    // Check if all 4 probes have been stable for STABILITY_DURATION_MS

    // Check variance for each probe
    for (int probe = 0; probe < 4; probe++) {
        float minVal = tempHistory[probe][0];
        float maxVal = tempHistory[probe][0];

        for (int i = 1; i < STABILITY_SAMPLES; i++) {
            if (tempHistory[probe][i] < minVal) minVal = tempHistory[probe][i];
            if (tempHistory[probe][i] > maxVal) maxVal = tempHistory[probe][i];
        }

        float variance = maxVal - minVal;

        if (variance > TEMP_STABLE_THRESHOLD) {
            // Not stable - reset timer
            stableStartTime = 0;
            isCurrentlyStable = false;
            return false;
        }
    }

    // All probes stable - check duration
    if (!isCurrentlyStable) {
        stableStartTime = millis();
        isCurrentlyStable = true;
    }

    return (millis() - stableStartTime) >= STABILITY_DURATION_MS;
}

float probesGetStabilityProgress() {
    // Return 0.0-1.0 indicating progress toward stability threshold

    if (!isCurrentlyStable || stableStartTime == 0) {
        return 0.0f;
    }

    uint32_t elapsed = millis() - stableStartTime;
    float progress = (float)elapsed / (float)STABILITY_DURATION_MS;

    return min(progress, 1.0f);
}

CornerReading probesCapture(Corner corner) {
    // Snapshot current readings into CornerReading struct

    CornerReading reading;
    reading.corner = corner;
    reading.tireInside = lastMeasurement.tire.inside.temperature;
    reading.tireMiddle = lastMeasurement.tire.middle.temperature;
    reading.tireOutside = lastMeasurement.tire.outside.temperature;
    reading.brakeTemp = lastMeasurement.brake.rotor.temperature;

    // Calculate derived values
    reading.tireAverage = (reading.tireInside + reading.tireMiddle + reading.tireOutside) / 3.0f;

    float minTire = min(min(reading.tireInside, reading.tireMiddle), reading.tireOutside);
    float maxTire = max(max(reading.tireInside, reading.tireMiddle), reading.tireOutside);
    reading.tireSpread = maxTire - minTire;

    reading.timestamp = millis();

    Serial.printf("[PROBES] Captured %s | In:%.1f Mid:%.1f Out:%.1f Brake:%.1f\n",
                  corner == CORNER_RF ? "RF" : corner == CORNER_LF ? "LF" :
                  corner == CORNER_LR ? "LR" : "RR",
                  reading.tireInside, reading.tireMiddle,
                  reading.tireOutside, reading.brakeTemp);

    return reading;
}

#ifndef ADAPTIVE_FILTER_H
#define ADAPTIVE_FILTER_H

#include "config.h"

// ================================================================
// ADAPTIVE FILTER CLASS
// ================================================================

class AdaptiveFilter {
private:
    float lastValue = 0;
    float lastRawValue = 0;
    unsigned long lastChangeTime = 0;
    bool inTransition = false;
    float stabilityBuffer[5] = {0};
    uint8_t stabilityIndex = 0;

public:
    float update(float raw) {
        float diff = abs(raw - lastRawValue);

        // Detect significant change
        if (diff > ScaleConfig::CHANGE_DETECT_THRESHOLD) {
            inTransition = true;
            lastChangeTime = millis();
        }

        // Check if we've settled
        if (inTransition && (millis() - lastChangeTime > ScaleConfig::SETTLE_TIME_MS)) {
            inTransition = false;
        }

        // Apply adaptive filtering
        float alpha = inTransition ? ScaleConfig::FAST_FILTER_ALPHA : ScaleConfig::SLOW_FILTER_ALPHA;

        // First reading initialization
        if (lastValue == 0) {
            lastValue = raw;
        }

        lastValue = (alpha * raw) + ((1.0f - alpha) * lastValue);
        lastRawValue = raw;

        // Update stability buffer
        stabilityBuffer[stabilityIndex] = lastValue;
        stabilityIndex = (stabilityIndex + 1) % 5;

        return lastValue;
    }

    bool isStable() {
        if (inTransition) return false;

        float minVal = stabilityBuffer[0];
        float maxVal = stabilityBuffer[0];

        for (int i = 1; i < 5; i++) {
            if (stabilityBuffer[i] < minVal) minVal = stabilityBuffer[i];
            if (stabilityBuffer[i] > maxVal) maxVal = stabilityBuffer[i];
        }

        return (maxVal - minVal) < ScaleConfig::STABILITY_RANGE;
    }

    void reset() {
        lastValue = 0;
        lastRawValue = 0;
        inTransition = false;
        for (int i = 0; i < 5; i++) stabilityBuffer[i] = 0;
        stabilityIndex = 0;
    }
};

#endif // ADAPTIVE_FILTER_H

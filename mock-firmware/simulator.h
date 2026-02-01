#pragma once
// =================================================================
// Simulation Utilities — Realistic fake data generation
//
// SimValue: Smoothly drifts toward a target with configurable noise.
//   Great for temperatures, weights, heights — anything that shouldn't
//   just be random noise.
//
// SimBattery: Slowly draining battery with voltage→percentage mapping.
//
// DampedOscillator: For weight settling (car placed on scale).
// =================================================================

#include <Arduino.h>
#include <math.h>

// ── Random helpers ───────────────────────────────────────────────
static float randomFloat(float minVal, float maxVal) {
    return minVal + (random(10001) / 10000.0f) * (maxVal - minVal);
}

static float gaussianNoise(float amplitude) {
    // Box-Muller approximation (cheap, good enough for mock data)
    float u1 = randomFloat(0.001f, 1.0f);
    float u2 = randomFloat(0.0f, 6.2832f);
    return amplitude * sqrtf(-2.0f * logf(u1)) * cosf(u2);
}

// ── SimValue: smooth drift toward target with noise ─────────────
class SimValue {
public:
    float current;
    float target;
    float maxRate;    // max change per second
    float noise;      // noise amplitude (Gaussian)

    SimValue(float initial = 0.0f, float rate = 1.0f, float noiseAmp = 0.1f)
        : current(initial), target(initial), maxRate(rate), noise(noiseAmp) {}

    void update(float dtSec) {
        float diff = target - current;
        float maxChange = maxRate * dtSec;

        if (fabsf(diff) > maxChange) {
            current += (diff > 0.0f) ? maxChange : -maxChange;
        } else {
            current = target;
        }

        // Add Gaussian noise
        if (noise > 0.0f) {
            current += gaussianNoise(noise);
        }
    }

    void setTarget(float t) { target = t; }
    void jumpTo(float v) { current = v; target = v; }
    void setRate(float r) { maxRate = r; }
};

// ── SimBattery: slowly draining battery ─────────────────────────
class SimBattery {
public:
    float voltage;       // 3.0–4.2V range
    uint8_t percent;     // 0–100%
    float drainRate;     // V per second (very slow)

    SimBattery(float initialV = 4.2f, float drain = 0.00005f)
        : voltage(initialV), drainRate(drain) {
        percent = voltageToPercent(voltage);
    }

    void update(float dtSec) {
        voltage -= drainRate * dtSec;
        if (voltage < 3.0f) voltage = 3.0f;
        percent = voltageToPercent(voltage);
    }

    void reset() {
        voltage = 4.2f;
        percent = 100;
    }

    static uint8_t voltageToPercent(float v) {
        // LiPo discharge curve approximation (3.0V=0%, 4.2V=100%)
        float pct = (v - 3.0f) / 1.2f * 100.0f;
        return (uint8_t)constrain(pct, 0.0f, 100.0f);
    }
};

// ── DampedOscillator: for scale weight settling ─────────────────
// Simulates placing a car on a scale — weight overshoots then settles
class DampedOscillator {
public:
    float current;
    float target;
    float amplitude;
    float decay;        // damping factor
    float frequency;    // oscillation frequency
    float elapsed;      // time since trigger
    bool settling;

    DampedOscillator(float initial = 0.0f)
        : current(initial), target(initial), amplitude(0.0f),
          decay(2.5f), frequency(3.0f), elapsed(0.0f), settling(false) {}

    void triggerSettle(float newTarget) {
        amplitude = (newTarget - current) * 0.15f;  // 15% overshoot
        target = newTarget;
        elapsed = 0.0f;
        settling = true;
    }

    void update(float dtSec, float noiseAmp = 0.1f) {
        if (settling) {
            elapsed += dtSec;
            float envelope = amplitude * expf(-decay * elapsed);

            if (fabsf(envelope) < 0.05f) {
                // Settled
                current = target + gaussianNoise(noiseAmp);
                settling = false;
            } else {
                current = target + envelope * sinf(frequency * 6.2832f * elapsed);
                current += gaussianNoise(noiseAmp * 0.5f);
            }
        } else {
            // Settled — just add noise
            current = target + gaussianNoise(noiseAmp);
        }
    }
};

// ── Temperature drift simulator ─────────────────────────────────
// For tire temps that slowly drift in a realistic pattern
class TempDrifter {
    float baseTemp;
    float driftRange;
    float phase;
    float phaseRate;

public:
    float current;

    TempDrifter(float base = 185.0f, float drift = 5.0f, float rate = 0.1f)
        : baseTemp(base), driftRange(drift), phase(randomFloat(0, 6.28f)),
          phaseRate(rate), current(base) {}

    void update(float dtSec, float noiseAmp = 1.0f) {
        phase += phaseRate * dtSec;
        if (phase > 6.2832f) phase -= 6.2832f;
        current = baseTemp + driftRange * sinf(phase) + gaussianNoise(noiseAmp);
    }

    void setBase(float b) { baseTemp = b; }
};

#include "led.h"
#include "pins.h"
#include "config.h"
#include <FastLED.h>

#define NUM_LEDS 1

static CRGB leds[NUM_LEDS];
static uint32_t lastBlinkTime = 0;
static bool blinkState = false;

void ledInit() {
    FastLED.addLeds<WS2812B, RGB_LED, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    ledOff();
    Serial.println("LED initialized");
}

void ledSolid(uint8_t r, uint8_t g, uint8_t b) {
    leds[0] = CRGB(r, g, b);
    FastLED.show();
}

void ledOff() {
    leds[0] = CRGB::Black;
    FastLED.show();
}

void ledBlink(uint8_t r, uint8_t g, uint8_t b, uint16_t intervalMs) {
    uint32_t now = millis();

    if (now - lastBlinkTime >= intervalMs) {
        lastBlinkTime = now;
        blinkState = !blinkState;

        if (blinkState) {
            ledSolid(r, g, b);
        } else {
            ledOff();
        }
    }
}

void ledBreathing(uint8_t r, uint8_t g, uint8_t b) {
    // Sine wave breathing: 2 second cycle (2000ms period)
    // millis() / 318.0 gives ~2 second period for full sine wave (2*PI*318 ~= 2000)
    float breath = (sin(millis() / 318.0) + 1.0) / 2.0;  // 0.0 to 1.0
    uint8_t brightness = (uint8_t)(breath * 255);

    leds[0] = CRGB(r, g, b);
    leds[0].nscale8(brightness);
    FastLED.show();
}

void ledPulse(uint8_t r, uint8_t g, uint8_t b) {
    // Fast pulse: 500ms cycle with quick ramp up/down
    uint32_t phase = millis() % 500;
    uint8_t brightness;

    if (phase < 100) {
        // Ramp up: 0-100ms
        brightness = map(phase, 0, 100, 0, 255);
    } else if (phase < 200) {
        // Ramp down: 100-200ms
        brightness = map(phase, 100, 200, 255, 0);
    } else {
        // Off: 200-500ms
        brightness = 0;
    }

    leds[0] = CRGB(r, g, b);
    leds[0].nscale8(brightness);
    FastLED.show();
}

void ledUpdate(DeviceState state) {
    switch (state) {
        case STATE_INITIALIZING:
            ledBlink(255, 200, 0, 100);  // Yellow fast blink
            break;

        case STATE_WAITING_CONNECTION:
            ledBreathing(0, 0, 255);  // Blue breathing
            break;

        case STATE_CORNER_RF:
        case STATE_CORNER_LF:
        case STATE_CORNER_LR:
        case STATE_CORNER_RR:
            ledOff();  // Off while waiting for contact
            break;

        case STATE_STABILIZING_RF:
        case STATE_STABILIZING_LF:
        case STATE_STABILIZING_LR:
        case STATE_STABILIZING_RR:
            ledPulse(255, 200, 0);  // Yellow pulse
            break;

        case STATE_CAPTURED_RF:
        case STATE_CAPTURED_LF:
        case STATE_CAPTURED_LR:
        case STATE_CAPTURED_RR:
            ledSolid(0, 255, 0);  // Green solid - GO!
            break;

        case STATE_SESSION_COMPLETE:
            ledSolid(0, 255, 0);  // Green solid
            break;

        case STATE_ERROR:
            ledBlink(255, 0, 0, 100);  // Red fast blink
            break;

        default:
            ledOff();
            break;
    }
}

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

void ledSetColor(uint8_t r, uint8_t g, uint8_t b) {
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
            ledSetColor(r, g, b);
        } else {
            ledOff();
        }
    }
}

void ledUpdate(DeviceState state, bool connected) {
    switch (state) {
        case STATE_INITIALIZING:
            ledBlink(255, 255, 0, 200);  // Yellow fast blink
            break;

        case STATE_IDLE:
            if (connected) {
                ledSetColor(0, 0, 255);  // Blue solid (connected, idle)
            } else {
                ledBlink(0, 255, 0, LED_IDLE_INTERVAL_MS);  // Green slow blink (not connected)
            }
            break;

        case STATE_MEASURING:
            if (connected) {
                ledSetColor(0, 255, 0);  // Green solid (connected, measuring)
            } else {
                ledBlink(0, 255, 0, LED_ACTIVE_INTERVAL_MS);  // Green fast blink
            }
            break;

        case STATE_TRANSMITTING:
            ledBlink(0, 255, 255, 100);  // Cyan very fast blink
            break;

        case STATE_LOW_BATTERY:
            ledBlink(255, 128, 0, 500);  // Orange blink
            break;

        case STATE_ERROR:
            ledBlink(255, 0, 0, LED_ERROR_INTERVAL_MS);  // Red fast blink
            break;

        default:
            ledOff();
            break;
    }
}

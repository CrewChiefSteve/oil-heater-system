#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include "types.h"

// Initialize LED
void ledInit();

// Update LED based on device state
void ledUpdate(DeviceState state, bool connected);

// Set LED to specific color
void ledSetColor(uint8_t r, uint8_t g, uint8_t b);

// Turn LED off
void ledOff();

// Blink LED (non-blocking)
void ledBlink(uint8_t r, uint8_t g, uint8_t b, uint16_t intervalMs);

#endif // LED_H

#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include "types.h"

void ledInit();
void ledUpdate(DeviceState state);  // Call in loop - handles patterns

// Direct control (optional use)
void ledOff();
void ledSolid(uint8_t r, uint8_t g, uint8_t b);
void ledBlink(uint8_t r, uint8_t g, uint8_t b, uint16_t intervalMs);
void ledBreathing(uint8_t r, uint8_t g, uint8_t b);  // Smooth fade in/out
void ledPulse(uint8_t r, uint8_t g, uint8_t b);      // Quick pulse pattern

#endif // LED_H

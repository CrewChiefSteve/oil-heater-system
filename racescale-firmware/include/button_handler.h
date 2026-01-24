#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "config.h"

// ================================================================
// BUTTON HANDLER CLASS
// ================================================================

class ButtonHandler {
private:
    uint8_t pin;
    unsigned long lastPressTime = 0;
    unsigned long pressStartTime = 0;
    bool isPressed = false;
    bool longPressTriggered = false;

public:
    ButtonHandler(uint8_t buttonPin) : pin(buttonPin) {
        pinMode(pin, INPUT_PULLUP);
    }

    enum ButtonEvent {
        NONE,
        SHORT_PRESS,
        LONG_PRESS
    };

    ButtonEvent update() {
        bool currentState = (digitalRead(pin) == LOW);
        ButtonEvent event = NONE;

        if (currentState && !isPressed) {
            // Button just pressed
            if (millis() - lastPressTime > ScaleConfig::BUTTON_DEBOUNCE_MS) {
                isPressed = true;
                pressStartTime = millis();
                longPressTriggered = false;
            }
        } else if (!currentState && isPressed) {
            // Button just released
            isPressed = false;
            lastPressTime = millis();

            if (!longPressTriggered) {
                event = SHORT_PRESS;
            }
        } else if (isPressed && !longPressTriggered) {
            // Button being held
            if (millis() - pressStartTime > ScaleConfig::BUTTON_HOLD_MS) {
                longPressTriggered = true;
                event = LONG_PRESS;
            }
        }

        return event;
    }
};

#endif // BUTTON_HANDLER_H

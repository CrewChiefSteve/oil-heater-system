#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"

// ==================== Global Objects ====================

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTempChar = nullptr;
NimBLECharacteristic* pCmdChar = nullptr;

// ==================== State Variables ====================

// Temperature data
float currentTempF = 0.0;
float ambientTempF = 0.0;
float maxTempF = -999.0;
float minTempF = 999.0;
float heldTempF = 0.0;

// Display mode
bool useFahrenheit = true;
MeasurementMode currentMode = MODE_INSTANT;

// Button states
struct ButtonState {
    bool lastState;
    bool currentState;
    unsigned long lastDebounceTime;
    unsigned long pressStartTime;
    bool longPressHandled;
};

ButtonState triggerBtn = {HIGH, HIGH, 0, 0, false};
ButtonState modeBtn = {HIGH, HIGH, 0, 0, false};
ButtonState holdBtn = {HIGH, HIGH, 0, 0, false};

// Battery level
int batteryPercent = 100;

// BLE connection status
bool bleConnected = false;

// Timing
unsigned long lastTempRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBleNotify = 0;

// ==================== BLE Callbacks ====================

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        bleConnected = true;
        Serial.println("BLE client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        bleConnected = false;
        Serial.println("BLE client disconnected");
        // Restart advertising
        NimBLEDevice::startAdvertising();
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String cmd = String(value.c_str());
            Serial.print("Received command: ");
            Serial.println(cmd);

            // Parse string commands per BLE protocol
            // Expected commands: EMIT:0.95, UNIT:F, UNIT:C, RESET, LASER:ON, LASER:OFF

            if (cmd.startsWith("EMIT:")) {
                // Set emissivity (e.g., "EMIT:0.95")
                float emissivity = cmd.substring(5).toFloat();
                if (emissivity >= 0.10 && emissivity <= 1.00) {
                    // Note: Writing emissivity to MLX90614 EEPROM requires caution
                    // Limited write cycles (~100k), and incorrect value can brick sensor
                    // mlx.writeEmissivity(emissivity);  // DANGER: Use with extreme caution!
                    Serial.printf("Emissivity command received: %.2f (not written to EEPROM)\n", emissivity);
                    // TODO: If needed, implement RAM-only emissivity compensation
                } else {
                    Serial.printf("Invalid emissivity: %.2f (must be 0.10-1.00)\n", emissivity);
                }
            }
            else if (cmd == "UNIT:F") {
                // Set Fahrenheit
                useFahrenheit = true;
                Serial.println("Unit set to Fahrenheit via BLE");
            }
            else if (cmd == "UNIT:C") {
                // Set Celsius
                useFahrenheit = false;
                Serial.println("Unit set to Celsius via BLE");
            }
            else if (cmd == "RESET") {
                // Reset min/max values
                maxTempF = currentTempF;
                minTempF = currentTempF;
                playTone(BUZZ_FREQ_RESET, BUZZ_DURATION_MS);
                Serial.println("Max/Min reset via BLE");
            }
            else if (cmd == "LASER:ON") {
                // Turn laser on (note: physical trigger button will override)
                digitalWrite(PIN_LASER, HIGH);
                Serial.println("Laser ON via BLE");
            }
            else if (cmd == "LASER:OFF") {
                // Turn laser off
                digitalWrite(PIN_LASER, LOW);
                Serial.println("Laser OFF via BLE");
            }
            else {
                Serial.print("Unknown command: ");
                Serial.println(cmd);
            }
        }
    }
};

// ==================== Helper Functions ====================

void playTone(int frequency, int duration) {
    tone(PIN_BUZZER, frequency, duration);
}

float celsiusToFahrenheit(float c) {
    return (c * 9.0 / 5.0) + 32.0;
}

float fahrenheitToCelsius(float f) {
    return (f - 32.0) * 5.0 / 9.0;
}

int readBatteryPercent() {
    long sum = 0;
    for (int i = 0; i < BAT_ADC_SAMPLES; i++) {
        sum += analogRead(PIN_BAT_SENSE);
        delay(1);
    }
    float adcValue = sum / (float)BAT_ADC_SAMPLES;

    // Convert ADC to voltage (ESP32 ADC: 0-4095 = 0-3.3V)
    float voltage = (adcValue / 4095.0) * 3.3 * BAT_VOLTAGE_DIVIDER_RATIO;

    // Map voltage to percentage
    int percent = (int)((voltage - BAT_MIN_VOLTAGE) / (BAT_MAX_VOLTAGE - BAT_MIN_VOLTAGE) * 100.0);
    return constrain(percent, 0, 100);
}

void updateButton(ButtonState &btn, int pin) {
    bool reading = digitalRead(pin);

    if (reading != btn.lastState) {
        btn.lastDebounceTime = millis();
    }

    if ((millis() - btn.lastDebounceTime) > DEBOUNCE_MS) {
        if (reading != btn.currentState) {
            btn.currentState = reading;

            if (btn.currentState == LOW) {
                // Button pressed
                btn.pressStartTime = millis();
                btn.longPressHandled = false;
            }
        }
    }

    btn.lastState = reading;
}

bool isButtonPressed(ButtonState &btn) {
    return (btn.currentState == LOW && btn.lastState == HIGH);
}

bool isButtonReleased(ButtonState &btn) {
    return (btn.currentState == HIGH && btn.lastState == LOW);
}

bool isButtonHeld(ButtonState &btn) {
    return (btn.currentState == LOW);
}

bool isLongPress(ButtonState &btn) {
    if (btn.currentState == LOW && !btn.longPressHandled) {
        if ((millis() - btn.pressStartTime) >= LONG_PRESS_MS) {
            btn.longPressHandled = true;
            return true;
        }
    }
    return false;
}

void sendBleData() {
    if (!bleConnected || pTempChar == nullptr) return;

    JsonDocument doc;

    // Select temperature based on mode
    float displayTemp = currentTempF;
    switch (currentMode) {
        case MODE_HOLD:
            displayTemp = heldTempF;
            break;
        case MODE_MAX:
            displayTemp = maxTempF;
            break;
        case MODE_MIN:
            displayTemp = minTempF;
            break;
        default:
            displayTemp = currentTempF;
            break;
    }

    doc["temp"] = round(displayTemp * 10) / 10.0;
    doc["amb"] = round(ambientTempF * 10) / 10.0;
    doc["max"] = round(maxTempF * 10) / 10.0;
    doc["min"] = round(minTempF * 10) / 10.0;
    doc["bat"] = batteryPercent;
    doc["mode"] = (int)currentMode;
    doc["unit"] = useFahrenheit ? "F" : "C";

    String jsonString;
    serializeJson(doc, jsonString);

    pTempChar->setValue(jsonString.c_str());
    pTempChar->notify();
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Top bar: Mode and battery
    display.setTextSize(1);
    display.setCursor(0, 0);

    const char* modeNames[] = {"INSTANT", "HOLD", "MAX", "MIN"};
    display.print(modeNames[currentMode]);

    // Battery indicator
    display.setCursor(90, 0);
    display.print("BAT:");
    display.print(batteryPercent);
    display.print("%");

    // BLE status
    if (bleConnected) {
        display.setCursor(0, 10);
        display.print("BLE");
    }

    // Main temperature reading
    float displayTemp = currentTempF;
    float displayAmbient = ambientTempF;

    switch (currentMode) {
        case MODE_HOLD:
            displayTemp = heldTempF;
            break;
        case MODE_MAX:
            displayTemp = maxTempF;
            break;
        case MODE_MIN:
            displayTemp = minTempF;
            break;
        default:
            break;
    }

    // Convert to Celsius if needed
    if (!useFahrenheit) {
        displayTemp = fahrenheitToCelsius(displayTemp);
        displayAmbient = fahrenheitToCelsius(displayAmbient);
    }

    // Large temperature display
    display.setTextSize(3);
    display.setCursor(0, 25);

    if (displayTemp > -999 && displayTemp < 999) {
        display.print(displayTemp, 1);
        display.setTextSize(2);
        display.print(useFahrenheit ? "F" : "C");
    } else {
        display.setTextSize(2);
        display.print("--.-");
    }

    // Ambient temperature
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print("Ambient: ");
    display.print(displayAmbient, 1);
    display.print(useFahrenheit ? "F" : "C");

    display.display();
}

// ==================== Setup ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Tire Temperature Gun ===");

    // Initialize pins
    pinMode(PIN_TRIGGER, INPUT_PULLUP);
    pinMode(PIN_MODE, INPUT_PULLUP);
    pinMode(PIN_HOLD, INPUT_PULLUP);
    pinMode(PIN_LASER, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_BAT_SENSE, INPUT);

    digitalWrite(PIN_LASER, LOW);

    // Initialize I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);  // 400kHz Fast Mode

    // Initialize MLX90614
    Serial.println("Initializing MLX90614...");
    if (!mlx.begin()) {
        Serial.println("ERROR: MLX90614 not found!");
        while (1) {
            playTone(1000, 100);
            delay(500);
        }
    }
    Serial.println("MLX90614 initialized");

    // Set emissivity for rubber tires
    // Note: Emissivity setting requires writing to EEPROM, use with caution
    // mlx.writeEmissivity(DEFAULT_EMISSIVITY);

    // Initialize display
    Serial.println("Initializing SSD1306...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDR)) {
        Serial.println("ERROR: SSD1306 not found!");
        while (1) {
            playTone(1500, 100);
            delay(500);
        }
    }
    Serial.println("SSD1306 initialized");

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Tire Temp Gun");
    display.println("Initializing...");
    display.display();
    delay(1000);

    // Initialize BLE
    Serial.println("Initializing BLE...");
    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Create BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create BLE Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Temperature characteristic (Read + Notify)
    pTempChar = pService->createCharacteristic(
        CHAR_TEMP_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Command characteristic (Write)
    pCmdChar = pService->createCharacteristic(
        CHAR_CMD_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pCmdChar->setCallbacks(new CharacteristicCallbacks());

    // Start service
    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE advertising started");
    Serial.print("Device name: ");
    Serial.println(BLE_DEVICE_NAME);

    // Startup beep
    playTone(BUZZ_FREQ_MODE, 100);
    delay(100);
    playTone(BUZZ_FREQ_MODE, 100);

    Serial.println("Ready!");
}

// ==================== Main Loop ====================

void loop() {
    unsigned long now = millis();

    // Read temperature sensor
    if (now - lastTempRead >= TEMP_READ_INTERVAL_MS) {
        lastTempRead = now;

        float objTempC = mlx.readObjectTempC();
        float ambTempC = mlx.readAmbientTempC();

        // Convert to Fahrenheit (store internally as F)
        currentTempF = celsiusToFahrenheit(objTempC);
        ambientTempF = celsiusToFahrenheit(ambTempC);

        // Update max/min tracking
        if (currentMode == MODE_INSTANT || currentMode == MODE_HOLD) {
            if (currentTempF > maxTempF || maxTempF < -900) {
                maxTempF = currentTempF;
            }
            if (currentTempF < minTempF || minTempF > 900) {
                minTempF = currentTempF;
            }
        }

        // Read battery level (less frequently)
        static int tempReadCount = 0;
        if (++tempReadCount >= 10) {  // Every 1 second
            tempReadCount = 0;
            batteryPercent = readBatteryPercent();
        }
    }

    // Handle buttons
    updateButton(triggerBtn, PIN_TRIGGER);
    updateButton(modeBtn, PIN_MODE);
    updateButton(holdBtn, PIN_HOLD);

    // Trigger button: Control laser
    if (isButtonHeld(triggerBtn)) {
        digitalWrite(PIN_LASER, HIGH);
    } else {
        digitalWrite(PIN_LASER, LOW);
    }

    // Mode button: Cycle through modes
    static bool modeBtnWasPressed = false;
    if (modeBtn.currentState == LOW && modeBtn.lastState == HIGH) {
        modeBtnWasPressed = true;
    }
    if (modeBtn.currentState == HIGH && modeBtnWasPressed) {
        modeBtnWasPressed = false;

        // Cycle mode
        currentMode = (MeasurementMode)((currentMode + 1) % 4);

        // Capture current temp for HOLD mode
        if (currentMode == MODE_HOLD) {
            heldTempF = currentTempF;
        }

        playTone(BUZZ_FREQ_MODE, BUZZ_DURATION_MS);
        Serial.print("Mode changed to: ");
        Serial.println(currentMode);
    }

    // Hold button: Toggle F/C or reset max/min on long press
    if (isLongPress(holdBtn)) {
        // Long press: Reset max/min
        maxTempF = currentTempF;
        minTempF = currentTempF;
        playTone(BUZZ_FREQ_RESET, 200);
        Serial.println("Max/Min reset");
    } else {
        static bool holdBtnWasPressed = false;
        if (holdBtn.currentState == LOW && holdBtn.lastState == HIGH) {
            holdBtnWasPressed = true;
        }
        if (holdBtn.currentState == HIGH && holdBtnWasPressed) {
            holdBtnWasPressed = false;

            // Short press: Toggle unit
            useFahrenheit = !useFahrenheit;
            playTone(BUZZ_FREQ_BUTTON, BUZZ_DURATION_MS);
            Serial.print("Unit changed to: ");
            Serial.println(useFahrenheit ? "Fahrenheit" : "Celsius");
        }
    }

    // Update display
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
        lastDisplayUpdate = now;
        updateDisplay();
    }

    // Send BLE data
    if (bleConnected && (now - lastBleNotify >= BLE_NOTIFY_INTERVAL_MS)) {
        lastBleNotify = now;
        sendBleData();
    }

    // Small delay to prevent WDT issues
    delay(1);
}

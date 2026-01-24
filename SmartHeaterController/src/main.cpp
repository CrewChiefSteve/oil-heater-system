#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_MCP23X17.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <cmath>  // For fabsf()
#include "config.h"

// ========================================
// Global Objects
// ========================================
TFT_eSPI tft = TFT_eSPI();
Adafruit_MCP23X17 mcp;

// BLE objects
BLEServer* pServer = nullptr;
BLECharacteristic* pTempCharacteristic = nullptr;
BLECharacteristic* pSetpointCharacteristic = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
volatile bool deviceConnected = false;

// BLE UUIDs
// Service UUID - MUST match SERVICE_UUIDS.OIL_HEATER in @crewchiefsteve/ble package
// See packages/ble/src/constants/uuids.ts for all device UUIDs
#define SERVICE_UUID        "4fafc201-0001-459e-8fcc-c5c9c331914b"
#define TEMP_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SETPOINT_CHAR_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define STATUS_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26aa"

// BLE Server Callbacks (static instance)
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("BLE Client connected");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE Client disconnected");
        // Restart advertising
        BLEDevice::startAdvertising();
        Serial.println("BLE advertising restarted");
    }
};

// Static instance to avoid memory leak
static MyServerCallbacks serverCallbacks;

// Touch calibration data (calibrated for this 3.5" CYD board)
uint16_t calData[5] = {326, 3433, 551, 3091, 7};

// ========================================
// State Variables (volatile for thread safety with BLE callbacks)
// ========================================
volatile float currentTemp = 0.0f;
volatile float setpointTemp = DEFAULT_SETPOINT_F;
volatile bool heaterOn = false;
volatile bool sensorError = false;
volatile bool safetyShutdown = false;

// Temperature smoothing (moving average)
#define TEMP_HISTORY_SIZE 8
static float tempHistory[TEMP_HISTORY_SIZE];
static uint8_t tempHistoryIndex = 0;
static uint8_t tempHistoryCount = 0;

// Timing variables (only used in main loop, no need for volatile)
unsigned long lastTempRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastTouchTime = 0;
unsigned long lastRelayChange = 0;

// Button feedback state (non-blocking)
unsigned long buttonFeedbackStart = 0;
int activeButton = 0;  // 0=none, 1=UP, 2=DOWN
bool buttonFeedbackActive = false;

// Touch state for press-only detection (ignore hold)
static bool wasTouched = false;

// BLE notification tracking (to avoid unnecessary notifications)
float lastNotifiedTemp = -999.0f;
float lastNotifiedSetpoint = -999.0f;
bool lastNotifiedHeaterOn = false;
bool lastNotifiedSafetyShutdown = false;
bool lastNotifiedSensorError = false;

// BLE connection display tracking
bool lastDisplayedBleState = false;

// Flag to force display update from main loop
volatile bool forceDisplayUpdate = false;

// Global display buffer to avoid stack allocation
static char displayBuffer[32];

// ========================================
// Function Prototypes
// ========================================
void initI2C();
void initMCP23017();
void initDisplay();
void initBluetooth();
void updateBluetooth();
void calibrateTouch();
float readMAX6675();
float computeSmoothedTemp(float newReading);
uint16_t readMAX6675Raw();
void bitBangSPI(uint8_t& byte1, uint8_t& byte2);
void updateThermostat();
void updateDisplay();
void handleTouch();
void handleButtonFeedback();
void setRelay(bool state);
void tryResetSafetyShutdown();
void drawButton(int x, int y, int w, int h, const char* label, uint16_t color);
void drawUI();

// ========================================
// BLE Setpoint Write Callback
// ========================================
class SetpointCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pCharacteristic) {
        Serial.println(">>> SetpointCallbacks::onRead() called");
    }
    
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        Serial.println("================================");
        Serial.println("=== BLE WRITE CALLBACK FIRED ===");
        Serial.println("================================");
        
        if (value.length() > 0) {
            Serial.printf("Received value: '%s'\n", value.c_str());
            Serial.printf("Value length: %d bytes\n", value.length());
            
            // Parse the incoming value as float
            float newSetpoint = atof(value.c_str());
            Serial.printf("Parsed as float: %.1f°F\n", newSetpoint);
            Serial.printf("Current setpointTemp BEFORE: %.1f°F\n", setpointTemp);

            // Validate range
            if (newSetpoint >= MIN_SETPOINT_F && newSetpoint <= MAX_SETPOINT_F) {
                Serial.println("✓ Value is within valid range");
                
                setpointTemp = newSetpoint;
                Serial.printf("✓ setpointTemp UPDATED to: %.1f°F\n", setpointTemp);

                // Try to reset safety shutdown if conditions allow
                tryResetSafetyShutdown();

                // Send notification back to confirm
                char setpointStr[16];
                snprintf(setpointStr, sizeof(setpointStr), "%.1f", setpointTemp);
                pCharacteristic->setValue(setpointStr);
                pCharacteristic->notify();
                Serial.printf("✓ BLE notification sent: %s\n", setpointStr);

                // Flag for display update in main loop (don't call updateDisplay here!)
                forceDisplayUpdate = true;
                Serial.println("✓ Display update flagged");
            } else {
                Serial.println("✗ REJECTED - Value out of range!");
                Serial.printf("  Range: %.1f°F to %.1f°F\n", MIN_SETPOINT_F, MAX_SETPOINT_F);
                Serial.printf("  Received: %.1f°F\n", newSetpoint);
            }
            
            Serial.printf("Current setpointTemp AFTER: %.1f°F\n", setpointTemp);
        } else {
            Serial.println("✗ ERROR: Empty value received");
        }
        
        Serial.println("================================\n");
    }
    
    void onNotify(BLECharacteristic* pCharacteristic) {
        Serial.println(">>> SetpointCallbacks::onNotify() called");
    }
};

// Global callback instance
static SetpointCallbacks setpointCallbacks;

// ========================================
// Safety Shutdown Reset Helper
// ========================================
void tryResetSafetyShutdown() {
    // Cache volatile values for consistent decision
    float localTemp = currentTemp;
    bool localSensorError = sensorError;
    
    if (safetyShutdown && localTemp < SAFETY_MAX_TEMP_F && !localSensorError) {
        safetyShutdown = false;
        Serial.println("✓ Safety shutdown reset");
    }
}

// ========================================
// Setup
// ========================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\nSmart Oil Heater Controller");
    Serial.println("============================");

    // Enable backlight on GPIO27 (3.5" CYD backlight pin)
    Serial.println("Enabling backlight on GPIO27...");
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);
    Serial.println("Backlight enabled");
    delay(500);

    // Initialize I2C
    initI2C();

    // Initialize MCP23017
    initMCP23017();

    // Initialize Display
    initDisplay();

    // Initialize Touch (built-in TFT_eSPI touch support)
    Serial.println("Initializing touchscreen...");
    tft.setTouch(calData);
    Serial.println("Touch initialized with calibration data");
    Serial.println("Note: Send 'c' via serial to run calibration");

    // Initialize Bluetooth
    initBluetooth();

    // Turn off heater initially
    setRelay(false);

    // Draw initial UI
    drawUI();

    Serial.println("Initialization complete!");
    Serial.printf("Default setpoint: %.1f°F\n", setpointTemp);
}

// ========================================
// Main Loop
// ========================================
void loop() {
    unsigned long currentMillis = millis();

    // Check for serial commands
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            calibrateTouch();
            drawUI();  // Redraw UI after calibration
        }
    }

    // Read temperature periodically
    if (currentMillis - lastTempRead >= TEMP_READ_INTERVAL) {
        lastTempRead = currentMillis;
        
        // Read and smooth temperature
        float tempReading = readMAX6675();
        currentTemp = tempReading;

        // Cache all volatile values for consistent logic in this block
        float localTemp = currentTemp;
        float localSetpoint = setpointTemp;
        bool localHeaterOn = heaterOn;
        bool localSafetyShutdown = safetyShutdown;
        bool localSensorError = sensorError;

        // Check for sensor error
        if (localTemp >= SENSOR_ERROR_TEMP) {
            sensorError = true;
            safetyShutdown = true;
            setRelay(false);
            Serial.println("ERROR: Sensor error detected!");
            localSensorError = true;
            localSafetyShutdown = true;
        } else {
            sensorError = false;
            localSensorError = false;
        }

        // Check for over-temperature (use >= for safety-critical threshold)
        if (localTemp >= SAFETY_MAX_TEMP_F && !localSensorError) {
            safetyShutdown = true;
            setRelay(false);
            Serial.printf("ERROR: Over-temperature detected! Temp: %.1f°F\n", localTemp);
            localSafetyShutdown = true;
        }

        // Debug: Print system state
        Serial.println("--- System State ---");
        Serial.printf("Current Temp: %.1f°F\n", localTemp);
        Serial.printf("Setpoint: %.1f°F (Hysteresis ±%.1f°F)\n", localSetpoint, TEMP_HYSTERESIS_F);
        Serial.printf("Heater: %s\n", localHeaterOn ? "ON" : "OFF");
        Serial.printf("Safety Shutdown: %s\n", localSafetyShutdown ? "YES" : "NO");
        Serial.printf("Sensor Error: %s\n", localSensorError ? "YES" : "NO");
        Serial.printf("BLE Connected: %s\n", deviceConnected ? "YES" : "NO");
        unsigned long timeSinceRelayChange = (currentMillis - lastRelayChange) / 1000;
        Serial.printf("Time since last relay change: %lu sec (min: %d sec)\n",
                     timeSinceRelayChange, RELAY_MIN_CYCLE_TIME / 1000);

        if (!localHeaterOn) {
            float turnOnThreshold = localSetpoint - TEMP_HYSTERESIS_F;
            Serial.printf("Heater will turn ON when temp < %.1f°F\n", turnOnThreshold);
            if (localTemp >= turnOnThreshold) {
                Serial.printf("  -> Still %.1f°F above turn-on threshold\n",
                             localTemp - turnOnThreshold);
            }
        } else {
            float turnOffThreshold = localSetpoint + TEMP_HYSTERESIS_F;
            Serial.printf("Heater will turn OFF when temp > %.1f°F\n", turnOffThreshold);
        }
        Serial.println("-------------------");

        // Update thermostat logic if not in safety shutdown
        if (!safetyShutdown) {
            updateThermostat();
        } else {
            Serial.println("THERMOSTAT DISABLED: Safety shutdown active!");
            Serial.println("To reset: Adjust setpoint when temp < safety max and sensor OK");
        }
    }

    // Handle non-blocking button feedback
    handleButtonFeedback();

    // Update display and Bluetooth periodically (or when forced)
    if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL || forceDisplayUpdate) {
        lastDisplayUpdate = currentMillis;
        forceDisplayUpdate = false;
        updateDisplay();
        updateBluetooth();
    }

    // Handle touch input
    handleTouch();

    delay(10);
}

// ========================================
// I2C Initialization
// ========================================
void initI2C() {
    Serial.println("Initializing I2C...");
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);
    Serial.printf("I2C initialized on SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
}

// ========================================
// MCP23017 Initialization
// ========================================
void initMCP23017() {
    Serial.println("Initializing MCP23017...");

    if (!mcp.begin_I2C(MCP23017_ADDR)) {
        Serial.println("ERROR: MCP23017 not found!");
        while (1) delay(10);
    }

    // Configure pins
    mcp.pinMode(MCP_RELAY_PIN, OUTPUT);      // Relay control
    mcp.pinMode(MCP_MAX6675_SCK, OUTPUT);    // MAX6675 SCK
    mcp.pinMode(MCP_MAX6675_CS, OUTPUT);     // MAX6675 CS
    mcp.pinMode(MCP_MAX6675_SO, INPUT);      // MAX6675 SO (data)

    // Set initial states
    mcp.digitalWrite(MCP_RELAY_PIN, LOW);    // Relay off (active HIGH)
    mcp.digitalWrite(MCP_MAX6675_CS, HIGH);  // CS high (inactive)
    mcp.digitalWrite(MCP_MAX6675_SCK, LOW);  // SCK low

    Serial.println("MCP23017 initialized");
}

// ========================================
// Display Initialization
// ========================================
void initDisplay() {
    Serial.println("Initializing display...");
    tft.init();
    tft.setRotation(1);  // Landscape mode
    tft.fillScreen(COLOR_BG);
    Serial.println("Display initialized");
}

// ========================================
// Touch Calibration
// ========================================
void calibrateTouch() {
    Serial.println("\n=== TOUCH CALIBRATION ===");
    Serial.println("Touch the corners when prompted...");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch Screen Calibration", 240, 100);
    tft.drawString("Follow the prompts", 240, 140);
    delay(2000);

    tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);

    Serial.println("\n=== CALIBRATION COMPLETE ===");
    Serial.printf("Calibration data: {%d, %d, %d, %d, %d}\n",
                  calData[0], calData[1], calData[2], calData[3], calData[4]);
    Serial.println("Copy these values to calData array in code for permanent calibration");
    Serial.println("=============================\n");

    tft.setTouch(calData);
    delay(1000);
}

// ========================================
// Bluetooth Initialization
// ========================================
void initBluetooth() {
    Serial.println("Initializing Bluetooth...");

    // Create BLE Device
    BLEDevice::init("Heater_Controller");

    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);  // Use static instance

    // Create BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create Temperature Characteristic
    pTempCharacteristic = pService->createCharacteristic(
        TEMP_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTempCharacteristic->addDescriptor(new BLE2902());

    // Create Setpoint Characteristic (with WRITE capability)
    pSetpointCharacteristic = pService->createCharacteristic(
        SETPOINT_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pSetpointCharacteristic->addDescriptor(new BLE2902());
    pSetpointCharacteristic->setCallbacks(&setpointCallbacks);

    // Create Status Characteristic (JSON format)
    pStatusCharacteristic = pService->createCharacteristic(
        STATUS_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pStatusCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Min connection interval (7.5ms units)
    pAdvertising->setMaxPreferred(0x12);  // Max connection interval
    BLEDevice::startAdvertising();

    Serial.println("Bluetooth initialized");
    Serial.println("BLE Device name: Heater_Controller");
    Serial.println("Waiting for client connection...");
}

// ========================================
// Update Bluetooth Characteristics
// ========================================
void updateBluetooth() {
    if (!deviceConnected) {
        return;  // No client connected, skip update
    }

    // Cache volatile values for consistent comparison
    float localTemp = currentTemp;
    float localSetpoint = setpointTemp;
    bool localHeaterOn = heaterOn;
    bool localSafetyShutdown = safetyShutdown;
    bool localSensorError = sensorError;

    // Update Temperature Characteristic (only if changed, with 0.1°F threshold)
    if (fabsf(localTemp - lastNotifiedTemp) >= 0.1f) {
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.1f", localTemp);
        pTempCharacteristic->setValue(tempStr);
        pTempCharacteristic->notify();
        lastNotifiedTemp = localTemp;
    }

    // Update Setpoint Characteristic (only if changed)
    if (fabsf(localSetpoint - lastNotifiedSetpoint) >= 0.1f) {
        char setpointStr[16];
        snprintf(setpointStr, sizeof(setpointStr), "%.1f", localSetpoint);
        pSetpointCharacteristic->setValue(setpointStr);
        pSetpointCharacteristic->notify();
        lastNotifiedSetpoint = localSetpoint;
    }

    // Update Status Characteristic (only if any status changed)
    // Using boolean values for cleaner mobile app parsing
    if (localHeaterOn != lastNotifiedHeaterOn ||
        localSafetyShutdown != lastNotifiedSafetyShutdown ||
        localSensorError != lastNotifiedSensorError) {
        
        char statusStr[128];
        snprintf(statusStr, sizeof(statusStr),
                 "{\"heater\":%s,\"safetyShutdown\":%s,\"sensorError\":%s}",
                 localHeaterOn ? "true" : "false",
                 localSafetyShutdown ? "true" : "false",
                 localSensorError ? "true" : "false");

        pStatusCharacteristic->setValue(statusStr);
        pStatusCharacteristic->notify();
        
        lastNotifiedHeaterOn = localHeaterOn;
        lastNotifiedSafetyShutdown = localSafetyShutdown;
        lastNotifiedSensorError = localSensorError;
    }
}

// ========================================
// Compute Smoothed Temperature (Moving Average)
// ========================================
float computeSmoothedTemp(float newReading) {
    // Don't smooth error values
    if (newReading >= SENSOR_ERROR_TEMP) {
        return newReading;
    }
    
    // Add to history
    tempHistory[tempHistoryIndex] = newReading;
    tempHistoryIndex = (tempHistoryIndex + 1) % TEMP_HISTORY_SIZE;
    if (tempHistoryCount < TEMP_HISTORY_SIZE) {
        tempHistoryCount++;
    }
    
    // Compute average
    float sum = 0.0f;
    for (uint8_t i = 0; i < tempHistoryCount; i++) {
        sum += tempHistory[i];
    }
    
    return sum / tempHistoryCount;
}

// ========================================
// Read MAX6675 Temperature
// ========================================
float readMAX6675() {
    uint16_t raw = readMAX6675Raw();

    // Check for error (D2 bit set)
    if (raw & 0x04) {
        Serial.println("MAX6675 Error: Thermocouple not connected");
        return SENSOR_ERROR_TEMP;
    }

    // Check for invalid reading
    if (raw == MAX6675_ERROR_VALUE) {
        return SENSOR_ERROR_TEMP;
    }

    // Extract temperature (bits D15-D3, right-shift by 3)
    uint16_t tempValue = raw >> 3;

    // Convert to Celsius (0.25°C per count)
    float tempC = tempValue * 0.25f;

    // Convert to Fahrenheit
    float tempF = (tempC * 9.0f / 5.0f) + 32.0f;

    // Apply moving average smoothing
    float smoothedTempF = computeSmoothedTemp(tempF);

    Serial.printf("Temp: %.2f°F (raw: %.2f°F, %.2f°C) [Raw: 0x%04X]\n", 
                  smoothedTempF, tempF, tempC, raw);

    return smoothedTempF;
}

// ========================================
// Read MAX6675 Raw Value
// ========================================
uint16_t readMAX6675Raw() {
    uint8_t byte1 = 0, byte2 = 0;

    // Pull CS low to start conversion
    mcp.digitalWrite(MCP_MAX6675_CS, LOW);
    delayMicroseconds(10);

    // Read 16 bits via bit-bang SPI
    bitBangSPI(byte1, byte2);

    // Pull CS high to end conversion
    mcp.digitalWrite(MCP_MAX6675_CS, HIGH);

    // Combine bytes (MSB first)
    uint16_t raw = ((uint16_t)byte1 << 8) | byte2;

    return raw;
}

// ========================================
// Bit-Bang SPI Read
// ========================================
void bitBangSPI(uint8_t& byte1, uint8_t& byte2) {
    byte1 = 0;
    byte2 = 0;

    // Read first byte (bits 15-8)
    for (int i = 7; i >= 0; i--) {
        mcp.digitalWrite(MCP_MAX6675_SCK, HIGH);
        delayMicroseconds(1);

        if (mcp.digitalRead(MCP_MAX6675_SO)) {
            byte1 |= (1 << i);
        }

        mcp.digitalWrite(MCP_MAX6675_SCK, LOW);
        delayMicroseconds(1);
    }

    // Read second byte (bits 7-0)
    for (int i = 7; i >= 0; i--) {
        mcp.digitalWrite(MCP_MAX6675_SCK, HIGH);
        delayMicroseconds(1);

        if (mcp.digitalRead(MCP_MAX6675_SO)) {
            byte2 |= (1 << i);
        }

        mcp.digitalWrite(MCP_MAX6675_SCK, LOW);
        delayMicroseconds(1);
    }
}

// ========================================
// Update Thermostat Logic
// ========================================
void updateThermostat() {
    unsigned long currentMillis = millis();

    // Prevent rapid relay cycling
    if (currentMillis - lastRelayChange < RELAY_MIN_CYCLE_TIME) {
        unsigned long timeRemaining = (RELAY_MIN_CYCLE_TIME - (currentMillis - lastRelayChange)) / 1000;
        Serial.printf("Relay cycling lockout: %lu seconds remaining\n", timeRemaining);
        return;
    }

    // Cache volatile values for consistent logic within this function
    float localTemp = currentTemp;
    float localSetpoint = setpointTemp;
    bool localHeaterOn = heaterOn;

    // Thermostat logic with hysteresis
    if (!localHeaterOn) {
        // Turn on if temp falls below (setpoint - hysteresis)
        float turnOnThreshold = localSetpoint - TEMP_HYSTERESIS_F;
        if (localTemp < turnOnThreshold) {
            setRelay(true);
            lastRelayChange = currentMillis;
            Serial.printf(">>> Heater ON: Temp %.1f°F < Threshold %.1f°F <<<\n",
                         localTemp, turnOnThreshold);
        } else {
            Serial.printf("Heater OFF: Waiting for temp to drop below %.1f°F (currently %.1f°F)\n",
                         turnOnThreshold, localTemp);
        }
    } else {
        // Turn off if temp rises above (setpoint + hysteresis)
        float turnOffThreshold = localSetpoint + TEMP_HYSTERESIS_F;
        if (localTemp > turnOffThreshold) {
            setRelay(false);
            lastRelayChange = currentMillis;
            Serial.printf(">>> Heater OFF: Temp %.1f°F > Threshold %.1f°F <<<\n",
                         localTemp, turnOffThreshold);
        } else {
            Serial.printf("Heater ON: Waiting for temp to rise above %.1f°F (currently %.1f°F)\n",
                         turnOffThreshold, localTemp);
        }
    }
}

// ========================================
// Update Display
// ========================================
void updateDisplay() {
    // Cache volatile values
    float localTemp = currentTemp;
    float localSetpoint = setpointTemp;
    bool localHeaterOn = heaterOn;
    bool localSensorError = sensorError;
    bool localSafetyShutdown = safetyShutdown;
    bool localBleConnected = deviceConnected;

    // Temperature display
    tft.fillRect(0, 75, 480, 60, COLOR_BG);
    tft.setTextColor(localSensorError ? COLOR_TEMP_HIGH : COLOR_TEMP_NORMAL, COLOR_BG);
    tft.setTextSize(3);
    tft.setTextDatum(MC_DATUM);

    if (localSensorError) {
        tft.drawString("ERROR", 240, 100);
    } else {
        snprintf(displayBuffer, sizeof(displayBuffer), "%.1f F", localTemp);
        tft.drawString(displayBuffer, 240, 100);
    }

    // Setpoint display
    tft.fillRect(0, 130, 480, 40, COLOR_BG);
    tft.setTextColor(COLOR_SETPOINT, COLOR_BG);
    tft.setTextSize(2);
    snprintf(displayBuffer, sizeof(displayBuffer), "Set: %.1f F", localSetpoint);
    tft.drawString(displayBuffer, 240, 145);

    // Heater status
    tft.fillRect(0, 165, 480, 30, COLOR_BG);
    tft.setTextColor(localHeaterOn ? COLOR_HEATER_ON : COLOR_HEATER_OFF, COLOR_BG);
    tft.setTextSize(2);

    if (localSafetyShutdown) {
        tft.setTextColor(COLOR_TEMP_HIGH, COLOR_BG);
        tft.drawString("SAFETY SHUTDOWN!", 240, 180);
    } else {
        tft.drawString(localHeaterOn ? "HEATER ON" : "HEATER OFF", 240, 180);
    }

    // BLE connection status (only update if changed to reduce flicker)
    if (localBleConnected != lastDisplayedBleState) {
        tft.fillRect(0, 295, 480, 25, COLOR_BG);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        
        if (localBleConnected) {
            tft.setTextColor(TFT_GREEN, COLOR_BG);
            tft.drawString("BLE: CONNECTED", 240, 305);
        } else {
            tft.setTextColor(TFT_DARKGREY, COLOR_BG);
            tft.drawString("BLE: DISCONNECTED", 240, 305);
        }
        
        lastDisplayedBleState = localBleConnected;
    }
}

// ========================================
// Draw Initial UI
// ========================================
void drawUI() {
    tft.fillScreen(COLOR_BG);

    // Title - split into two lines with color accent
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);

    // Top line with yellow accent
    tft.setTextColor(TFT_YELLOW, COLOR_BG);
    tft.drawString("CREWCHIEFSTEVE", 240, 15);

    // Bottom line same size
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.drawString("TECHNOLOGIES", 240, 40);

    // Decorative line under title
    tft.drawFastHLine(40, 65, 400, TFT_YELLOW);

    // Draw buttons
    drawButton(BUTTON_UP_X, BUTTON_UP_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "UP", COLOR_BUTTON);
    drawButton(BUTTON_DOWN_X, BUTTON_DOWN_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "DOWN", COLOR_BUTTON);

    // Force BLE status to redraw
    lastDisplayedBleState = !deviceConnected;

    // Update display content
    updateDisplay();
}

// ========================================
// Draw Button
// ========================================
void drawButton(int x, int y, int w, int h, const char* label, uint16_t color) {
    tft.fillRoundRect(x, y, w, h, 8, color);
    tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_BUTTON_TEXT, color);
    tft.setTextSize(2);
    tft.drawString(label, x + w/2, y + h/2);
}

// ========================================
// Handle Non-Blocking Button Feedback
// ========================================
void handleButtonFeedback() {
    if (!buttonFeedbackActive) {
        return;
    }

    unsigned long currentMillis = millis();
    
    // Check if feedback duration has elapsed (100ms for green flash)
    if (currentMillis - buttonFeedbackStart >= 100) {
        // Restore button to normal color
        if (activeButton == 1) {
            drawButton(BUTTON_UP_X, BUTTON_UP_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "UP", COLOR_BUTTON);
        } else if (activeButton == 2) {
            drawButton(BUTTON_DOWN_X, BUTTON_DOWN_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "DOWN", COLOR_BUTTON);
        }
        
        buttonFeedbackActive = false;
        activeButton = 0;
    }
}

// ========================================
// Handle Touch Input
// ========================================
void handleTouch() {
    uint16_t touchX = 0, touchY = 0;

    // Check if screen is being touched (TFT_eSPI built-in touch)
    bool touched = tft.getTouch(&touchX, &touchY, 300);  // 300 = pressure threshold

    // If not touched, reset touch state and return
    if (!touched) {
        wasTouched = false;
        return;
    }

    // Only trigger on new press (ignore hold)
    if (wasTouched) {
        return;
    }
    wasTouched = true;

    unsigned long currentMillis = millis();

    // Debounce (still useful for very fast repeated taps)
    if (currentMillis - lastTouchTime < TOUCH_DEBOUNCE_MS) {
        return;
    }

    // Cache volatile values
    float localSetpoint = setpointTemp;

    // Check if UP button pressed
    if (touchX >= BUTTON_UP_X && touchX <= (BUTTON_UP_X + BUTTON_WIDTH) &&
        touchY >= BUTTON_UP_Y && touchY <= (BUTTON_UP_Y + BUTTON_HEIGHT)) {

        if (localSetpoint < MAX_SETPOINT_F) {
            setpointTemp = localSetpoint + SETPOINT_INCREMENT;
            Serial.printf("UP button pressed! Setpoint increased to %.1f°F\n", setpointTemp);

            // Start non-blocking button feedback
            drawButton(BUTTON_UP_X, BUTTON_UP_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "UP", TFT_GREEN);
            buttonFeedbackStart = currentMillis;
            buttonFeedbackActive = true;
            activeButton = 1;

            // Force immediate display update
            forceDisplayUpdate = true;

            // Try to reset safety shutdown if conditions allow
            tryResetSafetyShutdown();
        }

        lastTouchTime = currentMillis;
    }

    // Check if DOWN button pressed
    else if (touchX >= BUTTON_DOWN_X && touchX <= (BUTTON_DOWN_X + BUTTON_WIDTH) &&
             touchY >= BUTTON_DOWN_Y && touchY <= (BUTTON_DOWN_Y + BUTTON_HEIGHT)) {

        if (localSetpoint > MIN_SETPOINT_F) {
            setpointTemp = localSetpoint - SETPOINT_INCREMENT;
            Serial.printf("DOWN button pressed! Setpoint decreased to %.1f°F\n", setpointTemp);

            // Start non-blocking button feedback
            drawButton(BUTTON_DOWN_X, BUTTON_DOWN_Y, BUTTON_WIDTH, BUTTON_HEIGHT, "DOWN", TFT_GREEN);
            buttonFeedbackStart = currentMillis;
            buttonFeedbackActive = true;
            activeButton = 2;

            // Force immediate display update
            forceDisplayUpdate = true;

            // Try to reset safety shutdown if conditions allow
            tryResetSafetyShutdown();
        }

        lastTouchTime = currentMillis;
    }
}

// ========================================
// Set Relay State
// ========================================
void setRelay(bool state) {
    heaterOn = state;
    // Relay is active HIGH (ON=HIGH, OFF=LOW)
    mcp.digitalWrite(MCP_RELAY_PIN, state ? HIGH : LOW);
    Serial.printf("Relay set to %s\n", state ? "ON" : "OFF");
}

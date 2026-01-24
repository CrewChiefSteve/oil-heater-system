/*
 * Smart Oil Heater Controller - Control Board
 *
 * Hardware: ESP32 DevKit v1
 * - MAX6675 K-type thermocouple (SPI)
 * - Relay module (active HIGH assumed, change RELAY_ACTIVE_HIGH if needed)
 * - UART serial communication with UI board (Serial2 on GPIO 16/17)
 *
 * Safety Features:
 * - Watchdog: Heater OFF if no UI command in 5 seconds
 * - Overtemp cutoff: Hard shutdown at MAX_SAFE_TEMP_C
 * - Sensor fault detection: Heater OFF if thermocouple disconnected
 */

#include <Arduino.h>
#include <max6675.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================================
// HARDWARE CONFIGURATION - ADJUST TO YOUR WIRING
// ============================================================================

// MAX6675 SPI pins (bit-bang, so any GPIO works)
static constexpr int PIN_THERMO_SCK = 18;  // Clock
static constexpr int PIN_THERMO_CS  = 5;   // Chip Select
static constexpr int PIN_THERMO_SO  = 19;  // Data Out (MISO)

// Relay control
static constexpr int PIN_RELAY = 23;
static constexpr bool RELAY_ACTIVE_HIGH = true;  // Set false if relay is active-LOW

// UART for display communication
#define UI_SERIAL Serial2
static constexpr int UI_RX_PIN = 16;
static constexpr int UI_TX_PIN = 17;
static constexpr int UI_BAUD = 115200;

// ============================================================================
// CONTROL PARAMETERS
// ============================================================================

static constexpr float DEFAULT_SETPOINT_C = 110.0f;   // ~230°F
static constexpr float HYSTERESIS_C       = 2.0f;     // Bang-bang deadband
static constexpr float MAX_SAFE_TEMP_C    = 160.0f;   // Hard cutoff (~320°F)
static constexpr float MIN_SETPOINT_C     = 50.0f;    // Minimum allowed setpoint
static constexpr float MAX_SETPOINT_C     = 150.0f;   // Maximum allowed setpoint

static constexpr uint32_t CMD_TIMEOUT_MS  = 5000;     // Safety watchdog timeout
static constexpr uint32_t TEMP_READ_MS    = 250;      // MAX6675 read interval
static constexpr uint32_t STATUS_SEND_MS  = 250;      // Status broadcast interval
static constexpr uint32_t BLE_UPDATE_MS   = 500;      // BLE notification interval

// ============================================================================
// BLE CONFIGURATION
// ============================================================================

#define BLE_DEVICE_NAME "Heater_Controller"
// Service UUID - MUST match SERVICE_UUIDS.OIL_HEATER in @crewchiefsteve/ble package
// See packages/ble/src/constants/uuids.ts for all device UUIDs
#define BLE_SERVICE_UUID        "4fafc201-0001-459e-8fcc-c5c9c331914b"
#define BLE_CHAR_TEMP_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Read/Notify
#define BLE_CHAR_SETPOINT_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Read/Write
#define BLE_CHAR_STATUS_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // Read/Notify
#define BLE_CHAR_ENABLE_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // Read/Write

// ============================================================================
// THERMOCOUPLE CALIBRATION CONFIGURATION
// ============================================================================

/**
 * CALIBRATION MODES:
 *
 * CAL_NONE      - No calibration, use raw MAX6675 readings
 *
 * CAL_SINGLE    - Single-point offset calibration
 *                 Use when: You have a reference thermometer at operating temp
 *                 Just corrects offset, assumes scale is accurate
 *                 Easiest method - compare readings, enter difference
 *
 * CAL_TWO_POINT - Two-point calibration (ice bath + boiling water)
 *                 Use when: You need accuracy across full temperature range
 *                 Corrects both offset and scale
 *                 More work but more accurate
 */

enum CalibrationMode {
    CAL_NONE,
    CAL_SINGLE,
    CAL_TWO_POINT
};

// >>> SELECT YOUR CALIBRATION MODE HERE <<<
static constexpr CalibrationMode CAL_MODE = CAL_SINGLE;

// Set to true when performing calibration tests to see raw vs calibrated values
#define CAL_DEBUG_RAW false

// ----------------------------------------------------------------------------
// SINGLE-POINT CALIBRATION (CAL_SINGLE)
// ----------------------------------------------------------------------------
// Compare MAX6675 reading to a reference thermometer at any temperature.
// CAL_SINGLE_OFFSET = reference_temp - raw_reading
//
// Example: Reference reads 200°F (93.3°C), MAX6675 reads 95.0°C
//          CAL_SINGLE_OFFSET = 93.3 - 95.0 = -1.7°C

static constexpr float CAL_SINGLE_OFFSET_C = 4.5f;  // Adjusted: was 6.2 (calculated in F by mistake)

// ----------------------------------------------------------------------------
// TWO-POINT CALIBRATION (CAL_TWO_POINT)
// ----------------------------------------------------------------------------
// Step 1: Ice bath test - record raw reading (should be ~0°C)
// Step 2: Boiling water test - record raw reading (should be ~100°C)
// Step 3: Enter your readings below
//
// Example: Ice bath reads 1.2°C, Boiling water reads 98.5°C

static constexpr float CAL_RAW_ICE_C  = 0.0f;    // Your ice bath reading
static constexpr float CAL_RAW_BOIL_C = 100.0f;  // Your boiling water reading

// Reference points (adjust boiling for altitude if needed)
static constexpr float CAL_REF_ICE_C  = 0.0f;    // Ice point (always 0°C)
static constexpr float CAL_REF_BOIL_C = 100.0f;  // Boiling point (~99°C at altitude)

// ============================================================================
// UART PROTOCOL (same packets as ESP-NOW version)
// ============================================================================

// Magic numbers for packet validation
static constexpr uint32_t MAGIC_UI2CTRL = 0x55494331;  // "UIC1"
static constexpr uint32_t MAGIC_CTRL2UI = 0x43554931;  // "CUI1"

// Fault codes
enum FaultCode : uint8_t {
    FAULT_NONE         = 0,
    FAULT_SENSOR_OPEN  = 1,
    FAULT_OVERTEMP     = 2,
    FAULT_COMM_TIMEOUT = 3,
};

#pragma pack(push, 1)
// Command from UI to Controller
struct UiToCtrlPacket {
    uint32_t magic;           // Must be MAGIC_UI2CTRL
    uint16_t setpoint_c_x10;  // Setpoint in °C × 10
    uint8_t  enable;          // 0 = disabled, 1 = enabled
    uint8_t  reserved;
    uint32_t seq;             // Sequence number
};

// Status from Controller to UI
struct CtrlToUiPacket {
    uint32_t magic;           // MAGIC_CTRL2UI
    int16_t  temp_c_x10;      // Current temp in °C × 10 (INT16_MIN on fault)
    uint16_t setpoint_c_x10;  // Current setpoint × 10
    uint8_t  relay_on;        // 0 or 1
    uint8_t  fault_code;      // FaultCode enum
    uint32_t uptime_s;        // Uptime in seconds
    uint32_t seq_echo;        // Last received command seq
};
#pragma pack(pop)

// ============================================================================
// GLOBALS
// ============================================================================

MAX6675 thermocouple(PIN_THERMO_SCK, PIN_THERMO_CS, PIN_THERMO_SO);

static float g_setpointC     = DEFAULT_SETPOINT_C;
static bool  g_heaterEnabled = false;
static bool  g_relayState    = false;
static uint32_t g_lastCmdMs  = 0;
static uint32_t g_lastCmdSeq = 0;

// UI connection tracking
static bool g_uiConnected = false;

// Timing
static uint32_t g_lastTempReadMs   = 0;
static uint32_t g_lastStatusSendMs = 0;

// Current readings
static float    g_currentTempC = NAN;
static uint8_t  g_currentFault = FAULT_NONE;

// Temperature smoothing - moving average filter
static constexpr int NUM_SAMPLES = 15;  // Increased from 5 for more aggressive smoothing
static float tempReadings[NUM_SAMPLES] = {0};
static int readingIndex = 0;
static float tempSum = 0.0f;
static bool arrayFilled = false;

// BLE globals
static BLEServer* g_bleServer = nullptr;
static BLECharacteristic* g_charTemp = nullptr;
static BLECharacteristic* g_charSetpoint = nullptr;
static BLECharacteristic* g_charStatus = nullptr;
static BLECharacteristic* g_charEnable = nullptr;
static bool g_bleClientConnected = false;
static uint32_t g_lastBleUpdateMs = 0;

// ============================================================================
// BLE CALLBACK CLASSES
// ============================================================================

// Server callbacks - track connection state
class BleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        g_bleClientConnected = true;
        Serial.println("[BLE] Client connected");
    }

    void onDisconnect(BLEServer* pServer) {
        g_bleClientConnected = false;
        Serial.println("[BLE] Client disconnected - restarting advertising");
        pServer->startAdvertising();
    }
};

// Setpoint characteristic callback - receives setpoint in Fahrenheit as string
class SetpointCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            // Parse Fahrenheit string
            float setpointF = atof(value.c_str());
            // Convert F to C
            float setpointC = (setpointF - 32.0f) * 5.0f / 9.0f;
            // Constrain and store
            g_setpointC = constrain(setpointC, MIN_SETPOINT_C, MAX_SETPOINT_C);
            // Reset watchdog timer (CRITICAL SAFETY)
            g_lastCmdMs = millis();

            Serial.printf("[BLE] Setpoint write: %.1fF (%.1fC)\n", setpointF, g_setpointC);
        }
    }
};

// Enable characteristic callback - receives "1" or "0"
class EnableCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            g_heaterEnabled = (value[0] == '1');
            // Reset watchdog timer (CRITICAL SAFETY)
            g_lastCmdMs = millis();

            Serial.printf("[BLE] Enable write: %d\n", g_heaterEnabled);
        }
    }
};

// ============================================================================
// RELAY CONTROL
// ============================================================================

void setRelay(bool on) {
    g_relayState = on;
    if (RELAY_ACTIVE_HIGH) {
        digitalWrite(PIN_RELAY, on ? HIGH : LOW);
    } else {
        digitalWrite(PIN_RELAY, on ? LOW : HIGH);
    }
}

// ============================================================================
// THERMOCOUPLE CALIBRATION
// ============================================================================

/**
 * Apply calibration to raw thermocouple reading based on selected mode
 * @param rawTempC Raw temperature from MAX6675 in °C
 * @return Calibrated temperature in °C
 */
float calibrateTemperature(float rawTempC) {
    switch (CAL_MODE) {
        case CAL_NONE:
            // No calibration - return raw reading
            return rawTempC;

        case CAL_SINGLE:
            // Single-point: just add offset
            return rawTempC + CAL_SINGLE_OFFSET_C;

        case CAL_TWO_POINT: {
            // Two-point: apply scale and offset
            // Formula: calibrated = (raw - raw_ice) * scale + ref_ice
            float scale = (CAL_REF_BOIL_C - CAL_REF_ICE_C) /
                          (CAL_RAW_BOIL_C - CAL_RAW_ICE_C);
            return (rawTempC - CAL_RAW_ICE_C) * scale + CAL_REF_ICE_C;
        }

        default:
            return rawTempC;
    }
}

/**
 * Print calibration information at startup
 */
void printCalibrationInfo() {
    Serial.println("[CAL] Thermocouple calibration:");

    switch (CAL_MODE) {
        case CAL_NONE:
            Serial.println("      Mode: NONE (raw readings)");
            Serial.println("      Status: UNCALIBRATED");
            break;

        case CAL_SINGLE:
            Serial.println("      Mode: SINGLE-POINT");
            Serial.printf("      Offset: %+.2f C\n", CAL_SINGLE_OFFSET_C);
            if (CAL_SINGLE_OFFSET_C == 0.0f) {
                Serial.println("      Status: NOT CONFIGURED");
            } else {
                Serial.println("      Status: CALIBRATED");
            }
            break;

        case CAL_TWO_POINT: {
            Serial.println("      Mode: TWO-POINT");
            Serial.printf("      Ice reading:  %.2f C (ref: %.2f C)\n",
                          CAL_RAW_ICE_C, CAL_REF_ICE_C);
            Serial.printf("      Boil reading: %.2f C (ref: %.2f C)\n",
                          CAL_RAW_BOIL_C, CAL_REF_BOIL_C);

            float scale = (CAL_REF_BOIL_C - CAL_REF_ICE_C) /
                          (CAL_RAW_BOIL_C - CAL_RAW_ICE_C);
            float offset = CAL_REF_ICE_C - (CAL_RAW_ICE_C * scale);
            Serial.printf("      Calculated scale:  %.4f\n", scale);
            Serial.printf("      Calculated offset: %+.2f C\n", offset);

            if (CAL_RAW_ICE_C == 0.0f && CAL_RAW_BOIL_C == 100.0f) {
                Serial.println("      Status: NOT CONFIGURED");
            } else {
                Serial.println("      Status: CALIBRATED");
            }
            break;
        }
    }
    Serial.println();
}

// ============================================================================
// UART RECEIVE PROCESSING
// ============================================================================

void processDisplaySerial() {
    static uint8_t rxBuffer[32];
    static int rxIndex = 0;

    while (UI_SERIAL.available()) {
        uint8_t b = UI_SERIAL.read();
        rxBuffer[rxIndex++] = b;

        if (rxIndex >= sizeof(rxBuffer)) {
            rxIndex = 0;  // Overflow protection
        }

        // Check for complete packet
        if (rxIndex >= sizeof(UiToCtrlPacket)) {
            uint32_t magic;
            memcpy(&magic, rxBuffer, sizeof(magic));

            if (magic == MAGIC_UI2CTRL) {
                UiToCtrlPacket pkt;
                memcpy(&pkt, rxBuffer, sizeof(pkt));

                // Update last command time (for watchdog)
                g_lastCmdMs = millis();
                g_lastCmdSeq = pkt.seq;

                // Mark UI as connected on first valid packet
                if (!g_uiConnected) {
                    g_uiConnected = true;
                    Serial.println("[OK] Display connected via UART!");
                }

                // Apply setpoint (convert from Cx10 to C)
                g_setpointC = constrain(pkt.setpoint_c_x10 / 10.0f, MIN_SETPOINT_C, MAX_SETPOINT_C);

                // Apply enable state
                g_heaterEnabled = (pkt.enable != 0);

                Serial.printf("RX: Set=%.1fC En=%d Seq=%lu\n",
                              g_setpointC, g_heaterEnabled, pkt.seq);

                rxIndex = 0;  // Clear buffer
            } else {
                // Resync - shift buffer by 1 byte
                memmove(rxBuffer, rxBuffer + 1, rxIndex - 1);
                rxIndex--;
            }
        }
    }
}

// ============================================================================
// SEND STATUS TO DISPLAY
// ============================================================================

void sendStatusToDisplay() {
    CtrlToUiPacket pkt = {};
    pkt.magic = MAGIC_CTRL2UI;

    if (isnan(g_currentTempC)) {
        pkt.temp_c_x10 = INT16_MIN;
    } else {
        pkt.temp_c_x10 = (int16_t)lroundf(g_currentTempC * 10.0f);
    }

    pkt.setpoint_c_x10 = (uint16_t)lroundf(g_setpointC * 10.0f);
    pkt.relay_on       = g_relayState ? 1 : 0;
    pkt.fault_code     = g_currentFault;
    pkt.uptime_s       = millis() / 1000;
    pkt.seq_echo       = g_lastCmdSeq;

    UI_SERIAL.write((uint8_t*)&pkt, sizeof(pkt));
}

// ============================================================================
// TEMPERATURE SMOOTHING
// ============================================================================

float getSmoothedTemperature() {
    // Read raw temperature from MAX6675
    float rawTemp = thermocouple.readCelsius();

    // If sensor error, return NAN immediately (no smoothing)
    if (isnan(rawTemp)) {
        return NAN;
    }

    // Apply calibration to raw reading
    float calibratedTemp = calibrateTemperature(rawTemp);

    // Optional debug output for calibration testing
    #if CAL_DEBUG_RAW
        Serial.printf("[CAL] Raw: %.2f C -> Calibrated: %.2f C\n",
                      rawTemp, calibratedTemp);
    #endif

    // Update circular buffer for moving average (using calibrated value)
    tempSum -= tempReadings[readingIndex];          // Subtract oldest reading
    tempReadings[readingIndex] = calibratedTemp;    // Add new calibrated reading
    tempSum += calibratedTemp;                      // Update sum

    // Move to next position in circular buffer
    readingIndex = (readingIndex + 1) % NUM_SAMPLES;

    // Mark array as filled after first complete cycle
    if (readingIndex == 0 && !arrayFilled) {
        arrayFilled = true;
    }

    // Calculate and return average
    if (arrayFilled) {
        return tempSum / NUM_SAMPLES;
    } else {
        // Until buffer is full, average only the readings we have
        int validSamples = readingIndex > 0 ? readingIndex : NUM_SAMPLES;
        return tempSum / validSamples;
    }
}

// ============================================================================
// BLE SERVER INITIALIZATION
// ============================================================================

void initBLE() {
    Serial.println("[BLE] Initializing BLE server...");

    // Initialize BLE device
    BLEDevice::init(BLE_DEVICE_NAME);

    // Create BLE server with callbacks
    g_bleServer = BLEDevice::createServer();
    g_bleServer->setCallbacks(new BleServerCallbacks());

    // Create service
    BLEService* pService = g_bleServer->createService(BLE_SERVICE_UUID);

    // Temperature characteristic (Read + Notify)
    g_charTemp = pService->createCharacteristic(
        BLE_CHAR_TEMP_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    g_charTemp->addDescriptor(new BLE2902());

    // Setpoint characteristic (Read + Write)
    g_charSetpoint = pService->createCharacteristic(
        BLE_CHAR_SETPOINT_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    g_charSetpoint->setCallbacks(new SetpointCallbacks());

    // Status characteristic (Read + Notify)
    g_charStatus = pService->createCharacteristic(
        BLE_CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    g_charStatus->addDescriptor(new BLE2902());

    // Enable characteristic (Read + Write)
    g_charEnable = pService->createCharacteristic(
        BLE_CHAR_ENABLE_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    g_charEnable->setCallbacks(new EnableCallbacks());

    // Start service
    pService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // Helps with iPhone connection
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[OK] BLE server started - advertising as: " BLE_DEVICE_NAME);
}

// ============================================================================
// BLE CHARACTERISTIC UPDATES
// ============================================================================

void updateBLECharacteristics() {
    if (!g_bleClientConnected) {
        return;  // No client connected, skip updates
    }

    // Temperature characteristic - send current temp in Fahrenheit as string
    char tempStr[16];
    if (isnan(g_currentTempC)) {
        strcpy(tempStr, "ERR");
    } else {
        float tempF = (g_currentTempC * 9.0f / 5.0f) + 32.0f;
        snprintf(tempStr, sizeof(tempStr), "%.1f", tempF);
    }
    g_charTemp->setValue(tempStr);
    g_charTemp->notify();

    // Setpoint characteristic - send setpoint in Fahrenheit as string
    char setpointStr[16];
    float setpointF = (g_setpointC * 9.0f / 5.0f) + 32.0f;
    snprintf(setpointStr, sizeof(setpointStr), "%.1f", setpointF);
    g_charSetpoint->setValue(setpointStr);

    // Status characteristic - send JSON string
    char statusJson[128];
    snprintf(statusJson, sizeof(statusJson),
             "{\"heating\":%s,\"fault\":%d,\"enabled\":%s}",
             g_relayState ? "true" : "false",
             g_currentFault,
             g_heaterEnabled ? "true" : "false");
    g_charStatus->setValue(statusJson);
    g_charStatus->notify();

    // Enable characteristic - send current state as "1" or "0"
    g_charEnable->setValue(g_heaterEnabled ? "1" : "0");
}

// ============================================================================
// THERMOSTAT LOGIC
// ============================================================================

void updateThermostat() {
    // Read smoothed temperature
    float tempC = getSmoothedTemperature();
    g_currentTempC = tempC;
    
    // Determine fault state
    g_currentFault = FAULT_NONE;
    
    // Check comm timeout
    if ((millis() - g_lastCmdMs) > CMD_TIMEOUT_MS) {
        g_currentFault = FAULT_COMM_TIMEOUT;
        g_heaterEnabled = false;
    }
    
    // Check sensor
    if (isnan(tempC)) {
        g_currentFault = FAULT_SENSOR_OPEN;
        g_heaterEnabled = false;
    }
    
    // Check overtemp
    if (!isnan(tempC) && tempC >= MAX_SAFE_TEMP_C) {
        g_currentFault = FAULT_OVERTEMP;
        g_heaterEnabled = false;
    }
    
    // Bang-bang thermostat with hysteresis
    bool wantRelayOn = false;
    
    if (g_currentFault == FAULT_NONE && g_heaterEnabled && !isnan(tempC)) {
        float lowThresh  = g_setpointC - HYSTERESIS_C;
        float highThresh = g_setpointC + HYSTERESIS_C;
        
        if (tempC <= lowThresh) {
            wantRelayOn = true;
        } else if (tempC >= highThresh) {
            wantRelayOn = false;
        } else {
            // In deadband - maintain current state
            wantRelayOn = g_relayState;
        }
    }
    
    setRelay(wantRelayOn);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Smart Oil Heater - Controller Board");
    Serial.println("========================================");
    Serial.println();
    
    // Initialize relay (OFF)
    pinMode(PIN_RELAY, OUTPUT);
    setRelay(false);
    Serial.println("[OK] Relay initialized (OFF)");

    // Initialize BLE server
    initBLE();

    // Initialize UART to display
    UI_SERIAL.begin(UI_BAUD, SERIAL_8N1, UI_RX_PIN, UI_TX_PIN);
    Serial.println("[OK] Display UART initialized");
    Serial.printf("     RX: GPIO %d, TX: GPIO %d, Baud: %d\n", UI_RX_PIN, UI_TX_PIN, UI_BAUD);

    // Print calibration configuration
    Serial.println();
    printCalibrationInfo();

    // Initialize timing
    g_lastCmdMs = millis();
    g_lastTempReadMs = millis();
    g_lastStatusSendMs = millis();
    g_lastBleUpdateMs = millis();

    Serial.println("Waiting for display connection...");
    Serial.println();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t now = millis();

    // Process incoming commands from display
    processDisplaySerial();

    // Read temperature and update thermostat
    if ((now - g_lastTempReadMs) >= TEMP_READ_MS) {
        g_lastTempReadMs = now;
        updateThermostat();
    }

    // Send status to display at regular intervals
    if ((now - g_lastStatusSendMs) >= STATUS_SEND_MS) {
        g_lastStatusSendMs = now;
        sendStatusToDisplay();

        // Serial debug output
        Serial.printf("T=%.1fC  Set=%.1fC  En=%d  Relay=%s  Fault=%d\n",
                      isnan(g_currentTempC) ? -999.0f : g_currentTempC,
                      g_setpointC,
                      g_heaterEnabled,
                      g_relayState ? "ON " : "OFF",
                      g_currentFault);
    }

    // Update BLE characteristics at 500ms intervals
    if ((now - g_lastBleUpdateMs) >= BLE_UPDATE_MS) {
        g_lastBleUpdateMs = now;
        updateBLECharacteristics();
    }

    delay(10);
}

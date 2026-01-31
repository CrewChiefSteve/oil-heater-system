#include <Arduino.h>
#include "config.h"
#include "pins.h"
#include "types.h"
#include "probes.h"
#include "ble_service.h"
#include "display.h"
#include "led.h"
#include "power.h"
#include <Preferences.h>

// State
static DeviceState currentState = STATE_INITIALIZING;
static SessionData session;
static SystemStatus systemStatus;

// Timing
static uint32_t stateEntryTime = 0;
static uint32_t lastProbeRead = 0;
static uint32_t lastStatusTx = 0;

// v2: Corner ID management (NVS persistence)
static uint8_t cornerID = DEFAULT_CORNER_ID;  // 0=LF, 1=RF, 2=LR, 3=RR
static String deviceName;
static Preferences preferences;

// Current corner tracking (v2: starts at LF=0, then RF=1, LR=2, RR=3)
static Corner currentCorner = CORNER_LF;

// Forward declarations - State handlers
void handleWaitingConnection();
void handleCornerWaiting();
void handleStabilizing();
void handleCaptured();
void handleSessionComplete();
void handleError();

// Forward declarations - Helper functions
void transitionTo(DeviceState newState);
void resetSession();
void loadSettings();
Corner getNextCorner(Corner current);
const char* getCornerName(Corner corner);
DeviceState getCornerState(Corner corner);
DeviceState getStabilizingState(Corner corner);
DeviceState getCapturedState(Corner corner);

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n=================================");
    Serial.println("Tire Probe v2.0 - BLE Protocol v2");
    Serial.println("Model: " DEVICE_MODEL);
    Serial.println("=================================\n");

    // v2: Load settings from NVS (corner ID)
    loadSettings();

    // Initialize subsystems
    ledInit();
    ledUpdate(STATE_INITIALIZING);

    probesInit();
    Serial.println("[INIT] Probes: OK");

    if (displayInit()) {
        Serial.println("[INIT] Display: OK");
    } else {
        Serial.println("[INIT] Display: FAILED");
    }

    powerInit();
    Serial.println("[INIT] Power: OK");

    // v2: Initialize BLE with dynamic device name and corner ID
    bleInit(deviceName.c_str(), cornerID);
    bleStartAdvertising();
    Serial.println("[INIT] BLE: Advertising");

    // Ready
    resetSession();
    transitionTo(STATE_WAITING_CONNECTION);
}

void loop() {
    uint32_t now = millis();

    // Always update LED
    ledUpdate(currentState);

    // Read probes at interval
    if (now - lastProbeRead >= TEMP_READ_INTERVAL_MS) {
        lastProbeRead = now;
        MeasurementData measurement;
        probesUpdate(measurement);
    }

    // Update power/battery
    powerUpdate(systemStatus);

    // Periodic status broadcast (when connected) - v2: no capturedCount parameter
    if (bleIsConnected() && (now - lastStatusTx >= STATUS_TX_INTERVAL_MS)) {
        lastStatusTx = now;
        bleTransmitSystemStatus(systemStatus);
    }

    // State machine
    switch (currentState) {
        case STATE_WAITING_CONNECTION:
            handleWaitingConnection();
            break;

        case STATE_CORNER_RF:
        case STATE_CORNER_LF:
        case STATE_CORNER_LR:
        case STATE_CORNER_RR:
            handleCornerWaiting();
            break;

        case STATE_STABILIZING_RF:
        case STATE_STABILIZING_LF:
        case STATE_STABILIZING_LR:
        case STATE_STABILIZING_RR:
            handleStabilizing();
            break;

        case STATE_CAPTURED_RF:
        case STATE_CAPTURED_LF:
        case STATE_CAPTURED_LR:
        case STATE_CAPTURED_RR:
            handleCaptured();
            break;

        case STATE_SESSION_COMPLETE:
            handleSessionComplete();
            break;

        case STATE_ERROR:
            handleError();
            break;

        default:
            break;
    }

    // Update BLE
    bleUpdate();

    delay(10);  // Watchdog
}

// ========== State Handlers ==========

void handleWaitingConnection() {
    displayShowWaitingConnection();

    if (bleIsConnected()) {
        Serial.println("[STATE] BLE connected - starting session");
        currentCorner = CORNER_LF;  // v2: Start at LF (0), then RF, LR, RR
        resetSession();
        probesResetStability();
        transitionTo(STATE_CORNER_LF);  // v2: Changed from STATE_CORNER_RF
    }
}

void handleCornerWaiting() {
    displayShowCornerPrompt(currentCorner);

    // Check for disconnect
    if (!bleIsConnected()) {
        Serial.println("[STATE] BLE disconnected");
        transitionTo(STATE_WAITING_CONNECTION);
        return;
    }

    // Check for probe contact
    if (probesDetectContact()) {
        Serial.printf("[STATE] Contact detected on %s\n", getCornerName(currentCorner));
        transitionTo(getStabilizingState(currentCorner));
    }
}

void handleStabilizing() {
    float progress = probesGetStabilityProgress();
    displayShowStabilizing(currentCorner, progress);

    // Check for disconnect
    if (!bleIsConnected()) {
        transitionTo(STATE_WAITING_CONNECTION);
        return;
    }

    // Check if contact lost
    if (!probesDetectContact()) {
        Serial.println("[STATE] Contact lost - returning to corner wait");
        probesResetStability();
        transitionTo(getCornerState(currentCorner));
        return;
    }

    // Check for stability achieved
    if (probesAreStable()) {
        // CAPTURE!
        CornerReading reading = probesCapture(currentCorner);
        session.corners[currentCorner] = reading;
        session.capturedCount++;

        // Transmit via BLE
        bleTransmitCornerReading(reading);

        Serial.printf("[CAPTURE] %s complete (%d/4)\n",
                      getCornerName(currentCorner), session.capturedCount);

        transitionTo(getCapturedState(currentCorner));
    }
}

void handleCaptured() {
    displayShowCaptured(session.corners[currentCorner]);

    // Show capture confirmation for CAPTURE_DISPLAY_MS
    if (millis() - stateEntryTime >= CAPTURE_DISPLAY_MS) {
        // Check if session complete
        if (session.capturedCount >= 4) {
            session.isComplete = true;
            transitionTo(STATE_SESSION_COMPLETE);
        } else {
            // Move to next corner
            currentCorner = getNextCorner(currentCorner);
            probesResetStability();
            transitionTo(getCornerState(currentCorner));
        }
    }
}

void handleSessionComplete() {
    displayShowComplete(session);

    // Wait for disconnect to reset
    if (!bleIsConnected()) {
        Serial.println("[STATE] Session complete - disconnected");
        transitionTo(STATE_WAITING_CONNECTION);
    }
}

void handleError() {
    displayShowError("Sensor Error");

    // Try to recover after 3 seconds
    if (millis() - stateEntryTime >= 3000) {
        transitionTo(STATE_WAITING_CONNECTION);
    }
}

// ========== Helper Functions ==========

void transitionTo(DeviceState newState) {
    Serial.printf("[STATE] %d -> %d\n", currentState, newState);
    currentState = newState;
    stateEntryTime = millis();
    systemStatus.state = newState;
}

void resetSession() {
    session.capturedCount = 0;
    session.isComplete = false;
    for (int i = 0; i < 4; i++) {
        session.corners[i] = CornerReading();
    }
    Serial.println("[SESSION] Reset");
}

// v2: Load settings from NVS
void loadSettings() {
    preferences.begin(NVS_NAMESPACE, false);
    cornerID = preferences.getUChar(NVS_CORNER_KEY, DEFAULT_CORNER_ID);
    preferences.end();

    // Validate corner ID (must be 0-3)
    if (cornerID > 3) {
        Serial.printf("[NVS] Invalid corner ID: %d, resetting to default\n", cornerID);
        cornerID = DEFAULT_CORNER_ID;
    }

    // v2: Build dynamic device name: TireProbe_XX (where XX = corner string)
    deviceName = String(DEVICE_NAME_BASE) + "_" + getCornerString(cornerID);

    Serial.println("=== Settings loaded from NVS ===");
    Serial.printf("Corner ID: %d (%s)\n", cornerID, getCornerString(cornerID));
    Serial.printf("Device name: %s\n", deviceName.c_str());
}

Corner getNextCorner(Corner current) {
    // v2: Sequence is LF(0) -> RF(1) -> LR(2) -> RR(3)
    switch (current) {
        case CORNER_LF: return CORNER_RF;
        case CORNER_RF: return CORNER_LR;
        case CORNER_LR: return CORNER_RR;
        case CORNER_RR: return CORNER_LF;  // Wrap (shouldn't happen)
        default: return CORNER_LF;
    }
}

const char* getCornerName(Corner corner) {
    // v2: Use getCornerString from ble_service (matches Corner enum values 0-3)
    return getCornerString(corner);
}

DeviceState getCornerState(Corner corner) {
    // v2: Updated to match new Corner enum (LF=0, RF=1, LR=2, RR=3)
    switch (corner) {
        case CORNER_LF: return STATE_CORNER_LF;
        case CORNER_RF: return STATE_CORNER_RF;
        case CORNER_LR: return STATE_CORNER_LR;
        case CORNER_RR: return STATE_CORNER_RR;
        default: return STATE_CORNER_LF;
    }
}

DeviceState getStabilizingState(Corner corner) {
    // v2: Updated to match new Corner enum
    switch (corner) {
        case CORNER_LF: return STATE_STABILIZING_LF;
        case CORNER_RF: return STATE_STABILIZING_RF;
        case CORNER_LR: return STATE_STABILIZING_LR;
        case CORNER_RR: return STATE_STABILIZING_RR;
        default: return STATE_STABILIZING_LF;
    }
}

DeviceState getCapturedState(Corner corner) {
    // v2: Updated to match new Corner enum
    switch (corner) {
        case CORNER_LF: return STATE_CAPTURED_LF;
        case CORNER_RF: return STATE_CAPTURED_RF;
        case CORNER_LR: return STATE_CAPTURED_LR;
        case CORNER_RR: return STATE_CAPTURED_RR;
        default: return STATE_CAPTURED_LF;
    }
}

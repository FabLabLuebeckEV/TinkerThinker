// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"
#include <Arduino.h>
#include <Bluepad32.h>
#include <WiFi.h>
#include "TinkerThinkerBoard.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_bt.h"
#include <uni.h>

// Removed DFPlayer and HardwareSerial includes
#define MODE_BUTTON_PIN 39

// Erzeuge globalen Board- und ConfigManager
ConfigManager configManager;
TinkerThinkerBoard board(&configManager);

// Soundboard (DFPlayer) removed per request


// Button/DPad enums removed; new InputBindingManager handles mapping

// --- GLOBAL VARIABLES ---
ControllerPtr myControllers[BP32_MAX_GAMEPADS];
// Input binding processor
#include "InputBindingManager.h"
static InputBindingManager inputBindings(&board, &configManager);

long timestampServo = 0;

enum class RadioMode { Normal, BluetoothOnly, WifiOnly };
static RadioMode radioMode = RadioMode::Normal;
static RadioMode lastAppliedMode = RadioMode::Normal;
static bool lastModeButtonState = true;
static uint32_t lastModeButtonChangeMs = 0;
static const uint32_t modeButtonDebounceMs = 50;
static int modeStep = 0;
static uint32_t wifiPauseUntilMs = 0;
static bool wifiPausedForBt = false;
static const uint32_t wifiPauseOnConnectMs = 3000;

static void setStatusLed(uint8_t r, uint8_t g, uint8_t b);
static void applyRadioMode(RadioMode mode);
static bool handleStartupReset();

static int findControllerIndex(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (myControllers[i] == ctl) return i;
    }
    return -1;
}

void processMouse(ControllerPtr ctl);        // Stub, falls unbenutzt einfach leer lassen
void processKeyboard(ControllerPtr ctl);     //   "
void processBalanceBoard(ControllerPtr ctl); //   "
void processMouse(ControllerPtr) {}
void processKeyboard(ControllerPtr) {}
void processBalanceBoard(ControllerPtr) {}


// This callback gets called any time a new gamepad is connected.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Console.printf("CALLBACK: Controller is connected, index=%d\n", i);
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            board.setLED(0, 0, 255, 0); // Green
            board.showLEDs();
            if (radioMode == RadioMode::Normal) {
                wifiPauseUntilMs = millis() + wifiPauseOnConnectMs;
            }
            break;
        }
    }
    if (!foundEmptySlot) {
        Console.println("CALLBACK: Controller connected, but could not find empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Console.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            board.setLED(0, 255, 0, 0); // Red
            board.showLEDs();
            // Stop all motors as a safety measure
            for (int j = 0; j < 4; j++) {
                board.controlMotorStop(j);
            }
            break;
        }
    }

    if (!foundController) {
        Console.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

// Button/axis handling now driven by InputBindingManager

void processGamepad(ControllerPtr ctl) {
    // Dispatch to input binding manager
    int idx = findControllerIndex(ctl);
    if (idx >= 0) inputBindings.process(ctl, idx);

    // Update player LEDs to show battery level
    float voltage = board.getBatteryVoltage();
    if (voltage > 4.0) {
        ctl->setPlayerLEDs(0b1111);
    } else if (voltage > 3.8) {
        ctl->setPlayerLEDs(0b0111);
    } else if (voltage > 3.5) {
        ctl->setPlayerLEDs(0b0011);
    } else {
        ctl->setPlayerLEDs(0b0001);
    }
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else if (myController->isMouse()) {
                processMouse(myController);
            } else if (myController->isKeyboard()) {
                processKeyboard(myController);
            } else if (myController->isBalanceBoard()) {
                processBalanceBoard(myController);
            } else {
                Console.printf("Unsupported controller\n");
            }
        }
    }
}

// Arduino setup function. Runs in CPU 1
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

    Serial.begin(115200);

    // DFPlayer initialization removed

    if (!configManager.init()) {
        Serial.println("Failed to init ConfigManager!");
    }
    board.begin();
    pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
    if (handleStartupReset()) {
        return;
    }

    // Load input bindings from config
    inputBindings.reload();

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    BP32.setup(&onConnectedController, &onDisconnectedController, true);
    BP32.forgetBluetoothKeys();
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
    sm_set_secure_connections_only_mode(false);                        // SC ausschalten
    uni_bt_allowlist_set_enabled(false);                             // Allowlist ausschalten

    setStatusLed(60, 60, 60);
}

// Arduino loop function. Runs in CPU 1.
// Duty-cycle configuration for BT scanning (defaults overridden by ConfigManager)
static uint32_t SCAN_ON_MS_NORMAL = 500;    // balanced when WiFi is idle
static uint32_t SCAN_OFF_MS_NORMAL = 500;
static uint32_t SCAN_ON_MS_STA_CONNECT = 150;  // give STA connect more airtime
static uint32_t SCAN_OFF_MS_STA_CONNECT = 850;
static uint32_t SCAN_ON_MS_AP_ACTIVE = 100;     // AP serving clients: keep scanning minimal
static uint32_t SCAN_OFF_MS_AP_ACTIVE = 1900;

static bool scanEnabled = false;
static uint32_t nextToggleAt = 0;
static uint32_t curOnMs = SCAN_ON_MS_NORMAL;
static uint32_t curOffMs = SCAN_OFF_MS_NORMAL;
static enum { PHASE_ON, PHASE_OFF } phase = PHASE_OFF;

static void setStatusLed(uint8_t r, uint8_t g, uint8_t b) {
    board.setLED(0, r, g, b);
    board.showLEDs();
}

static bool handleStartupReset() {
    const uint32_t holdMs = 10000;
    const uint32_t stepMs = 250;
    if (digitalRead(MODE_BUTTON_PIN) != LOW) {
        return false;
    }

    uint32_t start = millis();
    int phaseIndex = 0;
    while ((millis() - start) < holdMs) {
        if (digitalRead(MODE_BUTTON_PIN) != LOW) {
            setStatusLed(60, 60, 60);
            return false;
        }
        if ((phaseIndex % 3) == 0) setStatusLed(255, 0, 0);       // red
        else if ((phaseIndex % 3) == 1) setStatusLed(255, 255, 255); // white
        else setStatusLed(255, 120, 0);                           // orange
        delay(stepMs);
        phaseIndex++;
    }

    bool ok = configManager.resetConfig();
    if (!ok) {
        Serial.println("Config reset failed!");
    }
    delay(200);
    ESP.restart();
    return true;
}

static void applyRadioMode(RadioMode mode) {
    if (mode == lastAppliedMode) return;
    lastAppliedMode = mode;

    if (mode == RadioMode::Normal) {
        board.requestWifiEnable();
        BP32.enableNewBluetoothConnections(false);
        scanEnabled = false;
        phase = PHASE_OFF;
        nextToggleAt = millis();
        setStatusLed(60, 60, 60); // dim white
    } else if (mode == RadioMode::BluetoothOnly) {
        board.requestWifiDisable(false);
        BP32.enableNewBluetoothConnections(true);
        scanEnabled = true;
        phase = PHASE_ON;
        setStatusLed(0, 0, 255);
    } else if (mode == RadioMode::WifiOnly) {
        // First drop BT connections, then disable scanning, then set color
        for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
            if (myControllers[i] && myControllers[i]->isConnected()) {
                myControllers[i]->disconnect();
            }
        }
        BP32.enableNewBluetoothConnections(false);
        scanEnabled = false;
        phase = PHASE_OFF;
        board.requestWifiEnable();
        setStatusLed(255, 120, 0);
    }
}

void loop() {
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();
    // Refresh scan timings from config (lightweight)
    SCAN_ON_MS_NORMAL       = configManager.getBtScanOnNormal();
    SCAN_OFF_MS_NORMAL      = configManager.getBtScanOffNormal();
    SCAN_ON_MS_STA_CONNECT  = configManager.getBtScanOnSta();
    SCAN_OFF_MS_STA_CONNECT = configManager.getBtScanOffSta();
    SCAN_ON_MS_AP_ACTIVE    = configManager.getBtScanOnAp();
    SCAN_OFF_MS_AP_ACTIVE   = configManager.getBtScanOffAp();

    // Mode button handling (active-low)
    bool modeNow = (digitalRead(MODE_BUTTON_PIN) == LOW);
    uint32_t nowMs = millis();
    if (modeNow != lastModeButtonState && (nowMs - lastModeButtonChangeMs) > modeButtonDebounceMs) {
        lastModeButtonChangeMs = nowMs;
        lastModeButtonState = modeNow;
        if (modeNow) {
            modeStep = (modeStep + 1) % 4;
            if (modeStep == 0) radioMode = RadioMode::Normal;
            else if (modeStep == 1) radioMode = RadioMode::WifiOnly;
            else if (modeStep == 2) radioMode = RadioMode::BluetoothOnly;
            else radioMode = RadioMode::WifiOnly;
        }
    }
    applyRadioMode(radioMode);

    // Temporary WiFi pause to boost BT during connect (only in Normal mode)
    if (radioMode == RadioMode::Normal) {
        uint32_t now = millis();
        if (wifiPauseUntilMs > now) {
            if (!wifiPausedForBt) {
                board.requestWifiDisable(false);
                BP32.enableNewBluetoothConnections(true);
                wifiPausedForBt = true;
            }
        } else if (wifiPausedForBt) {
            board.requestWifiEnable();
            wifiPausedForBt = false;
        }
    } else if (wifiPausedForBt) {
        wifiPausedForBt = false;
        wifiPauseUntilMs = 0;
    }

    // Determine current scenario and duty cycle settings
    bool anyController = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] && myControllers[i]->isConnected()) {
            anyController = true;
            break;
        }
    }

    wifi_mode_t mode = WiFi.getMode();
    bool staConnecting = (mode == WIFI_STA || mode == WIFI_AP_STA) && (WiFi.status() != WL_CONNECTED);
    bool apActive = (mode == WIFI_AP || mode == WIFI_AP_STA) && (WiFi.softAPgetStationNum() > 0);
    bool wifiDisabledUntilRestart = (mode == WIFI_OFF);

    if (radioMode == RadioMode::BluetoothOnly) {
        if (!scanEnabled) {
            BP32.enableNewBluetoothConnections(true);
            scanEnabled = true;
        }
        vTaskDelay(10);
        return;
    }

    if (radioMode == RadioMode::WifiOnly) {
        if (scanEnabled) {
            BP32.enableNewBluetoothConnections(false);
            scanEnabled = false;
            phase = PHASE_OFF;
        }
        vTaskDelay(10);
        return;
    }

    if (wifiDisabledUntilRestart) {
        // Keep Bluetooth scanning fully enabled while Wi-Fi is down.
        if (!anyController) {
            if (!scanEnabled) {
                BP32.enableNewBluetoothConnections(true);
                scanEnabled = true;
                phase = PHASE_ON;
            }
        } else if (scanEnabled) {
            BP32.enableNewBluetoothConnections(false);
            scanEnabled = false;
            phase = PHASE_OFF;
        }
        vTaskDelay(10);
        return;
    }

    // Policy:
    // - If a controller is already connected: don't scan.
    // - Else if AP has clients: very low-duty scan.
    // - Else if STA is connecting: low-duty scan.
    // - Else: balanced scan.
    uint32_t targetOn = SCAN_ON_MS_NORMAL;
    uint32_t targetOff = SCAN_OFF_MS_NORMAL;
    if (anyController) {
        targetOn = 0; targetOff = 1000;  // keep scanning off
    } else if (apActive) {
        targetOn = SCAN_ON_MS_AP_ACTIVE; targetOff = SCAN_OFF_MS_AP_ACTIVE;
    } else if (staConnecting) {
        targetOn = SCAN_ON_MS_STA_CONNECT; targetOff = SCAN_OFF_MS_STA_CONNECT;
    }

    if (targetOn != curOnMs || targetOff != curOffMs) {
        // Scenario changed: reset scheduler
        curOnMs = targetOn; curOffMs = targetOff;
        phase = PHASE_OFF;
        scanEnabled = false;
        BP32.enableNewBluetoothConnections(false);
        nextToggleAt = millis() + curOffMs;
    }

    // Toggle scanning according to duty cycle (unless anyController)
    uint32_t now = millis();
    if (!anyController) {
        if (now >= nextToggleAt) {
            if (phase == PHASE_OFF) {
                // Turn scanning on for the on-duration
                scanEnabled = (curOnMs > 0);
                BP32.enableNewBluetoothConnections(scanEnabled);
                phase = PHASE_ON;
                nextToggleAt = now + (curOnMs > 0 ? curOnMs : curOffMs);
            } else {
                // Turn scanning off for the off-duration
                scanEnabled = false;
                BP32.enableNewBluetoothConnections(false);
                phase = PHASE_OFF;
                nextToggleAt = now + curOffMs;
            }
        }
    } else if (scanEnabled) {
        // Ensure scanning is off while a controller is connected
        BP32.enableNewBluetoothConnections(false);
        scanEnabled = false;
        phase = PHASE_OFF;
        nextToggleAt = now + 1000;
    }
    //board.updateWebClients();

    // A delay is required to prevent the watchdog timer from triggering.
    vTaskDelay(10);
}

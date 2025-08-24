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

#include "HardwareSerial.h"
#include <DFRobotDFPlayerMini.h>

// Erzeuge globalen Board- und ConfigManager
ConfigManager configManager;
TinkerThinkerBoard board(&configManager);

// DFPlayer Mini
HardwareSerial mySoftwareSerial(2);
DFRobotDFPlayerMini myDFPlayer;


// --- ENUMS ---
enum Buttons {
    BTN_B = 1,
    BTN_A = 2,
    BTN_Y = 4,
    BTN_X = 8,
    BUTTON_L1 = 16,
    BUTTON_R1 = 32,
    BUTTON_L2 = 64,
    BUTTON_R2 = 128,
    BUTTON_STICK_L = 256,
    BUTTON_STICK_R = 512
};

enum class DPad : unsigned long {
    UP = 1 << 0,
    DOWN = 1 << 1,
    RIGHT = 1 << 2,
    LEFT = 1 << 3
};

enum class MiscButtons : unsigned long {
    HOME = 1 << 0,
    MINUS = 1 << 1,
    PLUS = 1 << 2,
    DOT = 1 << 3
};

// --- GLOBAL VARIABLES ---
ControllerPtr myControllers[BP32_MAX_GAMEPADS];


long timestampServo = 0;

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

void processButtons(ControllerPtr ctl) {
    // Regular buttons
    unsigned long buttonState = ctl->buttons();
    if (buttonState) {
        if (buttonState & BTN_B) {
            for (int i = 1; i < 20; i++) board.setLED(i, 120, 120, 0);
            board.showLEDs();
        }
        if (buttonState & BTN_A) {
            for (int i = 1; i < 20; i++) board.setLED(i, 0, 120, 0);
            board.showLEDs();
        }
        if (buttonState & BTN_Y) {
            for (int i = 1; i < 20; i++) board.setLED(i, 0, 120, 120);
            board.showLEDs();
        }
        if (buttonState & BTN_X) {
            for (int i = 1; i < 20; i++) board.setLED(i, 0, 0, 0);
            board.showLEDs();
        }
        if (buttonState & BUTTON_R1) {
            myDFPlayer.play(1);
        }
        if (buttonState & BUTTON_L1) {
            myDFPlayer.play(2);
        }
    }

    // D-Pad
    unsigned long dpadState = ctl->dpad();
    if (dpadState) {
        if (dpadState & static_cast<unsigned long>(DPad::UP)) {
            myDFPlayer.volumeUp();
            delay(100);
        }
        if (dpadState & static_cast<unsigned long>(DPad::DOWN)) {
            myDFPlayer.volumeDown();
            delay(100);
        }
        if (dpadState & static_cast<unsigned long>(DPad::RIGHT)) {
            board.setServoAngle(2, board.getServoAngle(2) + 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::LEFT)) {
            board.setServoAngle(2, board.getServoAngle(2) - 10);
        }
    }

    // Misc Buttons (Home, etc.)
    unsigned long miscState = ctl->miscButtons();
    if (miscState) {
        // Empty now
    }
}

void processGamepad(ControllerPtr ctl) {
    // Control motors with joysticks (source-aware)
    // Right stick -> selected GUI pair; Left stick -> the other pair
    board.requestDriveFromBT(ctl->axisRX(), ctl->axisRY());
    board.requestDriveOtherFromBT(ctl->axisX(), ctl->axisY());

    // Process button presses for LEDs and Servos
    processButtons(ctl);

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

    // Init DFPlayer
    mySoftwareSerial.begin(9600, SERIAL_8N1, 5, 19);  // RX, TX
    Serial.println();
    Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
    if (!myDFPlayer.begin(mySoftwareSerial)) {
        Serial.println(F("Unable to begin:"));
        Serial.println(F("1.Please recheck the connection!"));
        Serial.println(F("2.Please insert the SD card!"));
        while(true){
            delay(0); // Code to compatible with ESP32 watch dog.
        }
    }
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(30);  //Set volume value. From 0 to 30

    if (!configManager.init()) {
        Serial.println("Failed to init ConfigManager!");
    }
    board.begin();

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    BP32.setup(&onConnectedController, &onDisconnectedController, true);
    BP32.forgetBluetoothKeys();
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
    sm_set_secure_connections_only_mode(false);                        // SC ausschalten
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
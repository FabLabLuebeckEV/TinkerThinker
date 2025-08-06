// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"
#include <Arduino.h>
#include <Bluepad32.h>
#include "TinkerThinkerBoard.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Erzeuge globalen Board- und ConfigManager
ConfigManager configManager;
TinkerThinkerBoard board(&configManager);

// --- CONSTANTS ---
#define DEBUG_OUTPUT 1
#define MAX_JOYSTICK_VALUE 512
#define MAX_PWM_VALUE 255

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
            for (int i = 1; i < 20; i++) board.setLED(i, 0, 0, 120);
            board.showLEDs();
        }
        if (buttonState & BUTTON_R1) {
            if (millis() - timestampServo < 1000) return;
            timestampServo = millis();
            board.setServoAngle(0, 150);
            delay(100);
            board.setServoAngle(0, 180);
        }
    }

    // D-Pad
    unsigned long dpadState = ctl->dpad();
    if (dpadState) {
        if (dpadState & static_cast<unsigned long>(DPad::UP)) {
            board.setServoAngle(0, board.getServoAngle(0) + 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::DOWN)) {
            board.setServoAngle(0, board.getServoAngle(0) - 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::RIGHT)) {
            board.setServoAngle(1, board.getServoAngle(1) + 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::LEFT)) {
            board.setServoAngle(1, board.getServoAngle(1) - 10);
        }
    }

    // Misc Buttons (Home, etc.)
    unsigned long miscState = ctl->miscButtons();
    if (miscState & static_cast<unsigned long>(MiscButtons::HOME) || miscState & static_cast<unsigned long>(MiscButtons::MINUS)) {
        // This button combination can be used for other purposes now
    }
}

void processGamepad(ControllerPtr ctl) {
#ifdef DEBUG_OUTPUT
    // Optional: Print joystick values for debugging
    // Console.printf("L(X,Y): (%d, %d) | R(X,Y): (%d, %d)\n", ctl->axisX(), ctl->axisY(), ctl->axisRX(), ctl->axisRY());
#endif

    // Control motors with joysticks
    board.controlMotors(ctl->axisRX(), ctl->axisRY());
    board.controlMotors(ctl->axisX(), ctl->axisY());


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
void loop() {
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();
    
    board.updateWebClients();

    // A delay is required to prevent the watchdog timer from triggering.
    vTaskDelay(10);
}
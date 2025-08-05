// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"
#include <Arduino.h>
#include <Bluepad32.h>
#include <FastLED.h>
#include <ESP32Servo.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
static esp_adc_cal_characteristics_t* adc_chars = nullptr;

// --- DEFINES & CONSTANTS ---
#define DEBUG_OUTPUT 1
#define POWER_ON_PIN 26
#define GPIO_FAULT_1 36
#define GPIO_FAULT_2 34
#define DEAD_BAND 50

#define NUM_LEDS 20
#define LED_PIN 2
CRGB leds[NUM_LEDS];

#define BATTERY_PIN ADC1_CHANNEL_7
#define DEFAULT_VREF 1100

#define MAX_JOYSTICK_VALUE 255
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

// --- STRUCTURES ---
typedef struct {
    int pin1;
    int pin2;
    int channel_forward;
    int channel_reverse;
} Motor;

// --- GLOBAL VARIABLES ---
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

Motor motors[4] = {
    {32, 27, 2, 3},  // Motor 1
    {25, 16, 0, 1},  // Motor 2
    {4, 12, 4, 5},   // Motor 3
    {15, 14, 6, 7}   // Motor 4
};

Servo servos[3];
const int servoPins[] = {13, 33, 17};

int JoyLxNeutral = -1;
int JoyLyNeutral = -1;
int JoyRxNeutral = -1;
int JoyRyNeutral = -1;

long timestampServo = 0;

void controlMotor(Motor* motor, int pwmValue);
void setServoAngle(int servoIndex, int angle);
void handleMotorControl(int axisX, int axisY, Motor* leftMotor, Motor* rightMotor);
float readBatteryVoltage();
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
            leds[0] = CRGB::Green;
            FastLED.show();
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
            leds[0] = CRGB::Red;
            FastLED.show();
            // Stop all motors as a safety measure
            for (int j = 0; j < 4; j++) {
                controlMotor(&motors[j], 0);
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
            for (int i = 1; i < NUM_LEDS; i++) leds[i] = CRGB(120, 120, 0);
            FastLED.show();
        }
        if (buttonState & BTN_A) {
            for (int i = 1; i < NUM_LEDS; i++) leds[i] = CRGB(0, 120, 0);
            FastLED.show();
        }
        if (buttonState & BTN_Y) {
            for (int i = 1; i < NUM_LEDS; i++) leds[i] = CRGB(0, 120, 120);
            FastLED.show();
        }
        if (buttonState & BTN_X) {
            for (int i = 1; i < NUM_LEDS; i++) leds[i] = CRGB(0, 0, 120);
            FastLED.show();
        }
        if (buttonState & BUTTON_R1) {
            if (millis() - timestampServo < 1000) return;
            timestampServo = millis();
            setServoAngle(0, 150);
            delay(100);
            setServoAngle(0, 180);
        }
    }

    // D-Pad
    unsigned long dpadState = ctl->dpad();
    if (dpadState) {
        if (dpadState & static_cast<unsigned long>(DPad::UP)) {
            setServoAngle(0, servos[0].read() + 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::DOWN)) {
            setServoAngle(0, servos[0].read() - 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::RIGHT)) {
            setServoAngle(1, servos[1].read() + 10);
        }
        if (dpadState & static_cast<unsigned long>(DPad::LEFT)) {
            setServoAngle(1, servos[1].read() - 10);
        }
    }

    // Misc Buttons (Home, etc.)
    unsigned long miscState = ctl->miscButtons();
    if (miscState & static_cast<unsigned long>(MiscButtons::HOME) || miscState & static_cast<unsigned long>(MiscButtons::MINUS)) {
        Console.println("Recalibrating joysticks");
        JoyLxNeutral = ctl->axisX();
        JoyLyNeutral = ctl->axisY();
        JoyRxNeutral = ctl->axisRX();
        JoyRyNeutral = ctl->axisRY();
    }
}

void processGamepad(ControllerPtr ctl) {
#ifdef DEBUG_OUTPUT
    // Optional: Print joystick values for debugging
    // Console.printf("L(X,Y): (%d, %d) | R(X,Y): (%d, %d)\n", ctl->axisX() - JoyLxNeutral, ctl->axisY() - JoyLyNeutral, ctl->axisRX() - JoyRxNeutral, ctl->axisRY() - JoyRyNeutral);
#endif

    // Calibrate joysticks on first run
    if (JoyLxNeutral == -1) {
        Console.println("Calibrating joysticks...");
        JoyLxNeutral = ctl->axisX();
        JoyLyNeutral = ctl->axisY();
        JoyRxNeutral = ctl->axisRX();
        JoyRyNeutral = ctl->axisRY();
    }

    // Control motors with joysticks
    handleMotorControl(ctl->axisRX() - JoyRxNeutral, ctl->axisRY() - JoyRyNeutral, &motors[0], &motors[1]);
    handleMotorControl(ctl->axisX() - JoyLxNeutral, ctl->axisY() - JoyLyNeutral, &motors[2], &motors[3]);

    // Process button presses for LEDs and Servos
    processButtons(ctl);

    // Update player LEDs to show battery level
    float voltage = readBatteryVoltage();
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

// --- INITIALIZATION FUNCTIONS ---
void initMotors() {
    for (int i = 0; i < 4; i++) {
        pinMode(motors[i].pin1, OUTPUT);
        pinMode(motors[i].pin2, OUTPUT);
        //ledcSetup(motors[i].channel_forward, 5000, 8);
        //ledcSetup(motors[i].channel_reverse, 5000, 8);
        ledcAttach(motors[i].pin1, motors[i].channel_forward, 8);
        ledcAttach(motors[i].pin2, motors[i].channel_reverse, 8);
        ledcChangeFrequency(motors[i].pin1, 5000, 8);
        ledcChangeFrequency(motors[i].pin2, 5000, 8);
    }
}

void initServos() {
    for (int i = 0; i < 3; i++) {
        servos[i].attach(servoPins[i]);
    }
}

void initLeds() {
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}

void initGPIOFaultCheck() {
    pinMode(GPIO_FAULT_1, INPUT);
    pinMode(GPIO_FAULT_2, INPUT);
}

void initADC() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(BATTERY_PIN, ADC_ATTEN_DB_12);
    adc_chars = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
}

// --- HELPER FUNCTIONS ---
float readBatteryVoltage() {
    uint32_t adc_reading = adc1_get_raw(BATTERY_PIN);   // so ist es korrekt
    uint32_t voltage_at_pin_mV = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    return (float)(voltage_at_pin_mV * 2) / 1000.0;
}

void checkMotorDriverFault() {
    if (digitalRead(GPIO_FAULT_1) == LOW) {
        Console.println("Motor1 Driver Fault");
    }
    if (digitalRead(GPIO_FAULT_2) == LOW) {
        Console.println("Motor2 Driver Fault");
    }
}

void setServoAngle(int servoIndex, int angle) {
    if (servoIndex < 0 || servoIndex >= 3) return;
    angle = constrain(angle, 0, 180);
    servos[servoIndex].write(angle);
}

// --- MOTOR CONTROL FUNCTIONS ---
void controlMotor(Motor* motor, int pwmValue) {
    pwmValue = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    if (pwmValue >= 0) {
        ledcWrite(motor->channel_forward, pwmValue);
        ledcWrite(motor->channel_reverse, 0);
    } else {
        ledcWrite(motor->channel_forward, 0);
        ledcWrite(motor->channel_reverse, -pwmValue);
    }
}

int scaleMovementToPWM(float movement) {
    if (movement > 0.0) {
        return 110 + (int)(movement * (255 - 110));
    } else if (movement < 0.0) {
        return -110 - (int)(abs(movement) * (255 - 110));
    }
    return 0;
}

void handleMotorControl(int axisX, int axisY, Motor* leftMotor, Motor* rightMotor) {
    int normX = (abs(axisX) < DEAD_BAND) ? 0 : axisX;
    int normY = (abs(axisY) < DEAD_BAND) ? 0 : axisY;

    float maxMovement = (float)(MAX_JOYSTICK_VALUE - DEAD_BAND);
    float movementLeft = (float)(normY + normX) / maxMovement;
    float movementRight = (float)(normY - normX) / maxMovement;

    int rightPWM = scaleMovementToPWM(movementLeft);
    int leftPWM = scaleMovementToPWM(movementRight);

    leftPWM = constrain(leftPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    rightPWM = constrain(rightPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    controlMotor(leftMotor, leftPWM);
    controlMotor(rightMotor, rightPWM);

#ifdef DEBUG_OUTPUT
    Console.printf("L_PWM: %d, R_PWM: %d\n", leftPWM, rightPWM);
#endif
}

// Arduino setup function. Runs in CPU 1
void setup() {
    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);

    initMotors();
    initServos();
    initLeds();
    initGPIOFaultCheck();
    initADC();
    Console.println("Peripherals Initialized");

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
}

// Arduino loop function. Runs in CPU 1.
void loop() {
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();

    // A delay is required to prevent the watchdog timer from triggering.
    vTaskDelay(10);
}
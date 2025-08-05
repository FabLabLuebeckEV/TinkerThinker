// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"
#include <Arduino.h>
#include <Bluepad32.h>
#include <stdio.h>
#include "driver/rmt.h"
#include "esp_err.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <uni.h>

// Definiere hier die MAC-Adressen der erlaubten Controller
const char* allowedAddresses[] = {"02:21:EC:01:81:AF", "01:21:EC:01:81:AF", "84:21:EC:01:81:AF", "03:21:EC:01:81:AF"};
const int numAllowed = sizeof(allowedAddresses) / sizeof(allowedAddresses[0]);

#define DEBUG_OUTPUT 1

#define POWER_ON_PIN 26
#define GPIO_FAULT_1 36
#define GPIO_FAULT_2 34
#define DEAD_BAND 50

#define LED_PIN 2
#define RMT_TX_CHANNEL RMT_CHANNEL_0
// Angepasste RMT-Timings für WS2812 (in RMT-Ticks bei clk_div = 2, 40MHz)
#define T0H 16
#define T0L 34
#define T1H 34
#define T1L 16
#define BITS_PER_LED_CMD 24
#define NUM_LEDS 20
#define TOTAL_BITS (NUM_LEDS * BITS_PER_LED_CMD)

static rmt_item32_t rmt_items[TOTAL_BITS];
/*#define T0H 14
#define T0L 52
#define T1H 52
#define T1L 14
#define BITS_PER_LED_CMD 24*/

#define BATTERY_PIN ADC1_CHANNEL_7
#define DEFAULT_VREF 1100
static esp_adc_cal_characteristics_t* adc_chars;

#define SERVO_MIN_PULSEWIDTH 500
#define SERVO_MAX_PULSEWIDTH 2400
#define LEDC_FREQUENCY 50
#define LEDC_RESOLUTION 16

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

typedef struct {
    int pin1;
    int pin2;
    int channel_forward;
    int channel_reverse;
} Motor;

typedef struct {
    int pin;
    int channel;
    int angle;
} Servo;

Motor motors[4] = {
    {32, 27, 2, 3},  // Motor 1 {16, 25, 0, 1}
    {25, 16, 0, 1},  // Motor 2 {32, 27, 2, 3}
    {4, 12, 4, 5},   // Motor 3 {4, 12, 4, 5}
    {15, 14, 6, 7}   // Motor 4 {15, 14, 6, 7}
};

Servo servos[3] = {
    {13, 8, 90},  // Servo 1
    {33, 9, 90},  // Servo 2
    {17, 10, 90}  // Servo 3
};

#define MAX_JOYSTICK_VALUE 255 // Maximale Achsenwert
#define MAX_PWM_VALUE 255      // Maximale PWM-Wert

int JoyLxNeutral = -1;   // From your readings
int JoyLyNeutral = -1;
int JoyRxNeutral = -1;
int JoyRyNeutral = -1;

long timestampServo = 0;


ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void initADC() {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(BATTERY_PIN, ADC_ATTEN_DB_12);
    adc_chars = (esp_adc_cal_characteristics_t*)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    if (adc_chars == NULL) {
        ESP_LOGE("ADC", "Failed to allocate memory for ADC characteristics");
        return;
    }
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_12,
        ADC_WIDTH_BIT_12,
        DEFAULT_VREF,
        adc_chars
    );
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI("ADC", "ADC calibration using e-fuse Vref");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI("ADC", "ADC calibration using Two Point");
    } else {
        ESP_LOGI("ADC", "ADC calibration using default Vref");
    }
}

float readBatteryVoltage() {
    uint32_t adc_reading = adc1_get_raw(BATTERY_PIN);
    uint32_t voltage_at_pin_mV = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    float battery_voltage = (float)(voltage_at_pin_mV * 2) / 1000.0;
    return battery_voltage;
}

void setServoAngle(Servo* servo, int angle) {
    angle = constrain(angle, 0, 180);
    int pulseWidth = map(angle, 0, 180, SERVO_MIN_PULSEWIDTH, SERVO_MAX_PULSEWIDTH);
    int dutyCycle = (pulseWidth * ((1 << LEDC_RESOLUTION) - 1)) / 20000;
    ledcWrite(servo->channel, dutyCycle);
    servo->angle = angle;
}

void initServos() {
    for (int i = 0; i < 3; i++) {
        ledcSetup(servos[i].channel, LEDC_FREQUENCY, LEDC_RESOLUTION);
        ledcAttachPin(servos[i].pin, servos[i].channel);
    }
}

void initMotors() {
    for (int i = 0; i < 4; i++) {
        pinMode(motors[i].pin1, OUTPUT);
        pinMode(motors[i].pin2, OUTPUT);
        ledcSetup(motors[i].channel_forward, 5000, 8);
        ledcSetup(motors[i].channel_reverse, 5000, 8);
        ledcAttachPin(motors[i].pin1, motors[i].channel_forward);
        ledcAttachPin(motors[i].pin2, motors[i].channel_reverse);
    }
}

void initGPIOFaultCheck() {
    pinMode(GPIO_FAULT_1, INPUT);
    pinMode(GPIO_FAULT_2, INPUT);
}

void checkMotorDriverFault() {
    int fault1 = digitalRead(GPIO_FAULT_1);
    int fault2 = digitalRead(GPIO_FAULT_2);
    #ifdef DEBUG_OUTPUT
    if (fault1 == LOW) {
        Console.println("Motor1 Driver Fault");
    } else if (fault2 == LOW) {
        Console.println("Motor2 Driver Fault");
    } else {
        Console.println("OK");
    }
    #endif
}

// Function to set the color of a specific LED
void setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led < 0 || led >= NUM_LEDS) {
        printf("LED index out of range: %d\n", led);
        return;
    }

    uint32_t led_data = (green << 16) | (red << 8) | blue; // GRB format
    int base = led * BITS_PER_LED_CMD;

    for (int bit = 0; bit < BITS_PER_LED_CMD; bit++) {
        if (led_data & (1 << (23 - bit))) {
            // Logical '1'
            rmt_items[base + bit].duration0 = T1H;
            rmt_items[base + bit].level0 = 1;
            rmt_items[base + bit].duration1 = T1L;
            rmt_items[base + bit].level1 = 0;
        } else {
            // Logical '0'
            rmt_items[base + bit].duration0 = T0H;
            rmt_items[base + bit].level0 = 1;
            rmt_items[base + bit].duration1 = T0L;
            rmt_items[base + bit].level1 = 0;
        }
    }
}

// Function to update all LEDs with the current RMT buffer
void showPixels() {
    // Send RMT items
    esp_err_t ret = rmt_write_items(RMT_TX_CHANNEL, rmt_items, TOTAL_BITS, true);
    if (ret != ESP_OK) {
        printf("RMT write failed: %d\n", ret);
    }
    
    // Wait for transmission to complete
    ret = rmt_wait_tx_done(RMT_TX_CHANNEL, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        printf("RMT wait failed: %d\n", ret);
    }
}

// Function to initialize the RMT peripheral for WS2812
void initWS2812() {
    // Configure RMT
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX((gpio_num_t)LED_PIN, RMT_TX_CHANNEL);
    config.clk_div = 2; // 40MHz clock (assuming APB clock is 80MHz)
    config.mem_block_num = 1; // Allocate one memory block
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    
    // Initialize RMT
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
    
    // Clear RMT buffer
    memset(rmt_items, 0, sizeof(rmt_items));
}

void controlMotor(Motor* motor, int pwmValue) {
    // Ensure PWM value is within -1023 to +1023
    pwmValue = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    if (pwmValue >= 0) {
        // Forward direction
        ledcWrite(motor->channel_forward, pwmValue);        // Set forward speed
        ledcWrite(motor->channel_reverse, 0);               // Reverse off
    } else {
        // Reverse direction
        ledcWrite(motor->channel_forward, 0);               // Forward off
        ledcWrite(motor->channel_reverse, -pwmValue);       // Set reverse speed
    }
    //Console.println(pwmValue);
}

// Neue Hilfsfunktion zur Skalierung der PWM mit Mindestwert
int scaleMovementToPWM(float movement) {
    if (movement > 0.0) {
        return 110 + (int)(movement * (255 - 110));
    } else if (movement < 0.0) {
        return -110 - (int)(abs(movement) * (255 - 110));
    } else {
        return 0;
    }
}

void handleMotorControl(int axisX, int axisY, Motor* leftMotor, Motor* rightMotor) {
       // Totzone anwenden
    int normX = (abs(axisX) < DEAD_BAND) ? 0 : axisX;
    int normY = (abs(axisY) < DEAD_BAND) ? 0 : axisY;

    // Berechnung der Bewegungsproportionen
    float maxMovement = (float)(MAX_JOYSTICK_VALUE - DEAD_BAND);
    float movementLeft = (float)(normY + normX) / maxMovement;
    float movementRight = (float)(normY - normX) / maxMovement;

    // PWM-Werte skalieren mit Mindestwert
    int rightPWM = scaleMovementToPWM(movementLeft);
    int leftPWM = scaleMovementToPWM(movementRight);

    // PWM-Werte begrenzen
    leftPWM = constrain(leftPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    rightPWM = constrain(rightPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    // Kontrolliere die Motoren
    controlMotor(leftMotor, leftPWM);
    controlMotor(rightMotor, rightPWM);
    #ifdef DEBUG_OUTPUT
    Console.println(rightPWM);
    Console.println(leftPWM);
    #endif
}

void processButtons(ControllerPtr ctl) {
    unsigned long buttonState = ctl->buttons();
    if (buttonState) {
        if (buttonState & BTN_B) {
            Console.println("Button B pressed");
            for (int a = 1; a-19;a++) {
                setPixelColor(a, 120, 120, 0);
            }
            showPixels();
        }
        if (buttonState & BTN_A) {
            Console.println("Button A pressed");
            for (int a = 1; a-19;a++) {
                setPixelColor(a, 0, 120, 0);
            }
            showPixels();
        }
        if (buttonState & BTN_Y) {
            Console.println("Button Y pressed");
            for (int a = 1; a-19;a++) {
                setPixelColor(a, 0, 120, 120);
            }
            showPixels();
        }
        if (buttonState & BTN_X) {
            Console.println("Button X pressed");
            for (int a = 1; a-19;a++) {
                setPixelColor(a, 0, 0, 120);
            }
            showPixels();
        }
        if (buttonState & BUTTON_L1) {
            Console.println("Button L1 pressed");
        }
        if (buttonState & BUTTON_R1) {
            if (millis() - timestampServo < 1000) {
                return;
            }
            timestampServo = millis();
            Console.println("Button R1 pressed");
            setServoAngle(&servos[0], 150);
            Console.println(servos[0].angle);
            delay(100);
            setServoAngle(&servos[0], 180);
        }
        if (buttonState & BUTTON_L2) {
            Console.println("Button L2 pressed");
        }
        if (buttonState & BUTTON_R2) {
            Console.println("Button R2 pressed");
        }
        if (buttonState & BUTTON_STICK_L) {
            Console.println("Stick Left pressed");
        }
        if (buttonState & BUTTON_STICK_R) {
            Console.println("Stick Right pressed");
        }
    }
    unsigned long dpadState = ctl->dpad();
    if (dpadState) {
        if (dpadState & static_cast<unsigned long>(DPad::UP)) {
            Console.println("D-pad Up pressed");
            setServoAngle(&servos[0], servos[0].angle + 10);
            Console.println(servos[0].angle);
        }
        if (dpadState & static_cast<unsigned long>(DPad::DOWN)) {
            Console.println("D-pad Down pressed");
            setServoAngle(&servos[0], servos[0].angle - 10);
            Console.println(servos[0].angle);
        }
        if (dpadState & static_cast<unsigned long>(DPad::RIGHT)) {
            Console.println("D-pad Right pressed");
            setServoAngle(&servos[1], servos[1].angle + 10);
            Console.println(servos[1].angle);
        }
        if (dpadState & static_cast<unsigned long>(DPad::LEFT)) {
            Console.println("D-pad Left pressed");
            setServoAngle(&servos[1], servos[1].angle - 10);
            Console.println(servos[1].angle);
        }
    }
    unsigned long miscState = ctl->miscButtons();
    if (miscState) {
        if (miscState & static_cast<unsigned long>(MiscButtons::HOME)) {
            Console.println("Home button pressed");
            JoyLxNeutral = ctl->axisX();
            JoyLyNeutral = ctl->axisY();
            JoyRxNeutral = ctl->axisRX();
            JoyRyNeutral = ctl->axisRY();
        }
        if (miscState & static_cast<unsigned long>(MiscButtons::MINUS)) {
            Console.println("Minus button pressed");
            JoyLxNeutral = ctl->axisX();
            JoyLyNeutral = ctl->axisY();
            JoyRxNeutral = ctl->axisRX();
            JoyRyNeutral = ctl->axisRY();
        }
        if (miscState & static_cast<unsigned long>(MiscButtons::PLUS)) {
            Console.println("Plus button pressed");
        }
        if (miscState & static_cast<unsigned long>(MiscButtons::DOT)) {
            Console.println("Dot button pressed");
        }
    }
}

void processGamepad(ControllerPtr ctl) {
    #ifdef DEBUG_OUTPUT
    // Ausgabe der Achsenwerte mit Offset und berechnetem Ausgabewert
Console.println("----------------------");

// Achse RX
int realRX = ctl->axisRX();
Console.print("Real Wert RX: ");
Console.print(realRX);
Console.print(" | Offset ist ");
Console.println(JoyRxNeutral);
Console.print("Ausgabewert RX: ");
Console.println(realRX - JoyRxNeutral);

// Achse RY
int realRY = ctl->axisRY();
Console.print("Real Wert RY: ");
Console.print(realRY);
Console.print(" | Offset ist ");
Console.println(JoyRyNeutral);
Console.print("Ausgabewert RY: ");
Console.println(realRY - JoyRyNeutral);

// Achse X
int realX = ctl->axisX();
Console.print("Real Wert X: ");
Console.print(realX);
Console.print(" | Offset ist ");
Console.println(JoyLxNeutral);
Console.print("Ausgabewert X: ");
Console.println(realX - JoyLxNeutral);

// Achse Y
int realY = ctl->axisY();
Console.print("Real Wert Y: ");
Console.print(realY);
Console.print(" | Offset ist ");
Console.println(JoyLyNeutral);
Console.print("Ausgabewert Y: ");
Console.println(realY - JoyLyNeutral);

Console.println("----------------------");
#endif
    if (JoyLxNeutral == -1) {
        JoyLxNeutral = ctl->axisX();
    }
    if (JoyLyNeutral == -1) {
        JoyLyNeutral = ctl->axisY();
    }
    if (JoyRxNeutral == -1) {
        JoyRxNeutral = ctl->axisRX();
    }
    if (JoyRyNeutral == -1) {
        JoyRyNeutral = ctl->axisRY();
    }
    handleMotorControl(
        ctl->axisRX() - JoyRxNeutral,
        ctl->axisRY() - JoyRyNeutral,
        &motors[0],  // Left motor (Motor 1)
        &motors[1]   // Right motor (Motor 2)
    );
    handleMotorControl(
        ctl->axisX() - JoyLxNeutral,
        ctl->axisY() - JoyLyNeutral,
        &motors[2],  // Left motor (Motor 3)
        &motors[3]   // Right motor (Motor 4)
    );
    processButtons(ctl);
    float voltage = readBatteryVoltage();
    if (voltage > 4) {
        ctl->setPlayerLEDs(13 & 0x0f);
    } else if (voltage > 3.8) {
        ctl->setPlayerLEDs(7 & 0x0f);
    } else if (voltage > 3.5) {
        ctl->setPlayerLEDs(0 & 0x0f);
    } else {
        ctl->setPlayerLEDs(0 & 0x0f);
    }
}

void processMouse(ControllerPtr ctl) {
    if (ctl->scrollWheel() > 0) {
        // Do Something
    } else if (ctl->scrollWheel() < 0) {
        // Do something else
    }
}

void processKeyboard(ControllerPtr ctl) {
    if (!ctl->isAnyKeyPressed())
        return;
    if (ctl->isKeyPressed(Keyboard_A)) {
        Console.println("Key 'A' pressed");
    }
    if (ctl->isKeyPressed(Keyboard_LeftShift)) {
        Console.println("Key 'LEFT SHIFT' pressed");
    }
    if (ctl->isKeyPressed(Keyboard_LeftArrow)) {
        Console.println("Key 'Left Arrow' pressed");
    }
}

void processBalanceBoard(ControllerPtr ctl) {
    if (ctl->topLeft() > 10000) {
        // Do Something
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

void onConnectedController(ControllerPtr ctl) {
    setPixelColor(0, 0, 120, 0);
    showPixels();
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Console.printf("CALLBACK: Controller is connected, index=%d\n", i);
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Console.println("CALLBACK: Controller connected, but could not find empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    setPixelColor(0, 180, 0, 0);
    showPixels();
    controlMotor(&motors[0], 0);
    controlMotor(&motors[1], 0);
    controlMotor(&motors[2], 0);
    controlMotor(&motors[3], 0);
    bool foundController = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Console.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }
    if (!foundController) {
        Console.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void setup() {
    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);
    initMotors();
    initServos();
    initGPIOFaultCheck();
    initWS2812();
    initADC();
    Console.println("Pins Initialized");
    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
    // 98:B6:E9:01:6C:47
    // Whitlist
    uni_bt_allowlist_init();
    uni_bt_allowlist_remove_all();

    // Füge die erlaubten MAC-Adressen zur Allowlist hinzu
    /*for (int i = 0; i < numAllowed; ++i) {
        bd_addr_t addr;
        sscanf_bd_addr(allowedAddresses[i], addr);
        uni_bt_allowlist_add_addr(addr);
    }*/

    // Aktiviere die Whitelist-Überprüfung
    uni_bt_allowlist_set_enabled(false);

    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
}

void loop() {
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();
    vTaskDelay(10);
}
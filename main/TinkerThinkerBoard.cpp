#include "TinkerThinkerBoard.h"
#include <Arduino.h>

// Pins wie gehabt
#define POWER_ON_PIN 26
#define GPIO_FAULT_1 39
#define GPIO_CURRENT_1 34
#define GPIO_CURRENT_2 36
#define BATTERY_PIN 35

TinkerThinkerBoard::TinkerThinkerBoard(ConfigManager* configManager)
: config(configManager)
{
    motors[0] = {16, 25, 0, 1};
    motors[1] = {32, 27, 2, 3};
    motors[2] = {4, 12, 4, 5};
    motors[3] = {15, 14, 6, 7};

    servos[0] = {13, 8, 90, 500, 2500};
    servos[1] = {33, 9, 90, 500, 2500};
    servos[2] = {17, 10, 90, 500, 2500};
}

void TinkerThinkerBoard::reApplyConfig() {
    // Arrays für Frequenzen, Deadbands und Invert erzeugen
    int freqs[MOTOR_COUNT];
    int dbs[MOTOR_COUNT];
    bool inv[MOTOR_COUNT];

    for (int i = 0; i < MOTOR_COUNT; i++) {
        freqs[i] = config->getMotorFrequency(i);
        dbs[i] = config->getMotorDeadband(i);
        inv[i] = config->getMotorInvert(i);
    }

    motorController = new MotorController(motors, MOTOR_COUNT, freqs, dbs, inv, config->getMotorSwap());
    motorController->init();

    servoController = new ServoController(servos, SERVO_COUNT);
    for (int i = 0; i < SERVO_COUNT; i++) {
        servoController->setPulseWidthRange(i, config->getServoMinPulsewidth(i), config->getServoMaxPulsewidth(i));
    }
    servoController->init();

    ledController = new LEDController(config->getLedCount());
    ledController->init();
    for (int i = 0; i < config->getLedCount(); i++) {
        ledController->setPixelColor(i, 50, 150, 80);
    }
    FastLED.show();

    batteryMonitor = new BatteryMonitor(BATTERY_PIN);
    batteryMonitor->init();

    systemMonitor = new SystemMonitor(GPIO_FAULT_1, GPIO_CURRENT_1, GPIO_CURRENT_2);
    systemMonitor->init();
    return;
}

void TinkerThinkerBoard::begin() {
    config->init();
    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);

    reApplyConfig() ;   

    webServerManager = new WebServerManager(this, config);
    webServerManager->init();

    xTaskCreate([](void* obj) {
        TinkerThinkerBoard* board = (TinkerThinkerBoard*)obj;
        for (;;) {
            board->updateWebClients();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }, "WebClientTask", 4096, this, 1, NULL);

    Serial.println("TinkerThinkerBoard initialized.");
}

void TinkerThinkerBoard::controlMotors(int axisX, int axisY) {
    motorController->handleMotorControl(axisX, axisY, motorLeftGUI, motorRightGUI);
}

void TinkerThinkerBoard::controlMotorForward(int motorIndex) {
    motorController->controlMotorForward(motorIndex);
}

void TinkerThinkerBoard::controlMotorBackward(int motorIndex) {
    motorController->controlMotorBackward(motorIndex);
}

void TinkerThinkerBoard::controlMotorStop(int motorIndex) {
    motorController->controlMotorStop(motorIndex);
}

void TinkerThinkerBoard::setMotorLeftGUI(int motorIndex) {
    motorLeftGUI = motorIndex;
}

void TinkerThinkerBoard::setMotorRightGUI(int motorIndex) {
    motorRightGUI = motorIndex;
}

void TinkerThinkerBoard::controlMotorDirect(int motorIndex, int pwmValue) {
    if (motorIndex < 0 || motorIndex >= 4) return;
    motorController->controlMotor(motorIndex, pwmValue);
}

void TinkerThinkerBoard::setServoAngle(int servoIndex, int angle) {
    servoController->setServoAngle(servoIndex, angle);
}

int TinkerThinkerBoard::getServoAngle(int servoIndex) {
    return servoController->getServoAngle(servoIndex);
}

void TinkerThinkerBoard::setLED(int led, uint8_t r, uint8_t g, uint8_t b) {
    ledController->setPixelColor(led, r, g, b);
}

void TinkerThinkerBoard::showLEDs() {
    ledController->showPixels();
}

CRGB TinkerThinkerBoard::getLEDColor(int ledIndex) {
    return ledController->getLEDColor(ledIndex);
}

float TinkerThinkerBoard::getBatteryVoltage() {
    return batteryMonitor->readVoltage();
}

float TinkerThinkerBoard::getBatteryPercentage() {
    return batteryMonitor->readPercentage();
}

void TinkerThinkerBoard::checkFaults() {
    systemMonitor->checkMotorDriverFault();
}

bool TinkerThinkerBoard::isMotorInFault() {
    return systemMonitor->isMotorInFault();
}

float TinkerThinkerBoard::getHBridgeAmps(int motorIndex) {
    return systemMonitor->getHBridgeAmps(motorIndex);
}

int TinkerThinkerBoard::getMotorPWM(int motorIndex) {
    return motorController->getMotorPWM(motorIndex);
}

void TinkerThinkerBoard::updateWebClients() {
    webServerManager->sendStatusUpdate();
}

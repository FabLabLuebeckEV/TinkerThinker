#include "MyCustomBoard.h"

// Pin-Konfigurationen
#define POWER_ON_PIN 26
#define GPIO_FAULT_1 39
#define GPIO_CURRENT_1 34
#define GPIO_CURRENT_2 36
#define LED_PIN 2
#define BATTERY_PIN 35 // Beispielkanal, anpassen

MyCustomBoard::MyCustomBoard() {
    // Initialisiere Motoren
    motors[0] = {16, 25, 0, 1};
    motors[1] = {32, 27, 2, 3};
    motors[2] = {4, 12, 4, 5};
    motors[3] = {15, 14, 6, 7};
    
    // Initialisiere Servos
    servos[0] = {13, 8, 90};
    servos[1] = {33, 9, 90};
    servos[2] = {17, 10, 90};
    
    // Initialisiere Komponenten
    motorController = new MotorController(motors, MOTOR_COUNT);
    servoController = new ServoController(servos, SERVO_COUNT);
    ledController = new LEDController();
    batteryMonitor = new BatteryMonitor(BATTERY_PIN);
    systemMonitor = new SystemMonitor(GPIO_FAULT_1, GPIO_CURRENT_1, GPIO_CURRENT_2);
    webServerManager = new WebServerManager(this, "MyBoardAP", "12345678");
}

void MyCustomBoard::begin() {
    Serial.begin(115200);
    #if DEBUG_OUTPUT
    Serial.println("Initializing MyCustomBoard...");
    #endif

    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH); // Aktivieren der Spannungswandler

    motorController->init();
    servoController->init();
    ledController->init();
    batteryMonitor->init();
    systemMonitor->init();
    // Webserver starten
    webServerManager->init();
    // erstelle einen Task, der updateWebClients() alle 1000ms aufruft
    xTaskCreate([](void* obj) {
        MyCustomBoard* board = (MyCustomBoard*)obj;
        for (;;) {
            board->updateWebClients();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }, "WebClientTask", 4096, this, 1, NULL);
}

void MyCustomBoard::controlMotors(int axisX, int axisY) {
    motorController->handleMotorControl(axisX, axisY, 0, 1); // Beispiel: Motor 0 und 1
}


void MyCustomBoard::controlMotorForward(int motorIndex) {
    motorController->controlMotorForward(motorIndex);
}

void MyCustomBoard::controlMotorBackward(int motorIndex) {
    motorController->controlMotorBackward(motorIndex);
}

void MyCustomBoard::controlMotorStop(int motorIndex) {
    motorController->controlMotorStop(motorIndex);
}


void MyCustomBoard::setServoAngle(int servoIndex, int angle) {
    servoController->setServoAngle(servoIndex, angle);
}

int MyCustomBoard::getServoAngle(int servoIndex) {
    return servoController->getServoAngle(servoIndex);
}

void MyCustomBoard::setLED(int led, uint8_t r, uint8_t g, uint8_t b) {
    ledController->setPixelColor(led, r, g, b);
}

void MyCustomBoard::showLEDs() {
    ledController->showPixels();
}

float MyCustomBoard::getBatteryVoltage() {
    return batteryMonitor->readVoltage();
}

float MyCustomBoard::getBatteryPercentage() {
    return batteryMonitor->readPercentage();
}

void MyCustomBoard::checkFaults() {
    systemMonitor->checkMotorDriverFault();
}

void MyCustomBoard::updateWebClients() {
    // Diese Funktion kannst du in deiner main loop alle 1000ms aufrufen, z. B. via millis().
    webServerManager->sendStatusUpdate();
}

CRGB MyCustomBoard::getLEDColor(int ledIndex) {
    return ledController->getLEDColor(ledIndex);
}

int MyCustomBoard::getMotorPWM(int motorIndex) {
    return motorController->getMotorPWM(motorIndex);
}

bool MyCustomBoard::isMotorInFault() {
    return systemMonitor->isMotorInFault();
}

float MyCustomBoard::getHBridgeAmps(int motorIndex) {
    return systemMonitor->getHBridgeAmps(motorIndex);
}
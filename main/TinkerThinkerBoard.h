#ifndef TINKER_THINKER_BOARD_H
#define TINKER_THINKER_BOARD_H

#include "MotorController.h"
#include "ServoController.h"
#include "LEDController.h"
#include "BatteryMonitor.h"
#include "SystemMonitor.h"
#include "WebServerManager.h"
#include "ConfigManager.h"

class TinkerThinkerBoard {
public:
    TinkerThinkerBoard(ConfigManager* configManager);
    void begin();
    void reApplyConfig();

    // Motorsteuerung
    void controlMotors(int axisX, int axisY);
    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    void controlMotorDirect(int motorIndex, int pwmValue);
    void setMotorLeftGUI(int motorIndex);
    void setMotorRightGUI(int motorIndex);

    // Servo-Steuerung
    void setServoAngle(int servoIndex, int angle);
    int getServoAngle(int servoIndex);

    // LED-Steuerung
    void setLED(int led, uint8_t r, uint8_t g, uint8_t b);
    void showLEDs();
    CRGB getLEDColor(int ledIndex);

    // Batteriemessung
    float getBatteryVoltage();
    float getBatteryPercentage();

    // Systemüberwachung
    void checkFaults();
    bool isMotorInFault();
    float getHBridgeAmps(int motorIndex);

    // Webserver-Update
    void updateWebClients();

    int getMotorPWM(int motorIndex);

private:
    ConfigManager* config;
    MotorController* motorController;
    ServoController* servoController;
    LEDController* ledController;
    BatteryMonitor* batteryMonitor;
    SystemMonitor* systemMonitor;
    WebServerManager* webServerManager;

    // Werte aus Config lesen
    static const size_t MOTOR_COUNT = 4;
    static const size_t SERVO_COUNT = 3;
    Motor motors[MOTOR_COUNT];
    ServoMotor servos[SERVO_COUNT];
    int motorLeftGUI = 2;
    int motorRightGUI = 3;
};

#endif

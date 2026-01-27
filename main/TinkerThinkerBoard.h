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

    // Motorsteuerung (legacy direct)
    void controlMotors(int axisX, int axisY);
    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    void controlMotorDirect(int motorIndex, int pwmValue);

    // Source-aware control API
    void requestDriveFromBT(int axisX, int axisY);
    void requestDriveFromWS(int axisX, int axisY);
    void requestDriveOtherFromBT(int axisX, int axisY);
    void requestDriveOtherFromWS(int axisX, int axisY);
    void requestMotorDirectFromBT(int motorIndex, int pwmValue);
    void requestMotorDirectFromWS(int motorIndex, int pwmValue);
    void requestMotorStopFromWS(int motorIndex);
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

    float getHBridgeAmps(int motorIndex);

    // Webserver-Update
    void updateWebClients();
    void requestWifiDisable(bool untilRestart);
    void requestWifiEnable();

    int getMotorPWM(int motorIndex);
    void setSpeedMultiplier(float m);
    float getSpeedMultiplier();

private:
    enum class ControlSource { None, Bluetooth, WebSocket };
    ControlSource lastSource = ControlSource::None;
    uint32_t lastActiveMs = 0;
    const int neutralThreshold = 16; // axis deadband for arbitration

    bool isNeutralAxes(int x, int y) {
        return (abs(x) <= neutralThreshold && abs(y) <= neutralThreshold);
    }
    bool shouldAccept(ControlSource src, bool isActive);
    void applyDrive(int axisX, int axisY);
    void applyDriveOther(int axisX, int axisY);
    void getOtherPair(int &leftIdx, int &rightIdx);
    void takeOwnership(ControlSource src);

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

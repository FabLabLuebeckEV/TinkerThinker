#ifndef TINKER_THINKER_BOARD_H
#define TINKER_THINKER_BOARD_H

#include "MotorController.h"
#include "ServoController.h"
#include "LEDController.h"
#include "BatteryMonitor.h"
#include "SystemMonitor.h"
#include "WebServerManager.h"
#include "ConfigManager.h"
#include <functional>
#include <Bluepad32.h>

struct ControllerInputSnapshot {
    bool     connected = false;
    uint32_t buttons   = 0;
    uint8_t  dpad      = 0;
    int16_t  axisX = 0, axisY = 0, axisRX = 0, axisRY = 0;
    uint32_t updatedMs = 0;
};

class TinkerThinkerBoard {
public:
    TinkerThinkerBoard(ConfigManager* configManager);
    void begin();
    void startServices();
    void reApplyConfig();

    // Motorsteuerung (legacy direct)
    void controlMotors(int axisX, int axisY);
    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    void controlMotorDirect(int motorIndex, int pwmValue);
    void controlMotorRaw(int motorIndex, int pwmValue);

    // Source-aware control API
    void requestDriveFromBT(int axisX, int axisY, bool swapSides = false);
    void requestDriveFromWS(int axisX, int axisY, bool swapSides = false);
    void requestDriveOtherFromBT(int axisX, int axisY, bool swapSides = false);
    void requestDriveOtherFromWS(int axisX, int axisY, bool swapSides = false);
    void requestMotorDirectFromBT(int motorIndex, int pwmValue);
    void requestMotorDirectFromWS(int motorIndex, int pwmValue);
    void requestMotorStopFromWS(int motorIndex);
    void setMotorLeftGUI(int motorIndex);
    void setMotorRightGUI(int motorIndex);

    // Servo-Steuerung
    void setServoAngle(int servoIndex, int angle);
    int getServoAngle(int servoIndex);
    size_t getActiveServoCount() const;

    // SchrottBot-Sumo (gui_mode "sumo"): Positionen in Prozent 0..100
    //   SUMO_ARMS: beide Arm-Servos (0 = zu, 100 = auf; der rechte läuft über seinen Winkelbereich gegenläufig)
    //   SUMO_LIFT: Rampe heben (0 = unten), SUMO_PUSH: Rampe vorschieben (0 = hinten)
    enum SumoRole { SUMO_ARMS = 0, SUMO_LIFT = 1, SUMO_PUSH = 2, SUMO_ROLES = 3 };
    void applySumoConfig(const String& json);
    String getSumoConfigJson() const;
    void setSumoPos(int role, float pct);
    void nudgeSumo(int role, float deltaPct);
    float getSumoPos(int role);

    // LED-Steuerung
    void setLED(int led, uint8_t r, uint8_t g, uint8_t b);
    void showLEDs();
    CRGB getLEDColor(int ledIndex);
    void setLedBrightness(uint8_t value);
    void setLedGamma(bool enabled);

    // Batteriemessung
    float getBatteryVoltage();
    float getBatteryPercentage();

    void checkFaults();
    bool isMotorInFault();
    float getHBridgeAmps(int motorIndex);
    // Zuletzt gemessener Strom (Sensor 0 = Motor B, Sensor 1 = Motor A), ohne neu zu messen
    float getLastHBridgeAmps(int sensorIndex) const;
    // Strom messen und Blockierschutz auswerten, alle 100 ms aus dem Status-Task
    void monitorMotors();
    bool isMotorBlocked(int motorIndex);

    // Webserver-Update
    void updateWebClients();
    void requestWifiDisable(bool untilRestart);
    void requestWifiEnable();
    bool isWifiDisabledUntilRestart();

    int getMotorPWM(int motorIndex);
    int getMotorRequest(int motorIndex);
    // Aus der Loop aufrufen: beendet abgelaufene Anlauf-Boosts der Motoren
    void updateMotors();
    void setSpeedMultiplier(float m);
    float getSpeedMultiplier();

    void notifyControllerConnected(int slot, const char* mac, const char* model);
    void notifyControllerDisconnected(int slot);
    void updateControllerSnapshot(int idx, ControllerPtr ctl);
    void clearControllerSnapshot(int idx);
    const ControllerInputSnapshot& getControllerSnapshot(int idx) const;
    void setWhitelistApplyCallback(std::function<void()> cb);

private:
    enum class ControlSource { None, Bluetooth, WebSocket };
    ControlSource lastSource = ControlSource::None;
    uint32_t lastActiveMs = 0;
    volatile float lastAmps[2] = {0.0f, 0.0f};
    // Akku: nur monitorMotors() misst (alle 500 ms), alle anderen lesen den gespeicherten Wert
    volatile float lastBatteryVoltage = 0.0f;
    volatile float lastBatteryPercent = 0.0f;
    uint32_t lastBatteryReadMs = 0;
    uint32_t stallSinceMs[2] = {0, 0};
    const int neutralThreshold = 16; // axis deadband for arbitration

    bool isNeutralAxes(int x, int y) {
        return (abs(x) <= neutralThreshold && abs(y) <= neutralThreshold);
    }
    bool shouldAccept(ControlSource src, bool isActive);
    void applyDrive(int axisX, int axisY, bool swapSides = false);
    void applyDriveOther(int axisX, int axisY, bool swapSides = false);
    void getOtherPair(int &leftIdx, int &rightIdx);
    void takeOwnership(ControlSource src);

    ConfigManager* config;
    MotorController* motorController = nullptr;
    ServoController* servoController = nullptr;
    LEDController* ledController = nullptr;
    BatteryMonitor* batteryMonitor = nullptr;
    SystemMonitor* systemMonitor = nullptr;
    WebServerManager* webServerManager = nullptr;

    // Werte aus Config lesen
    static const size_t MOTOR_COUNT = 4;
    static const size_t SERVO_COUNT = 7;
    Motor motors[MOTOR_COUNT];
    ServoMotor servos[SERVO_COUNT];
    int motorLeftGUI = 2;
    int motorRightGUI = 3;
    ControllerInputSnapshot controllerSnapshots[BP32_MAX_GAMEPADS];

    // Sumo: Servo-Index (0-basiert) und Winkel bei 0 % (a0) und 100 % (a1)
    // Reihenfolge: arm_l, arm_r, lift, push; Standard Servo 4..7, rechter Arm gegenläufig
    struct SumoServo { int servo; int a0; int a1; };
    SumoServo sumoServo[4] = {{3, 0, 180}, {4, 180, 0}, {5, 0, 180}, {6, 0, 180}};
    float sumoPos[SUMO_ROLES] = {NAN, NAN, NAN};
    portMUX_TYPE sumoMux = portMUX_INITIALIZER_UNLOCKED;
    int sumoAngle(int slot, float pct) const;
};

#endif

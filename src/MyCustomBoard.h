#ifndef MY_CUSTOM_BOARD_H
#define MY_CUSTOM_BOARD_H

#include "MotorController.h"
#include "ServoController.h"
#include "LEDController.h"
#include "BatteryMonitor.h"
#include "SystemMonitor.h"
#include "WebServerManager.h"

class MyCustomBoard {
public:
    MyCustomBoard();
    void begin();

    // Motorsteuerung
    void controlMotors(int axisX, int axisY);
    
    // Neue Methoden für Motor C
    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    
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
    
    // Motor Status
    int getMotorPWM(int motorIndex);
    
private:
    MotorController* motorController;
    ServoController* servoController;
    LEDController* ledController;
    BatteryMonitor* batteryMonitor;
    SystemMonitor* systemMonitor;
    WebServerManager* webServerManager;
    
    // Konfiguration
    static const size_t MOTOR_COUNT = 4;
    static const size_t SERVO_COUNT = 3;
    Motor motors[MOTOR_COUNT];
    ServoMotor servos[SERVO_COUNT];
    static const size_t LED_COUNT = 30; // NUM_LEDS definiert in LEDController
};

#endif

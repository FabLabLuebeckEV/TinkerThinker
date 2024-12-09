#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

struct Motor {
    int pin1;
    int pin2;
    int channel_forward;
    int channel_reverse;
};

class MotorController {
public:
    MotorController(Motor motors[], size_t motorCount);
    void init();
    void controlMotor(int index, int pwmValue);
    void handleMotorControl(int axisX, int axisY, int leftMotorIndex, int rightMotorIndex);
    
    // Neue Methoden für Motor C Steuerung
    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    
    // Statusabfragen
    int getMotorPWM(int motorIndex);
    
private:
    Motor* motors;
    size_t count;
    int scaleMovementToPWM(float movement);
};

#endif

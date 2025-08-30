// MotorController.h
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
    // Neuer Konstruktor erwartet Arrays für Frequenz und Deadband
    MotorController(Motor motors[], size_t motorCount, const int motorFrequencies[], const int motorDeadbands[], const bool motorInvert[], bool motorSwap);
    void init();
    void controlMotor(int index, int pwmValue);
    void handleMotorControl(int axisX, int axisY, int leftMotorIndex, int rightMotorIndex);

    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    int getMotorPWM(int motorIndex);
    void setSpeedMultiplier(float m);
    float getSpeedMultiplier() const { return speedMultiplier; }

private:
    Motor* motors;
    size_t count;
    bool motorInvertArray[4];
    bool motorSwap;

    int freq[4];
    int deadband[4];
    float speedMultiplier = 1.0f;

    int scaleMovementToPWM(float movement);
};

#endif

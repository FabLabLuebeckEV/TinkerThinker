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
    struct DriveProfileConfig {
        enum class Mixer : uint8_t { Arcade, Tank };
        Mixer mixer = Mixer::Arcade;
        int axisDeadband = 16;
        float turnGain = 1.0f;
    };

    struct MotorCurveConfig {
        enum class Type : uint8_t { Linear, Expo };
        Type type = Type::Linear;
        float strength = 0.0f;
    };

    MotorController(Motor motors[], size_t motorCount, const int motorFrequencies[], const int motorDeadbands[], const bool motorInvert[], bool motorSwap,
                    const DriveProfileConfig& driveProfile, const MotorCurveConfig& motorCurve);
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

    DriveProfileConfig driveProfile;
    MotorCurveConfig motorCurve;

    int scaleMovementToPWM(float movement);
    float normalizeAxis(int raw) const;
    float applyAxisDeadband(float value) const;
    void mixArcade(float throttle, float turn, float& left, float& right) const;
    float applyMotorCurve(float value) const;
};

#endif

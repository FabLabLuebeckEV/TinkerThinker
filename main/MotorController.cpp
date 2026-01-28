// MotorController.cpp
#include <Arduino.h>
#include <math.h>
#include "MotorController.h"

#define MAX_PWM_VALUE 255
#define MAX_JOYSTICK_VALUE 512

MotorController::MotorController(Motor motors[], size_t motorCount, const int motorFrequencies[], const int motorDeadbands[], const bool motorInvertArr[], bool motorSwap,
                                 const DriveProfileConfig& driveCfg, const MotorCurveConfig& curveCfg)
: motors(motors), count(motorCount), motorSwap(motorSwap), driveProfile(driveCfg), motorCurve(curveCfg)
{
    for (int i = 0; i < (int)count; i++) {
        freq[i] = motorFrequencies[i];
        deadband[i] = motorDeadbands[i];
        motorInvertArray[i] = motorInvertArr[i];
    }
}

void MotorController::init() {
    constexpr uint32_t PWM_FREQ = 1000;   // 5 kHz
    constexpr uint8_t  PWM_BITS = 8;      // 8 Bit Auflösung

    for (size_t i = 0; i < count; ++i) {
        pinMode(motors[i].pin1, OUTPUT);
        pinMode(motors[i].pin2, OUTPUT);

        // ersetzt ledcSetup + ledcAttachPin
        ledcAttachChannel(motors[i].pin1, PWM_FREQ, PWM_BITS,
                          motors[i].channel_forward);
        ledcAttachChannel(motors[i].pin2, PWM_FREQ, PWM_BITS,
                          motors[i].channel_reverse);
    }
}


void MotorController::controlMotor(int index, int pwmValue) {
    if (index >= (int)count) return;

    // Apply per-motor invert first
    if (motorInvertArray[index]) pwmValue = -pwmValue;

    // Clamp incoming request to valid range
    pwmValue = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    // Compute output magnitude with per-motor deadband once
    int db = constrain(deadband[index], 0, MAX_PWM_VALUE);
    int inMag = abs(pwmValue);
    int outMag = 0;
    if (inMag > 0) {
        // Map 1..255 → db..255 linearly (0 stays 0)
        outMag = db + (inMag * (MAX_PWM_VALUE - db)) / MAX_PWM_VALUE;
    }

    // Apply global speed multiplier
    outMag = (int)(outMag * speedMultiplier);
    outMag = constrain(outMag, 0, MAX_PWM_VALUE);

    if (pwmValue >= 0) {
        ledcWrite(motors[index].pin1, outMag);
        ledcWrite(motors[index].pin2, 0);
    } else {
        ledcWrite(motors[index].pin1, 0);
        ledcWrite(motors[index].pin2, outMag);
    }
}


int MotorController::scaleMovementToPWM(float movement) {
    if (movement > 1.0f) movement = 1.0f;
    if (movement < -1.0f) movement = -1.0f;
    float curved = applyMotorCurve(movement);
    return (int)roundf(curved * MAX_PWM_VALUE);
}

void MotorController::handleMotorControl(int axisX, int axisY, int leftMotorIndex, int rightMotorIndex) {
    float turn = applyAxisDeadband(normalizeAxis(axisX));
    float throttle = applyAxisDeadband(normalizeAxis(axisY));

    float movementLeft = 0.0f;
    float movementRight = 0.0f;

    switch (driveProfile.mixer) {
        case DriveProfileConfig::Mixer::Arcade: {
            mixArcade(throttle, turn, movementLeft, movementRight);
            break;
        }
        case DriveProfileConfig::Mixer::Tank: {
            // Interpret axisY as left stick (throttle) and axisX as right stick by convention
            movementLeft = throttle;
            movementRight = turn;
            break;
        }
    }

    const float outputDeadband = driveProfile.axisDeadband / (float)MAX_JOYSTICK_VALUE;
    if (fabsf(movementLeft) < outputDeadband) movementLeft = 0.0f;
    if (fabsf(movementRight) < outputDeadband) movementRight = 0.0f;

    int pwmLeft = scaleMovementToPWM(movementLeft);
    int pwmRight = scaleMovementToPWM(movementRight);

    int motorLeft = motorSwap ? rightMotorIndex : leftMotorIndex;
    int motorRight = motorSwap ? leftMotorIndex : rightMotorIndex;

    controlMotor(motorLeft, pwmLeft);
    controlMotor(motorRight, pwmRight);
}

void MotorController::controlMotorForward(int motorIndex) {
    if (motorIndex >= (int)count) return;
    int val = motorInvertArray[motorIndex] ? 0 : MAX_PWM_VALUE;
    int rev = motorInvertArray[motorIndex] ? MAX_PWM_VALUE : 0;
    ledcWrite(motors[motorIndex].pin1, val);
    ledcWrite(motors[motorIndex].pin2, rev);
}

void MotorController::controlMotorBackward(int motorIndex) {
    if (motorIndex >= (int)count) return;
    int val = motorInvertArray[motorIndex] ? MAX_PWM_VALUE : 0;
    int rev = motorInvertArray[motorIndex] ? 0 : MAX_PWM_VALUE;
    ledcWrite(motors[motorIndex].pin1, val);
    ledcWrite(motors[motorIndex].pin2, rev);
}

void MotorController::controlMotorStop(int motorIndex) {
    Serial.printf("Stopping motor %d\n", motorIndex);
    if (motorIndex >= (int)count) return;
    ledcWrite(motors[motorIndex].pin1, 0);
    ledcWrite(motors[motorIndex].pin2, 0);
}

void MotorController::controlMotorRaw(int motorIndex, int pwmValue) {
    if (motorIndex >= (int)count) return;

    // Apply per-motor invert, but skip deadband mapping
    if (motorInvertArray[motorIndex]) pwmValue = -pwmValue;
    pwmValue = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    int outMag = abs(pwmValue);
    outMag = (int)(outMag * speedMultiplier);
    outMag = constrain(outMag, 0, MAX_PWM_VALUE);

    if (pwmValue >= 0) {
        ledcWrite(motors[motorIndex].pin1, outMag);
        ledcWrite(motors[motorIndex].pin2, 0);
    } else {
        ledcWrite(motors[motorIndex].pin1, 0);
        ledcWrite(motors[motorIndex].pin2, outMag);
    }
}

int MotorController::getMotorPWM(int motorIndex) {
    if (motorIndex >= (int)count) return 0;
    int pwmForward = ledcRead(motors[motorIndex].pin1);
    int pwmReverse = ledcRead(motors[motorIndex].pin2);
    int val = 0;
    if (pwmForward > 0) {
        val = pwmForward;
    } else if (pwmReverse > 0) {
        val = -pwmReverse;
    }
    if (motorInvertArray[motorIndex]) val = -val;
    return val;
}

void MotorController::setSpeedMultiplier(float m) {
    // Clamp to reasonable range
    if (m < 0.2f) m = 0.2f;
    if (m > 1.5f) m = 1.5f;
    speedMultiplier = m;
}

float MotorController::normalizeAxis(int raw) const {
    float v = raw / (float)MAX_JOYSTICK_VALUE;
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
}

float MotorController::applyAxisDeadband(float value) const {
    int dbRaw = driveProfile.axisDeadband;
    if (dbRaw <= 0) return value;
    if (dbRaw >= MAX_JOYSTICK_VALUE) return 0.0f;
    float db = dbRaw / (float)MAX_JOYSTICK_VALUE;
    if (fabsf(value) <= db) return 0.0f;
    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    float scaled = (fabsf(value) - db) / (1.0f - db);
    if (scaled < 0.0f) scaled = 0.0f;
    return sign * scaled;
}

void MotorController::mixArcade(float throttle, float turn, float& left, float& right) const {
    float scaledTurn = turn * driveProfile.turnGain;
    left = throttle + scaledTurn;
    right = throttle - scaledTurn;
    float maxMag = fmaxf(fabsf(left), fabsf(right));
    if (maxMag > 1.0f) {
        left /= maxMag;
        right /= maxMag;
    }
}

float MotorController::applyMotorCurve(float value) const {
    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    float absVal = fabsf(value);

    switch (motorCurve.type) {
        case MotorCurveConfig::Type::Linear:
            break;
        case MotorCurveConfig::Type::Expo: {
            float exponent = 1.0f + motorCurve.strength;
            if (exponent < 0.2f) exponent = 0.2f;
            if (exponent > 5.0f) exponent = 5.0f;
            absVal = powf(absVal, exponent);
            break;
        }
    }

    return sign * absVal;
}

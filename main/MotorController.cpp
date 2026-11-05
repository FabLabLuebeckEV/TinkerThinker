// MotorController.cpp
#include <Arduino.h>
#include <math.h>
#include "MotorController.h"

#define MAX_PWM_VALUE 255
#define MAX_JOYSTICK_VALUE 512
#define PWM_BITS 8

MotorController::MotorController(Motor motors[], size_t motorCount, const int motorFrequencies[], const int motorDeadbands[], const bool motorInvertArr[], bool motorSwap,
                                 const DriveProfileConfig& driveCfg, const MotorCurveConfig& curveCfg, const OutputConfig& outCfg)
: motors(motors), count(motorCount)
{
    outMutex = xSemaphoreCreateMutex();
    for (int i = 0; i < 4; i++) freq[i] = 0;
    applyConfig(motorFrequencies, motorDeadbands, motorInvertArr, motorSwap, driveCfg, curveCfg, outCfg);
}

void MotorController::applyConfig(const int motorFrequencies[], const int motorDeadbands[], const bool motorInvertArr[], bool swap,
                                  const DriveProfileConfig& driveCfg, const MotorCurveConfig& curveCfg, const OutputConfig& outCfg) {
    xSemaphoreTake(outMutex, portMAX_DELAY);
    for (int i = 0; i < (int)count; i++) {
        int f = constrain(motorFrequencies[i], MIN_PWM_FREQ, MAX_PWM_FREQ);
        bool changed = (f != freq[i]);
        freq[i] = f;
        deadband[i] = motorDeadbands[i];
        motorInvertArray[i] = motorInvertArr[i];
        if (initialized && changed) {
            // Beide Pins eines Motors hängen am selben LEDC-Timer (Kanäle 2n und 2n+1)
            ledcChangeFrequency(motors[i].pin1, f, PWM_BITS);
            ledcChangeFrequency(motors[i].pin2, f, PWM_BITS);
        }
    }
    motorSwap = swap;
    driveProfile = driveCfg;
    motorCurve = curveCfg;
    output = outCfg;
    output.kickMs = constrain(outCfg.kickMs, 0, 500);
    output.kickPwm = constrain(outCfg.kickPwm, 0, MAX_PWM_VALUE);
    // Laufende Motoren sofort mit der (evtl. geänderten) Decay-Art neu ausgeben
    if (initialized) {
        for (int i = 0; i < (int)count; i++) applyPwm(i, state[i].dir, state[i].targetMag);
    }
    xSemaphoreGive(outMutex);
}

void MotorController::init() {
    for (size_t i = 0; i < count; ++i) {
        pinMode(motors[i].pin1, OUTPUT);
        pinMode(motors[i].pin2, OUTPUT);

        // ersetzt ledcSetup + ledcAttachPin; Frequenz pro Motor aus der Konfiguration
        ledcAttachChannel(motors[i].pin1, freq[i], PWM_BITS,
                          motors[i].channel_forward);
        ledcAttachChannel(motors[i].pin2, freq[i], PWM_BITS,
                          motors[i].channel_reverse);
        Serial.printf("Motor %u: PWM %d Hz, %s Decay\n", (unsigned)i, freq[i], output.slowDecay ? "Slow" : "Fast");
    }
    initialized = true;
    for (size_t i = 0; i < count; ++i) applyPwm(i, 0, 0);
}

// Gibt Richtung und Betrag an die H-Brücke aus (AT8833 / DRV8833 Wahrheitstabelle).
// Aufrufer hält outMutex.
void MotorController::applyPwm(int index, int dir, int mag) {
    int a;
    int b;
    if (dir == 0 || mag <= 0) {
        // Stopp: beide Eingänge high = Bremse, beide low = Freilauf
        a = output.brakeOnStop ? MAX_PWM_VALUE : 0;
        b = a;
    } else if (output.slowDecay) {
        // Slow Decay: ein Eingang dauerhaft high, der andere mit invertierter PWM.
        // In der Aus-Phase sind beide high = Bremse, der Motorstrom fließt weiter.
        if (dir > 0) { a = MAX_PWM_VALUE; b = MAX_PWM_VALUE - mag; }
        else         { a = MAX_PWM_VALUE - mag; b = MAX_PWM_VALUE; }
    } else {
        // Fast Decay: PWM auf einem Eingang, der andere low (Aus-Phase = Freilauf)
        if (dir > 0) { a = mag; b = 0; }
        else         { a = 0; b = mag; }
    }
    ledcWrite(motors[index].pin1, a);
    ledcWrite(motors[index].pin2, b);
}

// value: -255..255 nach Invertierung und Deadband, request: angeforderter Wert davor
void MotorController::writeOutput(int index, int value, int request) {
    if (index < 0 || index >= (int)count) return;
    value = constrain(value, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    int dir = (value > 0) - (value < 0);
    int mag = abs(value);

    xSemaphoreTake(outMutex, portMAX_DELAY);
    MotorState& st = state[index];
    uint32_t now = millis();
    if (st.blockedDir != 0) {
        if (dir == 0 || dir != st.blockedDir) {
            // Losgelassen oder Gegenrichtung (zum Freifahren): Sperre aufheben
            st.blockedDir = 0;
        } else {
            // Weiter in die blockierte Richtung gedrückt: Motor bleibt aus
            dir = 0;
            mag = 0;
        }
    }
    if (dir == 0) {
        st.kicking = false;
    } else if (dir != st.dir) {
        // Anfahren aus dem Stand oder Richtungswechsel: Anlauf-Boost, falls er stärker ist
        st.kicking = (output.kickMs > 0 && output.kickPwm > mag);
        st.kickUntilMs = now + output.kickMs;
    }
    st.dir = dir;
    st.targetMag = mag;
    st.request = request;

    int outMag = mag;
    if (st.kicking) {
        if ((int32_t)(now - st.kickUntilMs) < 0) outMag = max(mag, output.kickPwm);
        else st.kicking = false;
    }
    applyPwm(index, dir, outMag);
    xSemaphoreGive(outMutex);
}

void MotorController::update() {
    for (int i = 0; i < (int)count; i++) {
        if (!state[i].kicking) continue;   // ungeschützter Blick, entschieden wird unter dem Mutex
        xSemaphoreTake(outMutex, portMAX_DELAY);
        MotorState& st = state[i];
        if (st.kicking && (int32_t)(millis() - st.kickUntilMs) >= 0) {
            st.kicking = false;
            applyPwm(i, st.dir, st.targetMag);
        }
        xSemaphoreGive(outMutex);
    }
}

void MotorController::controlMotor(int index, int pwmValue) {
    if (index < 0 || index >= (int)count) return;
    int request = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    // Apply per-motor invert first
    pwmValue = motorInvertArray[index] ? -request : request;

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

    writeOutput(index, pwmValue >= 0 ? outMag : -outMag, request);
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
    if (motorIndex < 0 || motorIndex >= (int)count) return;
    writeOutput(motorIndex, motorInvertArray[motorIndex] ? -MAX_PWM_VALUE : MAX_PWM_VALUE, MAX_PWM_VALUE);
}

void MotorController::controlMotorBackward(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= (int)count) return;
    writeOutput(motorIndex, motorInvertArray[motorIndex] ? MAX_PWM_VALUE : -MAX_PWM_VALUE, -MAX_PWM_VALUE);
}

void MotorController::controlMotorStop(int motorIndex) {
    Serial.printf("Stopping motor %d\n", motorIndex);
    if (motorIndex < 0 || motorIndex >= (int)count) return;
    writeOutput(motorIndex, 0, 0);
}

void MotorController::controlMotorRaw(int motorIndex, int pwmValue) {
    if (motorIndex < 0 || motorIndex >= (int)count) return;
    int request = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    // Apply per-motor invert, but skip deadband mapping (Setup misst damit die Anlauf-Schwelle,
    // der Anlauf-Boost wirkt auch hier, damit die gemessene Schwelle zum Fahrbetrieb passt)
    pwmValue = motorInvertArray[motorIndex] ? -request : request;

    int outMag = abs(pwmValue);
    outMag = (int)(outMag * speedMultiplier);
    outMag = constrain(outMag, 0, MAX_PWM_VALUE);

    writeOutput(motorIndex, pwmValue >= 0 ? outMag : -outMag, request);
}

int MotorController::getMotorPWM(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= (int)count) return 0;
    int val = state[motorIndex].dir * state[motorIndex].targetMag;
    if (motorInvertArray[motorIndex]) val = -val;
    return val;
}

int MotorController::getMotorRequest(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= (int)count) return 0;
    return state[motorIndex].request;
}

void MotorController::tripStall(int motorIndex) {
    if (motorIndex < 0 || motorIndex >= (int)count) return;
    xSemaphoreTake(outMutex, portMAX_DELAY);
    MotorState& st = state[motorIndex];
    if (st.dir != 0) {
        st.blockedDir = st.dir;
        st.dir = 0;
        st.targetMag = 0;
        st.kicking = false;
        applyPwm(motorIndex, 0, 0);
    }
    xSemaphoreGive(outMutex);
}

bool MotorController::isBlocked(int motorIndex) const {
    if (motorIndex < 0 || motorIndex >= (int)count) return false;
    return state[motorIndex].blockedDir != 0;
}

int MotorController::getMotorFrequency(int motorIndex) const {
    if (motorIndex < 0 || motorIndex >= (int)count) return 0;
    return freq[motorIndex];
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

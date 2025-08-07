// MotorController.cpp
#include <Arduino.h>
#include "MotorController.h"

#define MAX_PWM_VALUE 255
#define MAX_JOYSTICK_VALUE 512

MotorController::MotorController(Motor motors[], size_t motorCount, const int motorFrequencies[], const int motorDeadbands[], const bool motorInvertArr[], bool motorSwap)
: motors(motors), count(motorCount), motorSwap(motorSwap)
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
    Serial.printf("Controlling motor %d with PWM value: %d\n", index, pwmValue);
    if (index >= (int)count) return;

    // Invert if needed
    if (motorInvertArray[index]) pwmValue = -pwmValue;

    int db = deadband[index];
    int maxPWM = MAX_PWM_VALUE;

    if (abs(pwmValue) < 10) {
        pwmValue = 0; // Below deadband, no movement
    } else {
        // Scale between deadband and maxPWM
        pwmValue = (pwmValue > 0) 
                   ? map(pwmValue, 10, maxPWM, db, maxPWM) 
                   : map(pwmValue, -maxPWM, -10, -maxPWM, -db);
    }

    pwmValue = constrain(pwmValue, -maxPWM, maxPWM);

    if (pwmValue >= 0) {
        ledcWrite(motors[index].pin1, pwmValue);
        ledcWrite(motors[index].pin2, 0);
    } else {
        ledcWrite(motors[index].pin1, 0);
        ledcWrite(motors[index].pin2, -pwmValue);
    }
}


int MotorController::scaleMovementToPWM(float movement) {
    if (movement > 0.0f) {
        return 110 + (int)(movement * (255 - 110));
    } else if (movement < 0.0f) {
        return -110 - (int)(abs(movement) * (255 - 110));
    } else {
        return 0;
    }
}

void MotorController::handleMotorControl(int axisX, int axisY, int leftMotorIndex, int rightMotorIndex) {
    // Wir nehmen hier an, dass der erste Motor seine deadband[i] betrifft
    // Allerdings ist das joystick-basierte Handling komplexer,
    // da wir für jeden Motor unterschiedliche Deadbands haben.
    // Der Einfachheit halber gilt: Wir nutzen den gleichen deadband
    // für die X/Y Berechnung. Man kann dies weiter anpassen.
    
    // Für ein differenziertes Handling müsste man überlegen, wie man die Deadband pro Motor anwendet.
    // Hier ein vereinfachter Ansatz:
    int db = deadband[leftMotorIndex]; // Annahme: gleicher Deadband für beide Motoren, oder du nimmst einen Mittelwert
    // Alternativ könnte man so etwas tun:
    // int db = (deadband[leftMotorIndex] + deadband[rightMotorIndex]) / 2;

    int normX = (abs(axisX) < db) ? 0 : axisX;
    int normY = (abs(axisY) < db) ? 0 : axisY;
    Serial.printf("normX: %d, normY: %d\n", normX, normY);

    float maxMovement = (float)(MAX_JOYSTICK_VALUE - db);
    float movementLeft = (float)(normY + normX) / maxMovement;
    float movementRight = (float)(normY - normX) / maxMovement;

    int rightPWM = scaleMovementToPWM(movementLeft);
    int leftPWM = scaleMovementToPWM(movementRight);

    leftPWM = constrain(leftPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    rightPWM = constrain(rightPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    int motorLeft = motorSwap ? rightMotorIndex : leftMotorIndex;
    int motorRight = motorSwap ? leftMotorIndex : rightMotorIndex;

    Serial.printf("Left PWM: %d, Right PWM: %d\n", leftPWM, rightPWM);
    controlMotor(motorLeft, leftPWM);
    controlMotor(motorRight, rightPWM);
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

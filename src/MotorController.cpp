// MotorController.cpp
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
    for (size_t i = 0; i < count; i++) {
        pinMode(motors[i].pin1, OUTPUT);
        pinMode(motors[i].pin2, OUTPUT);
        ledcSetup(motors[i].channel_forward, freq[i], 8);
        ledcSetup(motors[i].channel_reverse, freq[i], 8);
        ledcAttachPin(motors[i].pin1, motors[i].channel_forward);
        ledcAttachPin(motors[i].pin2, motors[i].channel_reverse);
    }
}

void MotorController::controlMotor(int index, int pwmValue) {
    if (index >= (int)count) return;
    // Invertierung berücksichtigen
    if (motorInvertArray[index]) pwmValue = -pwmValue;

    pwmValue = constrain(pwmValue, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    if (pwmValue >= 0) {
        ledcWrite(motors[index].channel_forward, pwmValue);
        ledcWrite(motors[index].channel_reverse, 0);
    } else {
        ledcWrite(motors[index].channel_forward, 0);
        ledcWrite(motors[index].channel_reverse, -pwmValue);
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

    float maxMovement = (float)(MAX_JOYSTICK_VALUE - db);
    float movementLeft = (float)(normY + normX) / maxMovement;
    float movementRight = (float)(normY - normX) / maxMovement;

    int rightPWM = scaleMovementToPWM(movementLeft);
    int leftPWM = scaleMovementToPWM(movementRight);

    leftPWM = constrain(leftPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    rightPWM = constrain(rightPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    int motorLeft = motorSwap ? rightMotorIndex : leftMotorIndex;
    int motorRight = motorSwap ? leftMotorIndex : rightMotorIndex;

    controlMotor(motorLeft, leftPWM);
    controlMotor(motorRight, rightPWM);
}

void MotorController::controlMotorForward(int motorIndex) {
    if (motorIndex >= (int)count) return;
    int val = motorInvertArray[motorIndex] ? 0 : MAX_PWM_VALUE;
    int rev = motorInvertArray[motorIndex] ? MAX_PWM_VALUE : 0;
    ledcWrite(motors[motorIndex].channel_forward, val);
    ledcWrite(motors[motorIndex].channel_reverse, rev);
}

void MotorController::controlMotorBackward(int motorIndex) {
    if (motorIndex >= (int)count) return;
    int val = motorInvertArray[motorIndex] ? MAX_PWM_VALUE : 0;
    int rev = motorInvertArray[motorIndex] ? 0 : MAX_PWM_VALUE;
    ledcWrite(motors[motorIndex].channel_forward, val);
    ledcWrite(motors[motorIndex].channel_reverse, rev);
}

void MotorController::controlMotorStop(int motorIndex) {
    if (motorIndex >= (int)count) return;
    ledcWrite(motors[motorIndex].channel_forward, 0);
    ledcWrite(motors[motorIndex].channel_reverse, 0);
}

int MotorController::getMotorPWM(int motorIndex) {
    if (motorIndex >= (int)count) return 0;
    int pwmForward = ledcRead(motors[motorIndex].channel_forward);
    int pwmReverse = ledcRead(motors[motorIndex].channel_reverse);
    int val = 0;
    if (pwmForward > 0) {
        val = pwmForward;
    } else if (pwmReverse > 0) {
        val = -pwmReverse;
    }
    if (motorInvertArray[motorIndex]) val = -val;
    return val;
}

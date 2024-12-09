#include "MotorController.h"

#define MAX_PWM_VALUE 255
#define DEAD_BAND 50
#define MAX_JOYSTICK_VALUE 512 // Beispielwert, anpassen

MotorController::MotorController(Motor motors[], size_t motorCount) : motors(motors), count(motorCount) {}

void MotorController::init() {
    for (size_t i = 0; i < count; i++) {
        pinMode(motors[i].pin1, OUTPUT);
        pinMode(motors[i].pin2, OUTPUT);
        ledcSetup(motors[i].channel_forward, 5000, 8); // PWM-Frequenz anpassen
        ledcSetup(motors[i].channel_reverse, 5000, 8);
        ledcAttachPin(motors[i].pin1, motors[i].channel_forward);
        ledcAttachPin(motors[i].pin2, motors[i].channel_reverse);
    }
}

void MotorController::controlMotor(int index, int pwmValue) {
    if (index >= count) return;
    
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
    if (movement > 0.0) {
        return 110 + (int)(movement * (255 - 110));
    } else if (movement < 0.0) {
        return -110 - (int)(abs(movement) * (255 - 110));
    } else {
        return 0;
    }
}

void MotorController::handleMotorControl(int axisX, int axisY, int leftMotorIndex, int rightMotorIndex) {
    int normX = (abs(axisX) < DEAD_BAND) ? 0 : axisX;
    int normY = (abs(axisY) < DEAD_BAND) ? 0 : axisY;

    float maxMovement = (float)(MAX_JOYSTICK_VALUE - DEAD_BAND);
    float movementLeft = (float)(normY + normX) / maxMovement;
    float movementRight = (float)(normY - normX) / maxMovement;

    int rightPWM = scaleMovementToPWM(movementLeft);
    int leftPWM = scaleMovementToPWM(movementRight);

    leftPWM = constrain(leftPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);
    rightPWM = constrain(rightPWM, -MAX_PWM_VALUE, MAX_PWM_VALUE);

    controlMotor(leftMotorIndex, leftPWM);
    controlMotor(rightMotorIndex, rightPWM);

    #ifdef DEBUG_OUTPUT
    Serial.println(rightPWM);
    Serial.println(leftPWM);
    #endif
}

void MotorController::controlMotorForward(int motorIndex) {
    if (motorIndex >= count) return;
    ledcWrite(motors[motorIndex].channel_forward, MAX_PWM_VALUE);
    ledcWrite(motors[motorIndex].channel_reverse, 0);
}

void MotorController::controlMotorBackward(int motorIndex) {
    if (motorIndex >= count) return;
    ledcWrite(motors[motorIndex].channel_forward, 0);
    ledcWrite(motors[motorIndex].channel_reverse, MAX_PWM_VALUE);
}

void MotorController::controlMotorStop(int motorIndex) {
    if (motorIndex >= count) return;
    ledcWrite(motors[motorIndex].channel_forward, 0);
    ledcWrite(motors[motorIndex].channel_reverse, 0);
}

int MotorController::getMotorPWM(int motorIndex) {
    if (motorIndex >= count) return 0;
    int pwmForward = ledcRead(motors[motorIndex].channel_forward);
    int pwmReverse = ledcRead(motors[motorIndex].channel_reverse);
    if (pwmForward > 0) {
        return pwmForward;
    } else if (pwmReverse > 0) {
        return -pwmReverse;
    }
    return 0;
}

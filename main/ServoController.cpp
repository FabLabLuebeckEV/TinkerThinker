#include "ServoController.h"

ServoController::ServoController(ServoMotor servos[], size_t servoCount) : servos(servos), count(servoCount) {
    // Standardwerte für Pulsweiten setzen, falls nicht anders definiert
    for (size_t i = 0; i < count; i++) {
        servos[i].min_pulsewidth = 500;
        servos[i].max_pulsewidth = 2500;
    }
}

void ServoController::init() {
    for (size_t i = 0; i < count; i++) {
        //ledcSetup(servos[i].channel, 50, ledc_resolution); // 50Hz für Servos
        ledcAttach(servos[i].pin, 50, ledc_resolution);
        setServoAngle(i, servos[i].angle);
    }
}

void ServoController::setServoAngle(int index, int angle) {
    //if (index >= count) return;

    angle = constrain(angle, 0, 180);
    int pulseWidth = map(angle, 0, 180, servos[index].min_pulsewidth, servos[index].max_pulsewidth);
    int maxDuty = (1 << ledc_resolution) - 1;
    int dutyCycle = (pulseWidth * maxDuty) / 20000;
    //Serial.println(String(servos[index].channel) +  String(dutyCycle));
    ledcWrite(servos[index].pin, dutyCycle);
    servos[index].angle = angle;
}

int ServoController::getServoAngle(int index) {
    if (index >= count) return -1;
    return servos[index].angle;
}

void ServoController::setPulseWidthRange(int index, int min_pw, int max_pw) {
    if (index >= count) return;
    servos[index].min_pulsewidth = min_pw;
    servos[index].max_pulsewidth = max_pw;
}

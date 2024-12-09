#include "ServoController.h"

#define SERVO_MIN_PULSEWIDTH 500
#define SERVO_MAX_PULSEWIDTH 2500
#define LEDC_RESOLUTION 16 // Beispielwert, anpassen

ServoController::ServoController(ServoMotor servos[], size_t servoCount) : servos(servos), count(servoCount) {}

void ServoController::init() {
    for (size_t i = 0; i < count; i++) {
        ledcSetup(servos[i].channel, 50, LEDC_RESOLUTION); // 50Hz für Servos
        ledcAttachPin(servos[i].pin, servos[i].channel);
        setServoAngle(i, servos[i].angle);
    }
}

void ServoController::setServoAngle(int index, int angle) {
    if (index >= count) return;
    
    angle = constrain(angle, 0, 180);
    int pulseWidth = map(angle, 0, 180, SERVO_MIN_PULSEWIDTH, SERVO_MAX_PULSEWIDTH);
    int dutyCycle = (pulseWidth * ((1 << LEDC_RESOLUTION) - 1)) / 20000;
    ledcWrite(servos[index].channel, dutyCycle);
    servos[index].angle = angle;
}

int ServoController::getServoAngle(int index) {
    if (index >= count) return -1;
    return servos[index].angle;
}

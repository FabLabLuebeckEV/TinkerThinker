#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>

struct ServoMotor {
    int pin;
    int channel;
    int angle;
};

class ServoController {
public:
    ServoController(ServoMotor servos[], size_t servoCount);
    void init();
    void setServoAngle(int index, int angle);
    int getServoAngle(int index);
    
private:
    ServoMotor* servos;
    size_t count;
};

#endif

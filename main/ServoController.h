#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>

struct ServoMotor {
    int pin;
    int channel;
    int angle;
    int min_pulsewidth; // neu
    int max_pulsewidth; // neu
};

class ServoController {
public:
    ServoController(ServoMotor servos[], size_t servoCount);
    void init();
    void setServoAngle(int index, int angle);
    int getServoAngle(int index);

    // Neue Methode zum Setzen der Pulsewidth-Range
    void setPulseWidthRange(int index, int min_pw, int max_pw);

private:
    ServoMotor* servos;
    size_t count;
    int ledc_resolution = 16; // Beispiel
};

#endif

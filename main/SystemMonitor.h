#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>

class SystemMonitor {
public:
    SystemMonitor(int currentPin1, int currentPin2, int faultPin1 = -1);
    void init();
    // Legacy-Board umschalten: Fault-Eingang aktivieren (Pin) oder abschalten (-1)
    void setFaultPin(int faultPin);
    void checkMotorDriverFault();
    bool isMotorInFault();
    float getHBridgeAmps(int motorIndex);
    // PWM-Frequenz des Motors am Sensor (0 = GPIO 34 / Motor B, 1 = GPIO 36 / Motor A)
    void setPwmFrequency(int sensorIndex, int frequencyHz);
    float readAverageCurrent(int adcPin, int pwmPeriodUs);
    
private:
    int fault1Pin = -1;
    int currentPin1;
    int currentPin2;
    // Der Strom fließt nur in der Ein-Phase der PWM durch den Shunt. Gemittelt wird deshalb
    // über ganze PWM-Perioden (mindestens 1 ms), passend zur Frequenz des jeweiligen Motors.
    int pwmPeriodUs[2] = {50, 50};  // Standard: 20 kHz
};

#endif

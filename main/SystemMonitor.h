#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>

class SystemMonitor {
public:
    SystemMonitor(int faultPin1, int currentPin1, int currentPin2);
    void init();
    void checkMotorDriverFault();
    bool isMotorInFault();
    float getHBridgeAmps(int motorIndex);
    float readAverageCurrent(int adcPin);
    
private:
    int fault1Pin;
    int currentPin1;
    int currentPin2;
    int pwmFrequency = 5000;        // PWM-Frequenz in Hz
    int samplesPerCycle = 20;       // Anzahl der ADC-Samples pro PWM-Zyklus
    int pwmPeriodUs = 200;          // PWM-Periode in Mikrosekunden (1/5000 Hz = 200 µs)
};

#endif

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
    float readAverageCurrent(int adcPin);
    
private:
    int fault1Pin = -1;
    int currentPin1;
    int currentPin2;
    int pwmFrequency = 5000;        // PWM-Frequenz in Hz
    int samplesPerCycle = 20;       // Anzahl der ADC-Samples pro PWM-Zyklus
    int pwmPeriodUs = 200;          // PWM-Periode in Mikrosekunden (1/5000 Hz = 200 us)
};

#endif

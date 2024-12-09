#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>

class SystemMonitor {
public:
    SystemMonitor(int faultPin1, int faultPin2);
    void init();
    void checkMotorDriverFault();
    bool isMotorInFault(int motorIndex);
    
private:
    int fault1Pin;
    int fault2Pin;
};

#endif

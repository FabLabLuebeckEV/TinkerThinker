#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>

class BatteryMonitor {
public:
    BatteryMonitor(int batteryPin);
    void init();
    float readVoltage();
    float readPercentage();
    
private:
    int pin;
    // Kalibrierungsfaktoren für Lithium-Ionen-Akkus
    float voltageMin = 3.0;
    float voltageMax = 4.2;
};

#endif

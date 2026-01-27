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
    float mapVoltageToPercent(float voltage);

    // Divider ratio: Vbat = Vpin * dividerRatio
    // Adjust if resistor values differ.
    float dividerRatio = 2.14f;
};

#endif

#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(int batteryPin) : pin(batteryPin) {}

void BatteryMonitor::init() {
    analogReadResolution(12); // 12-bit ADC
    // analogSetAttenuation(ADC_11db); // Voltamperbereich anpassen
}

float BatteryMonitor::readVoltage() {
    // Mesure mittelwert
    int adc_reading = 0;
    for (int i = 0; i < 5; i++) {
        adc_reading += analogRead(pin);
        delay(1);
    }
    adc_reading /= 5;
    
    // Annahme: 3.3V Referenz und Spannungsteiler
    float voltage = (adc_reading / 4095.0) * 3.3 * dividerRatio; // Beispiel: Spannungsteilung x2
    return voltage;
}

float BatteryMonitor::readPercentage() {
    float voltage = readVoltage();
    return mapVoltageToPercent(voltage);
}

float BatteryMonitor::mapVoltageToPercent(float voltage) {
    // Typical 1S Li-ion discharge curve (loaded), piecewise-linear
    static const float v[] = {
        4.20, 4.15, 4.11, 4.08, 4.02, 3.98, 3.92, 3.87,
        3.82, 3.79, 3.77, 3.74, 3.70, 3.65, 3.30
    };
    static const float p[] = {
        100,  95,  90,  85,  75,  65,  55,  45,
         35,  25,  20,  15,  10,   5,   0
    };
    if (voltage >= v[0]) return 100.0f;
    if (voltage <= v[14]) return 0.0f;
    for (int i = 0; i < 14; i++) {
        if (voltage <= v[i] && voltage >= v[i + 1]) {
            float t = (voltage - v[i + 1]) / (v[i] - v[i + 1]);
            return p[i + 1] + t * (p[i] - p[i + 1]);
        }
    }
    return 0.0f;
}

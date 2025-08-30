#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(int batteryPin) : pin(batteryPin) {}

void BatteryMonitor::init() {
    analogReadResolution(12); // 12-bit ADC
    //analogSetAttenuation(ADC_11db); // Voltamperbereich anpassen
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
    float voltage = (adc_reading / 4095.0) * 3.3 * 2.14; // Beispiel: Spannungsteilung x2
    return voltage;
}

float BatteryMonitor::readPercentage() {
    float voltage = readVoltage();
    if (voltage <= voltageMin) return 0.0;
    if (voltage >= voltageMax) return 100.0;
    return ((voltage - voltageMin) / (voltageMax - voltageMin)) * 100.0;
}

#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(int batteryPin) : pin(batteryPin) {}

void BatteryMonitor::init() {
    analogReadResolution(12); // 12-bit ADC
    //analogSetAttenuation(ADC_11db); // Voltamperbereich anpassen
}

float BatteryMonitor::readVoltage() {
    int adc_reading = analogRead(pin);
    // Annahme: 3.3V Referenz und Spannungsteiler
    float voltage = (adc_reading / 4095.0) * 3.3 * 2; // Beispiel: Spannungsteilung x2
    return voltage;
}

float BatteryMonitor::readPercentage() {
    float voltage = readVoltage();
    if (voltage <= voltageMin) return 0.0;
    if (voltage >= voltageMax) return 100.0;
    return ((voltage - voltageMin) / (voltageMax - voltageMin)) * 100.0;
}

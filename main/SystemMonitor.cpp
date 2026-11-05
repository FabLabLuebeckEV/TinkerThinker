#include "SystemMonitor.h"

SystemMonitor::SystemMonitor(int xcurrentPin1, int xcurrentPin2, int xfaultPin1) {
    currentPin1 = xcurrentPin1;
    currentPin2 = xcurrentPin2;
    fault1Pin = xfaultPin1;
}

void SystemMonitor::init() {
    if (fault1Pin >= 0) {
        pinMode(fault1Pin, INPUT);
    }
    pinMode(currentPin1, INPUT);
    pinMode(currentPin2, INPUT);
}

void SystemMonitor::setFaultPin(int faultPin) {
    fault1Pin = faultPin;
    if (fault1Pin >= 0) {
        pinMode(fault1Pin, INPUT);
    }
}

void SystemMonitor::checkMotorDriverFault() {
    if (fault1Pin < 0) return;
    #ifdef DEBUG_OUTPUT
    int fault1 = digitalRead(fault1Pin);
    if (fault1 == HIGH) {
        Serial.println("Motor Driver Fault");
    } else {
        Serial.println("Motor Drivers OK");
    }
    #endif
}

bool SystemMonitor::isMotorInFault() {
    if (fault1Pin < 0) return false;
    return digitalRead(fault1Pin) == HIGH;
}

void SystemMonitor::setPwmFrequency(int sensorIndex, int frequencyHz) {
    if (sensorIndex < 0 || sensorIndex > 1 || frequencyHz <= 0) return;
    pwmPeriodUs[sensorIndex] = max(1, 1000000 / frequencyHz);
}

// Funktion zur Mittelung der ADC-Werte
float SystemMonitor::readAverageCurrent(int adcPin, int periodUs) {
    // Fenster: kleinstes Vielfaches der PWM-Periode, das mindestens 1 ms lang ist.
    int periods = (1000 + periodUs - 1) / periodUs;
    uint32_t windowUs = (uint32_t)periods * periodUs;
    // Sample-Anzahl teilerfremd zur Zahl der Perioden: Die Samples fallen dann auf
    // gleichmäßig verteilte Phasen der PWM statt immer auf dieselbe Stelle.
    int samples = (periods % 23 == 0) ? 29 : 23;

    long adcSum = 0;
    // Zeitpunkte absolut planen, damit die Dauer von analogRead() das Fenster nicht verschiebt
    uint32_t start = micros();
    for (int i = 0; i < samples; i++) {
        uint32_t due = start + (uint32_t)((uint64_t)i * windowUs / samples);
        while ((int32_t)(micros() - due) < 0) {}
        adcSum += analogRead(adcPin);      // ADC-Wert lesen
    }

    // Durchschnitt der ADC-Werte berechnen
    float averageADC = adcSum / (float)samples;

    // ADC-Wert in Spannung umrechnen (12-Bit-ADC)
    float voltage = averageADC * (3.3 / 4096.0); // 3.3 V Referenz, 12-Bit Auflösung

    // Spannung in Strom umrechnen
    float current = voltage / (0.5926 * 20 * 0.2); // Spannungsteiler- und Verstärkungsfaktor berücksichtigen

    return current; // Strom in Ampere
}

float SystemMonitor::getHBridgeAmps(int motorIndex) {
    float current = 0.0;
    if (motorIndex == 0) {
        current = readAverageCurrent(currentPin1, pwmPeriodUs[0]);
    } else if (motorIndex == 1) {
        current = readAverageCurrent(currentPin2, pwmPeriodUs[1]);
    }
    return current;
}

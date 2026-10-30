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

// Funktion zur Mittelung der ADC-Werte
float SystemMonitor::readAverageCurrent(int adcPin) {
    long adcSum = 0;

    // Innerhalb eines PWM-Zyklus 20 Samples sammeln
    for (int i = 0; i < samplesPerCycle; i++) {
        adcSum += analogRead(adcPin);      // ADC-Wert lesen
        delayMicroseconds(pwmPeriodUs / samplesPerCycle); // Abtastintervall
    }

    // Durchschnitt der ADC-Werte berechnen
    float averageADC = adcSum / (float)samplesPerCycle;

    // ADC-Wert in Spannung umrechnen (12-Bit-ADC)
    float voltage = averageADC * (3.3 / 4096.0); // 3.3 V Referenz, 12-Bit Auflösung

    // Spannung in Strom umrechnen
    float current = voltage / (0.5926 * 20 * 0.2); // Spannungsteiler- und Verstärkungsfaktor berücksichtigen

    return current; // Strom in Ampere
}

float SystemMonitor::getHBridgeAmps(int motorIndex) {
    float current = 0.0;
    if (motorIndex == 0) {
        current = readAverageCurrent(currentPin1);
    } else if (motorIndex == 1) {
        current = readAverageCurrent(currentPin2);
    }
    return current;
}

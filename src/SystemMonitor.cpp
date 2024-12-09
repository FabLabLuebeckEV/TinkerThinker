#include "SystemMonitor.h"

SystemMonitor::SystemMonitor(int faultPin1, int faultPin2) : fault1Pin(faultPin1), fault2Pin(faultPin2) {}

void SystemMonitor::init() {
    pinMode(fault1Pin, INPUT);
    pinMode(fault2Pin, INPUT);
}

void SystemMonitor::checkMotorDriverFault() {
    int fault1 = digitalRead(fault1Pin);
    int fault2 = digitalRead(fault2Pin);
    #ifdef DEBUG_OUTPUT
    if (fault1 == LOW) {
        Serial.println("Motor1 Driver Fault");
    }
    if (fault2 == LOW) {
        Serial.println("Motor2 Driver Fault");
    }
    if (fault1 == HIGH && fault2 == HIGH) {
        Serial.println("Motor Drivers OK");
    }
    #endif
}

bool SystemMonitor::isMotorInFault(int motorIndex) {
    if (motorIndex == 0 || motorIndex == 1) {
        return digitalRead(fault1Pin) == LOW;
    } else if (motorIndex == 2 || motorIndex == 3) {
        return digitalRead(fault2Pin) == LOW;
    }
}

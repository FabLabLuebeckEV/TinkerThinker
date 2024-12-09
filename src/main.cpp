#include <Arduino.h>
#include "MyCustomBoard.h"

// Initialisiere MyCustomBoard
MyCustomBoard board;

// Zeitvariable für regelmäßige Status-Updates
unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 1000; // 1 Sekunde

void setup() {
    board.begin();
}

void loop() {
    Serial.println("Hello, World!");
    sleep(1000);
}

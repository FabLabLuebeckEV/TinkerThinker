#include <Arduino.h>
#include "MyCustomBoard.h"

// Initialisiere MyCustomBoard
MyCustomBoard board;

void setup() {
    board.begin();
}

void loop() {
    Serial.println("Hello, World!");
    sleep(1000);
}

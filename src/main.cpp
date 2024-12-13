#include <Arduino.h>
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"

// Erzeuge globalen Board- und ConfigManager
ConfigManager configManager;
TinkerThinkerBoard board(&configManager);

void setup() {
    Serial.begin(115200);
    if (!configManager.init()) {
        Serial.println("Failed to init ConfigManager!");
    }
    board.begin();
}

void loop() {
    // Hier können weitere Logiken rein, falls nötig.
    // Der WebServer läuft asynchron.
    delay(1000);
}

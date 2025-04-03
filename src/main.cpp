#include <Arduino.h>
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Erzeuge globalen Board- und ConfigManager
ConfigManager configManager;
TinkerThinkerBoard board(&configManager);

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

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

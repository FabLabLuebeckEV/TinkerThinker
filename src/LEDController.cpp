#include "LEDController.h"

LEDController::LEDController() : ledCount(NUM_LEDS) {}

void LEDController::init() {
    // Verwende den festgelegten LED_DATA_PIN
    FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(ledsArray, ledCount).setCorrection(TypicalLEDStrip);
    FastLED.clear();
    FastLED.show();
}

void LEDController::setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led < 0 || led >= ledCount) {
        Serial.printf("LED index out of range: %d\n", led);
        return;
    }
    ledsArray[led] = CRGB(green, red, blue); // GRB Format
}

void LEDController::showPixels() {
    FastLED.show();
}

CRGB LEDController::getLEDColor(int ledIndex) {
    if (ledIndex < 0 || ledIndex >= ledCount) {
        Serial.printf("LED index out of range: %d\n", ledIndex);
        return CRGB::Black;
    }
    return ledsArray[ledIndex];
}
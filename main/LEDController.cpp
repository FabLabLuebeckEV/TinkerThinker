#include "LEDController.h"

LEDController::LEDController(int count)
: ledCount(count) 
{
    ledsArray = new CRGB[ledCount];
}

void LEDController::init() {
    FastLED.addLeds<WS2812, 2, GRB>(ledsArray, ledCount).setCorrection(TypicalLEDStrip);
    FastLED.clear();
    FastLED.show();
}

void LEDController::setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led < 0 || led >= ledCount) {
        Serial.printf("LED index out of range: %d\n", led);
        return;
    }
    ledsArray[led] = CRGB(red, green, blue); // Beachte: FastLED ist im Format RGB, nicht GRB
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

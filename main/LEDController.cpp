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
    CRGB c(red, green, blue);
    if (gammaEnabled) c.napplyGamma_video(2.2f);
    ledsArray[led] = c;
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

void LEDController::setBrightness(uint8_t value) {
    FastLED.setBrightness(value);
    FastLED.show();
}

void LEDController::setGamma(bool enabled) {
    gammaEnabled = enabled;
}

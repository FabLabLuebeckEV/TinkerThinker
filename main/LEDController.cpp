#include "LEDController.h"

LEDController::LEDController(int count)
: ledCount(count) 
{
    ledsArray = new CRGB[ledCount];
    rawColorsArray = new CRGB[ledCount];
    for (int i = 0; i < ledCount; i++) {
        ledsArray[i] = CRGB::Black;
        rawColorsArray[i] = CRGB::Black;
    }
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
    CRGB raw(red, green, blue);
    rawColorsArray[led] = raw;
    lastSetColor = raw;

    CRGB c = raw;
    if (gammaEnabled) napplyGamma_video(c, 2.2f); // freie FastLED-Funktion (CRGB hat keinen Member)
    ledsArray[led] = c;
}

void LEDController::setLedCount(int count) {
    if (count < 0) count = 0;
    if (count == ledCount) return;

    CRGB* newLeds = new CRGB[count];
    CRGB* newRaw = new CRGB[count];
    for (int i = 0; i < count; i++) {
        newLeds[i] = (i < ledCount) ? ledsArray[i] : CRGB::Black;
        newRaw[i] = (i < ledCount) ? rawColorsArray[i] : CRGB::Black;
    }

    CRGB* oldLeds = ledsArray;
    CRGB* oldRaw = rawColorsArray;
    // Reihenfolge so, dass setPixelColor() aus einem anderen Task nie ueber das Pufferende
    // schreibt: beim Verkleinern zuerst die Anzahl senken, beim Vergroessern zuletzt erhoehen.
    if (count < ledCount) {
        // Ueberzaehlige LEDs dunkel schalten, bevor der Treiber sie nicht mehr ansteuert
        for (int i = count; i < ledCount; i++) oldLeds[i] = CRGB::Black;
        FastLED.show();
        ledCount = count;
        FastLED[0].setLeds(newLeds, count);
        ledsArray = newLeds;
        rawColorsArray = newRaw;
    } else {
        ledsArray = newLeds;
        rawColorsArray = newRaw;
        FastLED[0].setLeds(newLeds, count);
        ledCount = count;
    }

    // Ein FastLED.show() aus einem anderen Task kann noch kurz den alten Puffer lesen
    // (300 LEDs ~ 9 ms). Erst danach freigeben.
    vTaskDelay(pdMS_TO_TICKS(50));
    delete[] oldLeds;
    delete[] oldRaw;
}

void LEDController::showPixels() {
    FastLED.show();
}

CRGB LEDController::getLEDColor(int ledIndex) {
    if (ledIndex == 0) {
        return lastSetColor;
    }
    if (ledIndex < 0 || ledIndex >= ledCount) {
        Serial.printf("LED index out of range: %d\n", ledIndex);
        return CRGB::Black;
    }
    return rawColorsArray[ledIndex];
}

void LEDController::setBrightness(uint8_t value) {
    FastLED.setBrightness(value);
    FastLED.show();
}

void LEDController::setGamma(bool enabled) {
    gammaEnabled = enabled;
}

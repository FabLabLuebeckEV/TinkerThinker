#include "LEDController.h"

LEDController::LEDController(int count)
: ledCount(count) 
{
    ledMutex = xSemaphoreCreateMutex();
    ledsArray = new CRGB[ledCount];
    rawColorsArray = new CRGB[ledCount];
    for (int i = 0; i < ledCount; i++) {
        ledsArray[i] = CRGB::Black;
        rawColorsArray[i] = CRGB::Black;
    }
}

void LEDController::init() {
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    FastLED.addLeds<WS2812, 2, GRB>(ledsArray, ledCount).setCorrection(TypicalLEDStrip);
    FastLED.clear();
    FastLED.show();
    xSemaphoreGive(ledMutex);
}

void LEDController::setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue) {
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    if (led < 0 || led >= ledCount) {
        xSemaphoreGive(ledMutex);
        Serial.printf("LED index out of range: %d\n", led);
        return;
    }
    CRGB raw(red, green, blue);
    rawColorsArray[led] = raw;
    lastSetColor = raw;

    CRGB c = raw;
    if (gammaEnabled) napplyGamma_video(c, 2.2f); // freie FastLED-Funktion (CRGB hat keinen Member)
    ledsArray[led] = c;
    xSemaphoreGive(ledMutex);
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

    // Unter dem Mutex kann kein anderer Task die Puffer lesen oder ausgeben,
    // die alten Puffer dürfen deshalb sofort freigegeben werden.
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    CRGB* oldLeds = ledsArray;
    CRGB* oldRaw = rawColorsArray;
    if (count < ledCount) {
        // Überzählige LEDs dunkel schalten, bevor der Treiber sie nicht mehr ansteuert
        for (int i = count; i < ledCount; i++) oldLeds[i] = CRGB::Black;
        FastLED.show();
    }
    ledsArray = newLeds;
    rawColorsArray = newRaw;
    FastLED[0].setLeds(newLeds, count);
    ledCount = count;
    delete[] oldLeds;
    delete[] oldRaw;
    xSemaphoreGive(ledMutex);
}

void LEDController::showPixels() {
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    FastLED.show();
    xSemaphoreGive(ledMutex);
}

CRGB LEDController::getLEDColor(int ledIndex) {
    if (ledIndex == 0) {
        return lastSetColor;
    }
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    if (ledIndex < 0 || ledIndex >= ledCount) {
        xSemaphoreGive(ledMutex);
        Serial.printf("LED index out of range: %d\n", ledIndex);
        return CRGB::Black;
    }
    CRGB color = rawColorsArray[ledIndex];
    xSemaphoreGive(ledMutex);
    return color;
}

void LEDController::setBrightness(uint8_t value) {
    xSemaphoreTake(ledMutex, portMAX_DELAY);
    FastLED.setBrightness(value);
    FastLED.show();
    xSemaphoreGive(ledMutex);
}

void LEDController::setGamma(bool enabled) {
    gammaEnabled = enabled;
}

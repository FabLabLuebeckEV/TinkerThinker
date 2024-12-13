#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <FastLED.h>

class LEDController {
public:
    LEDController(int ledCount); // Neuer Konstruktor mit Parameter
    void init();
    void setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue);
    void showPixels();
    CRGB getLEDColor(int ledIndex);

private:
    int ledCount;
    CRGB* ledsArray; // Dynamisches Array
    int dataPin = 2; // Standarddatenpin, ggf. anpassen oder aus Config laden
};

#endif

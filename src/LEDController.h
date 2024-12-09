#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <FastLED.h>

// Definiere den Datenpin als Makro oder constexpr
#define LED_DATA_PIN 2 // Passe dies entsprechend deiner Hardware an

#define NUM_LEDS 30 // Anzahl der LEDs anpassen

class LEDController {
public:
    LEDController();
    void init();
    void setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue);
    void showPixels();
    CRGB getLEDColor(int ledIndex);

private:
    // Entferne dataPin als Member-Variable
    size_t ledCount;
    CRGB ledsArray[NUM_LEDS];
};

#endif

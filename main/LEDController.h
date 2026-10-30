#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <FastLED.h>

class LEDController {
public:
    LEDController(int ledCount); // Neuer Konstruktor mit Parameter
    void init();
    // LED-Anzahl zur Laufzeit aendern, ohne den FastLED-Treiber neu zu registrieren
    void setLedCount(int count);
    void setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue);
    void showPixels();
    CRGB getLEDColor(int ledIndex);
    void setBrightness(uint8_t value);
    void setGamma(bool enabled);

private:
    int ledCount;
    CRGB* ledsArray;      // Dynamisches Array (evtl. Gamma-korrigiert für Ausgabe)
    CRGB* rawColorsArray; // Unverfälschte RGB-Farben je LED für Auslesen in der GUI
    CRGB lastSetColor = CRGB(0, 0, 0); // Zuletzt gesetzte LED-Farbe
    int dataPin = 2; // Standarddatenpin, ggf. anpassen oder aus Config laden
    bool gammaEnabled = false;
};

#endif

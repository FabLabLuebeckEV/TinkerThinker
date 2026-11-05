// MotorController.h
#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

struct Motor {
    int pin1;
    int pin2;
    int channel_forward;
    int channel_reverse;
};

class MotorController {
public:
    // Neuer Konstruktor erwartet Arrays für Frequenz und Deadband
    struct DriveProfileConfig {
        enum class Mixer : uint8_t { Arcade, Tank };
        Mixer mixer = Mixer::Arcade;
        int axisDeadband = 16;
        float turnGain = 1.0f;
    };

    struct MotorCurveConfig {
        enum class Type : uint8_t { Linear, Expo };
        Type type = Type::Linear;
        float strength = 0.0f;
    };

    // Ansteuerung der H-Brücken (AT8833 / DRV8833)
    struct OutputConfig {
        // Slow Decay: in der Aus-Phase der PWM wird der Motor kurzgeschlossen (Bremse), der Strom
        // fließt weiter. Drehzahl folgt der PWM fast linear und der Motor hat schon bei kleiner
        // PWM Kraft. Fast Decay (alt): in der Aus-Phase läuft der Motor frei, bei kleiner PWM
        // kaum Kraft, dann plötzlich zu schnell.
        bool slowDecay = true;
        // Anlauf-Boost: beim Anfahren aus dem Stand oder bei Richtungswechsel kurz mit
        // kickPwm fahren, damit Getriebemotoren die Haftreibung überwinden. 0 ms = aus.
        int kickMs = 40;
        int kickPwm = 255;
    };

    static constexpr int MIN_PWM_FREQ = 100;
    static constexpr int MAX_PWM_FREQ = 40000;   // AT8833/DRV8833: bis ca. 50 kHz, mit Reserve

    MotorController(Motor motors[], size_t motorCount, const int motorFrequencies[], const int motorDeadbands[], const bool motorInvert[], bool motorSwap,
                    const DriveProfileConfig& driveProfile, const MotorCurveConfig& motorCurve, const OutputConfig& output);
    void init();
    // Neue Konfiguration übernehmen, ohne Objekt und PWM-Kanäle neu anzulegen
    void applyConfig(const int motorFrequencies[], const int motorDeadbands[], const bool motorInvert[], bool motorSwap,
                     const DriveProfileConfig& driveProfile, const MotorCurveConfig& motorCurve, const OutputConfig& output);
    // Regelmäßig aus der Loop aufrufen: beendet abgelaufene Anlauf-Boosts
    void update();
    void controlMotor(int index, int pwmValue);
    void handleMotorControl(int axisX, int axisY, int leftMotorIndex, int rightMotorIndex);

    void controlMotorForward(int motorIndex);
    void controlMotorBackward(int motorIndex);
    void controlMotorStop(int motorIndex);
    void controlMotorRaw(int motorIndex, int pwmValue);
    // Ausgegebene PWM (nach Deadband, ohne Anlauf-Boost), Vorzeichen wie angefordert
    int getMotorPWM(int motorIndex);
    // Zuletzt angeforderter Wert (vor Deadband), z. B. als Startwert für Rampen
    int getMotorRequest(int motorIndex);
    int getMotorFrequency(int motorIndex) const;
    void setSpeedMultiplier(float m);
    float getSpeedMultiplier() const { return speedMultiplier; }

private:
    struct MotorState {
        int dir = 0;              // -1, 0, 1 (nach Invertierung, wie an der H-Brücke)
        int targetMag = 0;        // Ziel-PWM 0..255
        int request = 0;          // angeforderter Wert vor Deadband
        bool kicking = false;
        uint32_t kickUntilMs = 0;
    };

    Motor* motors;
    size_t count;
    bool motorInvertArray[4];
    bool motorSwap;
    bool initialized = false;

    int freq[4];
    int deadband[4];
    float speedMultiplier = 1.0f;

    DriveProfileConfig driveProfile;
    MotorCurveConfig motorCurve;
    OutputConfig output;
    MotorState state[4];
    // Mehrere Tasks steuern die Motoren (Webserver, BT-Loop, Rampen); Zustand und
    // PWM-Ausgabe müssen zusammen passieren, sonst überschreibt ein alter Wert einen Stopp.
    SemaphoreHandle_t outMutex;

    void writeOutput(int index, int signedMag, int request);
    void applyPwm(int index, int dir, int mag);
    int scaleMovementToPWM(float movement);
    float normalizeAxis(int raw) const;
    float applyAxisDeadband(float value) const;
    void mixArcade(float throttle, float turn, float& left, float& right) const;
    float applyMotorCurve(float value) const;
};

#endif

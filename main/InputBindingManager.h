#ifndef INPUT_BINDING_MANAGER_H
#define INPUT_BINDING_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Bluepad32.h>

class TinkerThinkerBoard;
class ConfigManager;

class InputBindingManager {
public:
    InputBindingManager(TinkerThinkerBoard* board, ConfigManager* config);
    void reload();
    void process(ControllerPtr ctl, int controllerIndex);

private:
    TinkerThinkerBoard* board;
    ConfigManager* config;
    DynamicJsonDocument bindings;
    uint32_t prevButtons[BP32_MAX_GAMEPADS] = {0};
    uint32_t prevDpad[BP32_MAX_GAMEPADS] = {0};
    int servoBand[3] = {0,0,0};

    // motor_direct hold-coast state
    int      motorHoldPWM[4]             = {0,0,0,0};
    uint32_t motorHoldUntil[4]           = {0,0,0,0};
    bool     motorHoldActiveThisCycle[4] = {false,false,false,false};
    static const uint32_t MOTOR_HOLD_COAST_MS = 500;

    // Helpers
    int getAxis(ControllerPtr ctl, const char* name);
    void applyAction(JsonObject action, ControllerPtr ctl);
    void applyMotorHold(JsonObject action, uint32_t tickMs);
    uint32_t buttonMaskFromString(const String& code);
    uint32_t dpadMaskFromString(const String& dir);
};

#endif


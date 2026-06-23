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
    void tick();

private:
    TinkerThinkerBoard* board;
    ConfigManager* config;
    JsonDocument bindings;
    uint32_t prevButtons[BP32_MAX_GAMEPADS] = {0};
    uint32_t prevDpad[BP32_MAX_GAMEPADS] = {0};
    int servoBand[7] = {0,0,0,0,0,0,0};

    // servo_sweep state (pro Servo 0..6)
    bool    sweepActive[7]   = {false,false,false,false,false,false,false};
    int     sweepFrom[7]     = {0,0,0,0,0,0,0};
    int     sweepTo[7]       = {180,180,180,180,180,180,180};
    int     sweepStep[7]     = {2,2,2,2,2,2,2};
    int     sweepPos[7]      = {0,0,0,0,0,0,0};
    int     sweepDir[7]      = {1,1,1,1,1,1,1};
    uint32_t sweepNextMs[7]  = {0,0,0,0,0,0,0};

    // motor_ramp state (pro Motor 0..3)
    bool     rampActive[4]   = {false,false,false,false};
    int      rampTarget[4]   = {0,0,0,0};
    int      rampCurrent[4]  = {0,0,0,0};
    int      rampStep[4]     = {10,10,10,10};
    uint32_t rampNextMs[4]   = {0,0,0,0};

    // motor_direct hold-coast state
    int      motorHoldPWM[4]   = {0,0,0,0};
    uint32_t motorHoldUntil[4] = {0,0,0,0};
    static const uint32_t MOTOR_HOLD_COAST_MS = 500;

    // Helpers
    int getAxis(ControllerPtr ctl, const char* name);
    void applyAction(JsonObject action, ControllerPtr ctl);
    void applyMotorHold(JsonObject action, uint32_t tickMs);
    void startServoSweep(JsonObject action);
    void startMotorRamp(JsonObject action);
    void tickSweepsAndRamps(uint32_t now);
    uint32_t buttonMaskFromString(const String& code);
    uint32_t dpadMaskFromString(const String& dir);
};

#endif


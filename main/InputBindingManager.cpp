#include "InputBindingManager.h"
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"

InputBindingManager::InputBindingManager(TinkerThinkerBoard* b, ConfigManager* c)
    : board(b), config(c) {}

void InputBindingManager::reload() {
    bindings.clear();
    auto json = config->getControlBindingsJson();
    if (json.length() == 0) return;
    DeserializationError err = deserializeJson(bindings, json);
    if (err) {
        Serial.printf("InputBindingManager: Failed to parse bindings JSON: %s. Falling back to defaults.\n", err.c_str());
        deserializeJson(bindings, ConfigManager::getDefaultControlBindingsJson());
    }
    Serial.print("InputBindingManager: Loaded bindings: ");
    serializeJson(bindings, Serial);
    Serial.println();
}

int InputBindingManager::getAxis(ControllerPtr ctl, const char* name) {
    if (!name) return 0;
    if (!strcmp(name, "X")) return ctl->axisX();
    if (!strcmp(name, "Y")) return ctl->axisY();
    if (!strcmp(name, "RX")) return ctl->axisRX();
    if (!strcmp(name, "RY")) return ctl->axisRY();
    return 0;
}

uint32_t InputBindingManager::buttonMaskFromString(const String& code) {
    if (code == "BTN_A") return 1;
    if (code == "BTN_B") return 2;
    if (code == "BTN_X") return 4;
    if (code == "BTN_Y") return 8;
    if (code == "BUTTON_L1") return 16;
    if (code == "BUTTON_R1") return 32;
    if (code == "BUTTON_L2") return 64;
    if (code == "BUTTON_R2") return 128;
    if (code == "BUTTON_STICK_L") return 256;
    if (code == "BUTTON_STICK_R") return 512;
    // Misc-Tasten (Bluepad32 miscButtons) liegen ab Bit 16, siehe process()/Snapshot
    if (code == "MISC_SYSTEM") return 1u << 16;   // PS-/Home-Taste
    if (code == "MISC_SELECT") return 2u << 16;   // Share / Select / Back
    if (code == "MISC_START")  return 4u << 16;   // Options / Start
    return 0;
}

uint32_t InputBindingManager::dpadMaskFromString(const String& dir) {
    if (dir == "UP") return 1 << 0;
    if (dir == "DOWN") return 1 << 1;
    if (dir == "RIGHT") return 1 << 2;
    if (dir == "LEFT") return 1 << 3;
    return 0;
}

void InputBindingManager::applyAction(JsonObject action, ControllerPtr ctl) {
    const char* type = action["type"] | "";
    if (!strcmp(type, "drive_pair")) {
        const char* target = action["target"] | "gui";
        // axis values are taken from last evaluated axis binding; kept via closure in process
    } else if (!strcmp(type, "servo_set")) {
        int idx = action["servo"] | 0;
        int angle = action["angle"] | 0;
        board->setServoAngle(idx, angle);
    } else if (!strcmp(type, "servo_toggle_band")) {
        int idx = action["servo"] | 0;
        JsonArray bands = action["bands"].as<JsonArray>();
        if (!bands.isNull() && bands.size() > 0) {
            // find current band index by matching current angle to nearest band value
            int cur = board->getServoAngle(idx);
            int next = bands[0] | 0;
            // toggle: choose next different from current quantized band
            // simple toggle between two bands or cycle over many
            // determine closest band index
            int closest = 0; int bestd = 1000;
            for (size_t i=0;i<bands.size();++i){ int v = bands[i] | 0; int d = abs(v-cur); if (d < bestd){bestd=d; closest=i;}}
            int ni = (closest + 1) % bands.size();
            next = bands[ni] | next;
            board->setServoAngle(idx, next);
        }
    } else if (!strcmp(type, "servo_nudge")) {
        int idx = action["servo"] | 0;
        int delta = action["delta"] | 0;
        board->setServoAngle(idx, board->getServoAngle(idx) + delta);
    } else if (!strcmp(type, "led_set")) {
        const char* color = action["color"] | "#000000";
        int start = action["start"] | 0;
        int count = action["count"] | 1;
        long rgb = strtol(color+1, nullptr, 16);
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >> 8) & 0xFF;
        uint8_t b = (rgb) & 0xFF;
        for (int i= start; i< start+count; ++i) board->setLED(i, r,g,b);
        board->showLEDs();
    } else if (!strcmp(type, "gpio_set")) {
        int pin = action["pin"] | -1;
        int level = action["level"] | 0;
        if (pin >= 0) { pinMode(pin, OUTPUT); digitalWrite(pin, level ? HIGH : LOW); }
    } else if (!strcmp(type, "speed_adjust")) {
        float delta = action["delta"] | 0.0f;
        board->setSpeedMultiplier(board->getSpeedMultiplier() + delta);
    } else if (!strcmp(type, "speed_toggle")) {
        // Zwischen zwei Geschwindigkeiten umschalten, z.B. 0.5 <-> 1.0
        float low = action["low"] | 0.5f;
        float high = action["high"] | 1.0f;
        float cur = board->getSpeedMultiplier();
        float next = (cur > (low + high) * 0.5f) ? low : high;
        board->setSpeedMultiplier(next);
        Serial.printf("speed_toggle: %.2f -> %.2f\n", cur, board->getSpeedMultiplier());
    } else if (!strcmp(type, "motor_direct")) {
        int idx = action["motor"] | 0;
        int pwm = action["pwm"] | 0;
        if (idx >= 0 && idx < 4) {
            pwm = constrain(pwm, -255, 255);
            board->requestMotorDirectFromBT(idx, pwm);
        }
    } else if (!strcmp(type, "sumo_set")) {
        // SchrottBot-Sumo: Position direkt setzen (0..100 %)
        int role = sumoRoleFromString(action["role"] | "arms");
        if (role >= 0) board->setSumoPos(role, action["pct"] | 0.0f);
    } else if (!strcmp(type, "sumo_toggle")) {
        // Zwischen 0 % und 100 % umschalten (z.B. Arme auf/zu mit einer Taste)
        int role = sumoRoleFromString(action["role"] | "arms");
        if (role >= 0) {
            float cur = board->getSumoPos(role);
            board->setSumoPos(role, (cur < 50.0f) ? 100.0f : 0.0f);
        }
    } else if (!strcmp(type, "sumo_move")) {
        // läuft in tickSumo() solange die Taste gehalten wird; hier nichts tun
    } else if (!strcmp(type, "servo_sweep")) {
        startServoSweep(action);
    } else if (!strcmp(type, "motor_ramp")) {
        startMotorRamp(action);
    }
}

void InputBindingManager::applyMotorHold(JsonObject action, uint32_t tickMs) {
    int motorIdx = action["motor"] | 0;
    int pwm      = action["pwm"]   | 0;
    if (motorIdx < 0 || motorIdx >= 4) return;
    pwm = constrain(pwm, -255, 255);
    motorHoldPWM[motorIdx]   = pwm;
    motorHoldUntil[motorIdx] = tickMs + MOTOR_HOLD_COAST_MS;
}

void InputBindingManager::startServoSweep(JsonObject action) {
    int idx = action["servo"] | 0;
    int maxServos = config ? config->getActiveServoCount() : 7;
    if (idx < 0 || idx >= maxServos) return;
    int from = action["from"] | 0;
    int to   = action["to"]   | 180;
    int step = action["step"] | 2;
    if (step < 1) step = 1;
    // Toggle: aktiver Sweep wird gestoppt
    if (sweepActive[idx]) { sweepActive[idx] = false; return; }
    sweepFrom[idx] = constrain(from, 0, 180);
    sweepTo[idx]   = constrain(to,   0, 180);
    sweepStep[idx] = step;
    sweepPos[idx]  = sweepFrom[idx];
    sweepDir[idx]  = (sweepTo[idx] >= sweepFrom[idx]) ? 1 : -1;
    sweepNextMs[idx] = millis();
    sweepActive[idx] = true;
}

void InputBindingManager::startMotorRamp(JsonObject action) {
    int idx = action["motor"] | 0;
    if (idx < 0 || idx >= 4) return;
    int target = action["pwm"]  | 0;
    int step   = action["step"] | 10;
    if (step < 1) step = 1;
    rampTarget[idx]  = constrain(target, -255, 255);
    rampStep[idx]    = step;
    // Angeforderter Wert (vor Deadband), damit die Rampe ohne Sprung weiterläuft
    rampCurrent[idx] = board->getMotorRequest(idx);
    rampNextMs[idx]  = millis();
    rampActive[idx]  = true;
}

void InputBindingManager::tickSweepsAndRamps(uint32_t now) {
    // Servo-Sweeps (alle 20 ms ein Schritt)
    int maxServos = config ? config->getActiveServoCount() : 7;
    for (int i = 0; i < maxServos; i++) {
        if (!sweepActive[i]) continue;
        if (now < sweepNextMs[i]) continue;
        sweepNextMs[i] = now + 20;
        sweepPos[i] += sweepDir[i] * sweepStep[i];
        if (sweepDir[i] > 0 && sweepPos[i] >= sweepTo[i])   { sweepPos[i] = sweepTo[i];   sweepDir[i] = -1; }
        else if (sweepDir[i] < 0 && sweepPos[i] <= sweepFrom[i]) { sweepPos[i] = sweepFrom[i]; sweepDir[i] = 1; }
        board->setServoAngle(i, sweepPos[i]);
    }
    // Motor-Rampen (alle 20 ms ein Schritt)
    for (int i = 0; i < 4; i++) {
        if (!rampActive[i]) continue;
        if (now < rampNextMs[i]) continue;
        rampNextMs[i] = now + 20;
        if (rampCurrent[i] < rampTarget[i]) {
            rampCurrent[i] = min(rampCurrent[i] + rampStep[i], rampTarget[i]);
        } else if (rampCurrent[i] > rampTarget[i]) {
            rampCurrent[i] = max(rampCurrent[i] - rampStep[i], rampTarget[i]);
        }
        board->controlMotorDirect(i, rampCurrent[i]);
        if (rampCurrent[i] == rampTarget[i]) rampActive[i] = false;
    }
}

int InputBindingManager::sumoRoleFromString(const char* role) {
    if (!strcmp(role, "arms")) return TinkerThinkerBoard::SUMO_ARMS;
    if (!strcmp(role, "lift")) return TinkerThinkerBoard::SUMO_LIFT;
    if (!strcmp(role, "push")) return TinkerThinkerBoard::SUMO_PUSH;
    return -1;
}

// sumo_move {role, speed (%/s, Vorzeichen = Richtung)} an Taste/D-Pad: solange gehalten, fährt die Position
// stetig weiter und bleibt beim Loslassen stehen. Ausgewertet über den Controller-Snapshot, weil Controller
// beim Halten oft keine neuen Daten schicken (process() läuft nur bei neuen Daten).
void InputBindingManager::tickSumo(uint32_t now) {
    if (now - sumoLastMs < 10) return;
    uint32_t dt = now - sumoLastMs;
    sumoLastMs = now;
    if (dt > 100) dt = 100;   // nach einer Pause (Konfig-Reload, Start) keinen Sprung machen
    if (!bindings.is<JsonArray>()) return;
    float rate[TinkerThinkerBoard::SUMO_ROLES] = {0.0f, 0.0f, 0.0f};
    bool any = false;
    for (int c = 0; c < BP32_MAX_GAMEPADS; c++) {
        const ControllerInputSnapshot& snap = board->getControllerSnapshot(c);
        if (!snap.connected) continue;
        for (JsonObject b : bindings.as<JsonArray>()) {
            JsonObject action = b["action"].as<JsonObject>();
            if (strcmp(action["type"] | "", "sumo_move")) continue;
            JsonObject input = b["input"].as<JsonObject>();
            const char* inType = input["type"] | "";
            bool pressed = false;
            if (!strcmp(inType, "button")) pressed = snap.buttons & buttonMaskFromString(input["code"].as<String>());
            else if (!strcmp(inType, "dpad")) pressed = snap.dpad & dpadMaskFromString(input["dir"].as<String>());
            if (!pressed) continue;
            int role = sumoRoleFromString(action["role"] | "arms");
            if (role < 0) continue;
            rate[role] += action["speed"] | 150.0f;
            any = true;
        }
    }
    if (!any) return;
    for (int r = 0; r < TinkerThinkerBoard::SUMO_ROLES; r++) {
        if (rate[r] != 0.0f) board->nudgeSumo(r, rate[r] * dt / 1000.0f);
    }
}

void InputBindingManager::tick() {
    uint32_t now = millis();
    tickSweepsAndRamps(now);
    tickSumo(now);
    for (int m = 0; m < 4; m++) {
        if (motorHoldUntil[m] == 0) continue;
        if (now < motorHoldUntil[m]) {
            board->controlMotorDirect(m, motorHoldPWM[m]);
        } else {
            board->controlMotorDirect(m, 0);
            motorHoldUntil[m] = 0;
        }
    }
}

void InputBindingManager::process(ControllerPtr ctl, int idx) {
    static uint32_t lastReload = 0;
    uint32_t tickMs = millis();
    if (tickMs - lastReload > 2000) { reload(); lastReload = tickMs; }
    if (!bindings.is<JsonArray>()) return;
    JsonArray arr = bindings.as<JsonArray>();

    // Precompute axes
    int axX = ctl->axisX();
    int axY = ctl->axisY();
    int axRX = ctl->axisRX();
    int axRY = ctl->axisRY();
    uint32_t btns = ctl->buttons() | ((uint32_t)ctl->miscButtons() << 16);
    uint32_t dpad = ctl->dpad();

    for (JsonObject b : arr) {
        JsonObject input  = b["input"].as<JsonObject>();
        JsonObject action = b["action"].as<JsonObject>();
        const char* inType = input["type"] | "";

        if (!strcmp(inType, "axis_pair")) {
            const char* xname = input["x"] | "X";
            const char* yname = input["y"] | "Y";
            int dead = input["deadband"] | 16;
            int x = (!strcmp(xname,"X"))?axX:(!strcmp(xname,"Y"))?axY:(!strcmp(xname,"RX"))?axRX:axRY;
            int y = (!strcmp(yname,"X"))?axX:(!strcmp(yname,"Y"))?axY:(!strcmp(yname,"RX"))?axRX:axRY;

            const char* actType = action["type"] | "";
            if (!strcmp(actType, "drive_pair")) {
                const char* target = action["target"] | "gui";
                // Richtung/Drehung wird zentral über die BT-Joystick-Richtung gesetzt
                // (Config bt_swap_axes/bt_invert_x/bt_invert_y) - NICHT mehr pro Binding,
                // damit es keine Doppel-Anwendung gibt.
                static uint32_t lastPrint = 0;
                if ((abs(x) > dead || abs(y) > dead) && (millis() - lastPrint > 500)) {
                    Serial.printf("InputBindingManager: drive_pair target=%s raw=(%d,%d)\n", target, x, y);
                    lastPrint = millis();
                }
                if (!strcmp(target, "gui"))   board->requestDriveFromBT(x, y, false);
                else if (!strcmp(target, "other")) board->requestDriveOtherFromBT(x, y, false);
            } else if (!strcmp(actType, "servo_axes")) {
                int servo = action["servo"] | 0;
                float scale = action["scale"] | 1.0f;
                int sy = (input["invertY"] | false) ? -y : y;
                int val = (int)((sy / 512.0f) * 90.0f * scale + 90.0f);
                val = constrain(val, 0, 180);
                board->setServoAngle(servo, val);
            } else if (!strcmp(actType, "motor_axis")) {
                // Eine einzelne Stick-Achse → ein Motor (proportional).
                int motor       = action["motor"]    | 0;
                const char* sel = action["axis"]     | "y";   // "x" oder "y" des Achsenpaars
                float scale     = action["scale"]    | 1.0f;
                int mdead       = action["deadband"] | 16;
                bool inv        = action["invert"]   | false;
                int v = (!strcmp(sel, "x")) ? x : y;
                if (abs(v) < mdead) v = 0;
                int pwm = (int)((v / 512.0f) * 255.0f * scale);
                if (inv) pwm = -pwm;
                pwm = constrain(pwm, -255, 255);
                if (motor >= 0 && motor < 4) board->requestMotorDirectFromBT(motor, pwm);
            }
        } else if (!strcmp(inType, "button")) {
            String code = input["code"].as<String>();
            String edge = input["edge"].as<String>();
            uint32_t mask = buttonMaskFromString(code);
            bool pressed = (btns & mask);
            bool was     = (prevButtons[idx] & mask);
            if (edge == "press") {
                if (pressed && !was) applyAction(action, ctl);
            } else if (edge == "release") {
                if (!pressed && was) applyAction(action, ctl);
            } else if (edge == "hold") {
                if (pressed) {
                    if (!strcmp(action["type"] | "", "motor_direct"))
                        applyMotorHold(action, tickMs);
                    else
                        applyAction(action, ctl);
                }
            }
        } else if (!strcmp(inType, "dpad")) {
            String dir  = input["dir"].as<String>();
            String edge = input["edge"].as<String>();
            uint32_t mask = dpadMaskFromString(dir);
            bool pressed = (dpad & mask);
            bool was     = (prevDpad[idx] & mask);
            if (edge == "press") {
                if (pressed && !was) applyAction(action, ctl);
            } else if (edge == "release") {
                if (!pressed && was) applyAction(action, ctl);
            } else if (edge == "hold") {
                if (pressed) {
                    if (!strcmp(action["type"] | "", "motor_direct"))
                        applyMotorHold(action, tickMs);
                    else
                        applyAction(action, ctl);
                }
            }
        }
    }

    prevButtons[idx] = btns;
    prevDpad[idx] = dpad;
}

#include "InputBindingManager.h"
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"

InputBindingManager::InputBindingManager(TinkerThinkerBoard* b, ConfigManager* c)
    : board(b), config(c), bindings(8192) {}

void InputBindingManager::reload() {
    bindings.clear();
    auto json = config->getControlBindingsJson();
    if (json.length() == 0) return;
    DeserializationError err = deserializeJson(bindings, json);
    if (err) {
        // fallback to defaults
        deserializeJson(bindings, ConfigManager::getDefaultControlBindingsJson());
    }
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
    if (code == "BTN_A") return 2;
    if (code == "BTN_B") return 1;
    if (code == "BTN_X") return 8;
    if (code == "BTN_Y") return 4;
    if (code == "BUTTON_L1") return 16;
    if (code == "BUTTON_R1") return 32;
    if (code == "BUTTON_L2") return 64;
    if (code == "BUTTON_R2") return 128;
    if (code == "BUTTON_STICK_L") return 256;
    if (code == "BUTTON_STICK_R") return 512;
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
    } else if (!strcmp(type, "motor_direct")) {
        int idx = action["motor"] | 0;
        int pwm = action["pwm"] | 0;
        if (idx >= 0 && idx < 4) {
            pwm = constrain(pwm, -255, 255);
            board->requestMotorDirectFromBT(idx, pwm);
        }
    }
}

void InputBindingManager::process(ControllerPtr ctl, int idx) {
    static uint32_t lastReload = 0;
    uint32_t now = millis();
    if (now - lastReload > 2000) { reload(); lastReload = now; }
    if (!bindings.is<JsonArray>()) return;
    JsonArray arr = bindings.as<JsonArray>();
    // Precompute axes
    int axX = ctl->axisX();
    int axY = ctl->axisY();
    int axRX = ctl->axisRX();
    int axRY = ctl->axisRY();
    uint32_t btns = ctl->buttons();
    uint32_t dpad = ctl->dpad();

    for (JsonObject b : arr) {
        JsonObject input = b["input"].as<JsonObject>();
        JsonObject action = b["action"].as<JsonObject>();
        const char* inType = input["type"] | "";
        if (!strcmp(inType, "axis_pair")) {
            const char* xname = input["x"] | "X";
            const char* yname = input["y"] | "Y";
            int dead = input["deadband"] | 16;
            int x = (!strcmp(xname,"X"))?axX:(!strcmp(xname,"Y"))?axY:(!strcmp(xname,"RX"))?axRX:axRY;
            int y = (!strcmp(yname,"X"))?axX:(!strcmp(yname,"Y"))?axY:(!strcmp(yname,"RX"))?axRX:axRY;
            if (abs(x) <= dead && abs(y) <= dead) {
                // neutral: let arbiter decide via request methods
            }
            const char* actType = action["type"] | "";
            if (!strcmp(actType, "drive_pair")) {
                const char* target = action["target"] | "gui";
                if (!strcmp(target, "gui")) board->requestDriveFromBT(x, y);
                else if (!strcmp(target, "other")) board->requestDriveOtherFromBT(x, y);
            } else if (!strcmp(actType, "servo_axes")) {
                // Optional: map axes to a servo angle (0..180)
                int servo = action["servo"] | 0;
                float scale = action["scale"] | 1.0f;
                int val = (int)( (y / 512.0f) * 90.0f * scale + 90.0f );
                val = constrain(val, 0, 180);
                board->setServoAngle(servo, val);
            }
        } else if (!strcmp(inType, "button")) {
            String code = input["code"].as<String>();
            String edge = input["edge"].as<String>();
            uint32_t mask = buttonMaskFromString(code);
            bool now = (btns & mask);
            bool was = (prevButtons[idx] & mask);
            if (edge == "press") {
                if (now && !was) applyAction(action, ctl);
            } else if (edge == "release") {
                if (!now && was) applyAction(action, ctl);
            } else if (edge == "hold") {
                if (now) applyAction(action, ctl);
            }
        } else if (!strcmp(inType, "dpad")) {
            String dir = input["dir"].as<String>();
            String edge = input["edge"].as<String>();
            uint32_t mask = dpadMaskFromString(dir);
            bool now = (dpad & mask);
            bool was = (prevDpad[idx] & mask);
            if (edge == "press") {
                if (now && !was) applyAction(action, ctl);
            } else if (edge == "release") {
                if (!now && was) applyAction(action, ctl);
            } else if (edge == "hold") {
                if (now) applyAction(action, ctl);
            }
        }
    }

    prevButtons[idx] = btns;
    prevDpad[idx] = dpad;
}

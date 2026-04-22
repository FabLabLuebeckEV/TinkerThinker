// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Bluepad32.h>
#include <WiFi.h>
#include "TinkerThinkerBoard.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_bt.h"
#include <uni.h>

// Removed DFPlayer and HardwareSerial includes
#define MODE_BUTTON_PIN 39

// Erzeuge globalen Board- und ConfigManager
ConfigManager configManager;
TinkerThinkerBoard board(&configManager);

// Soundboard (DFPlayer) removed per request


// Button/DPad enums removed; new InputBindingManager handles mapping

// --- GLOBAL VARIABLES ---
ControllerPtr myControllers[BP32_MAX_GAMEPADS];
// Input binding processor
#include "InputBindingManager.h"
static InputBindingManager inputBindings(&board, &configManager);

long timestampServo = 0;
static uint32_t lastHeartbeatMs = 0;

enum class RadioMode { Normal, BluetoothOnly, WifiOnly };
static RadioMode radioMode = RadioMode::Normal;
static RadioMode lastAppliedMode = RadioMode::Normal;
static bool radioModeAppliedOnce = false;
static int modeStep = 0;
static uint32_t modeButtonPressStartMs = 0;
static bool modeButtonLongPressHandled = false;
static const uint32_t modeButtonHoldToSwitchMs = 700;
static uint32_t wifiPauseUntilMs = 0;
static bool wifiPausedForBt = false;
static const uint32_t wifiPauseOnConnectMs = 3000;
static uint32_t scanRestartAfterMs = 0;   // Pause nach Disconnect bevor Scan neu startet

static void setStatusLed(uint8_t r, uint8_t g, uint8_t b);
static void applyRadioMode(RadioMode mode);
static bool handleStartupReset();
static bool isModePressedStable();
static bool boardReady = false;
static void pollSerialCommands();
static void emitSerialReady();

static char serialCmdBuffer[1024];
static size_t serialCmdLen = 0;

static String formatBtMac() {
    const uint8_t* addr = BP32.localBdAddress();
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return String(buf);
}

static String sanitizeDeviceName(String name) {
    name.trim();
    name.replace("\r", "");
    name.replace("\n", "");
    if (name.length() > 32) {
        name.remove(32);
    }
    return name;
}

template <typename TDoc>
static void sendSerialJson(TDoc& doc) {
    String out;
    serializeJson(doc, out);
    Serial.print("TTJSON:");
    Serial.println(out);
}

template <typename TDoc>
static void fillSerialInfo(TDoc& doc) {
    doc["wifi_mode"] = configManager.getWifiMode();
    doc["wifi_ssid"] = configManager.getWifiSSID();
    doc["hotspot_ssid"] = configManager.getHotspotSSID();
    doc["ota_enabled"] = configManager.getOTAEnabled();
    doc["speed_multiplier"] = board.getSpeedMultiplier();
    doc["wifi_mac"] = WiFi.macAddress();
    doc["bt_mac"] = formatBtMac();
    doc["firmware"] = BP32.firmwareVersion();
    doc["uptime_ms"] = millis();
}

template <typename TDoc>
static void fillSerialConfig(TDoc& doc) {
    fillSerialInfo(doc);
    doc["wifi_password"] = configManager.getWifiPassword();
    doc["hotspot_password"] = configManager.getHotspotPassword();
    doc["led_count"] = configManager.getLedCount();
    doc["motor_left_gui"] = configManager.getMotorLeftGUI();
    doc["motor_right_gui"] = configManager.getMotorRightGUI();
    doc["drive_mixer"] = configManager.getDriveMixer();
    doc["drive_turn_gain"] = configManager.getDriveTurnGain();
    doc["drive_axis_deadband"] = configManager.getDriveAxisDeadband();
    doc["motor_curve_type"] = configManager.getMotorCurveType();
    doc["motor_curve_strength"] = configManager.getMotorCurveStrength();
    doc["bt_scan_on_normal_ms"] = configManager.getBtScanOnNormal();
    doc["bt_scan_off_normal_ms"] = configManager.getBtScanOffNormal();
    doc["bt_scan_on_sta_ms"] = configManager.getBtScanOnSta();
    doc["bt_scan_off_sta_ms"] = configManager.getBtScanOffSta();
    doc["bt_scan_on_ap_ms"] = configManager.getBtScanOnAp();
    doc["bt_scan_off_ap_ms"] = configManager.getBtScanOffAp();
}

static void emitSerialReady() {
    StaticJsonDocument<384> doc;
    doc["event"] = "ready";
    fillSerialInfo(doc);
    sendSerialJson(doc);
}

static void handleSerialCommandLine(const char* rawLine) {
    String line = rawLine ? String(rawLine) : String();
    line.trim();
    if (!line.length()) return;
    if (line.startsWith("TTCMD:")) {
        line.remove(0, 6);
        line.trim();
    }

    StaticJsonDocument<1024> cmd;
    DeserializationError err = deserializeJson(cmd, line);
    if (err) {
        StaticJsonDocument<256> resp;
        resp["event"] = "error";
        resp["message"] = "invalid_json";
        sendSerialJson(resp);
        return;
    }

    const char* command = cmd["cmd"] | "";
    StaticJsonDocument<1024> resp;
    resp["cmd"] = command;

    if (!strcmp(command, "ping")) {
        resp["event"] = "pong";
        resp["uptime_ms"] = millis();
        sendSerialJson(resp);
        return;
    }

    if (!strcmp(command, "get_info")) {
        resp["event"] = "info";
        fillSerialInfo(resp);
        sendSerialJson(resp);
        return;
    }

    if (!strcmp(command, "get_config")) {
        resp["event"] = "config";
        fillSerialConfig(resp);
        sendSerialJson(resp);
        return;
    }

    if (!strcmp(command, "set_name")) {
        const char* requestedName = cmd["name"] | "";
        String newName = sanitizeDeviceName(String(requestedName));
        if (!newName.length()) {
            resp["event"] = "error";
            resp["message"] = "empty_name";
            sendSerialJson(resp);
            return;
        }
        configManager.setHotspotSSID(newName);
        bool saved = configManager.saveConfig();
        resp["event"] = saved ? "config_saved" : "error";
        resp["saved"] = saved;
        resp["reboot_required"] = true;
        resp["hotspot_ssid"] = configManager.getHotspotSSID();
        if (!saved) resp["message"] = "save_failed";
        sendSerialJson(resp);
        return;
    }

    if (!strcmp(command, "set_config")) {
        JsonObject cfg = cmd["config"].as<JsonObject>();
        if (cfg.isNull()) {
            cfg = cmd.as<JsonObject>();
        }

        bool touched = false;
        bool reapplyHardware = false;
        bool rebootRequired = false;

        if (cfg.containsKey("wifi_mode")) {
            const char* mode = cfg["wifi_mode"] | "AP";
            configManager.setWifiMode(String(mode));
            touched = true;
            rebootRequired = true;
        }
        if (cfg.containsKey("wifi_ssid")) {
            const char* rawSsid = cfg["wifi_ssid"] | "";
            String ssid = String(rawSsid);
            ssid.trim();
            configManager.setWifiSSID(ssid);
            touched = true;
            rebootRequired = true;
        }
        if (cfg.containsKey("wifi_password")) {
            const char* rawPass = cfg["wifi_password"] | "";
            String pass = String(rawPass);
            pass.trim();
            configManager.setWifiPassword(pass);
            touched = true;
            rebootRequired = true;
        }
        if (cfg.containsKey("hotspot_ssid")) {
            const char* rawHotspot = cfg["hotspot_ssid"] | "";
            String ssid = sanitizeDeviceName(String(rawHotspot));
            if (ssid.length()) {
                configManager.setHotspotSSID(ssid);
                touched = true;
                rebootRequired = true;
            }
        }
        if (cfg.containsKey("hotspot_password")) {
            const char* rawPass = cfg["hotspot_password"] | "";
            configManager.setHotspotPassword(String(rawPass));
            touched = true;
            rebootRequired = true;
        }
        if (cfg.containsKey("ota_enabled")) {
            configManager.setOTAEnabled(cfg["ota_enabled"].as<bool>());
            touched = true;
        }
        if (cfg.containsKey("led_count")) {
            configManager.setLedCount(cfg["led_count"].as<int>());
            touched = true;
            reapplyHardware = true;
        }
        if (cfg.containsKey("motor_left_gui")) {
            configManager.setMotorLeftGUI(constrain(cfg["motor_left_gui"].as<int>(), 0, 3));
            touched = true;
            reapplyHardware = true;
        }
        if (cfg.containsKey("motor_right_gui")) {
            configManager.setMotorRightGUI(constrain(cfg["motor_right_gui"].as<int>(), 0, 3));
            touched = true;
            reapplyHardware = true;
        }
        if (cfg.containsKey("bt_scan_on_normal_ms")) {
            configManager.setBtScanOnNormal(cfg["bt_scan_on_normal_ms"].as<int>());
            touched = true;
        }
        if (cfg.containsKey("bt_scan_off_normal_ms")) {
            configManager.setBtScanOffNormal(cfg["bt_scan_off_normal_ms"].as<int>());
            touched = true;
        }
        if (cfg.containsKey("bt_scan_on_sta_ms")) {
            configManager.setBtScanOnSta(cfg["bt_scan_on_sta_ms"].as<int>());
            touched = true;
        }
        if (cfg.containsKey("bt_scan_off_sta_ms")) {
            configManager.setBtScanOffSta(cfg["bt_scan_off_sta_ms"].as<int>());
            touched = true;
        }
        if (cfg.containsKey("bt_scan_on_ap_ms")) {
            configManager.setBtScanOnAp(cfg["bt_scan_on_ap_ms"].as<int>());
            touched = true;
        }
        if (cfg.containsKey("bt_scan_off_ap_ms")) {
            configManager.setBtScanOffAp(cfg["bt_scan_off_ap_ms"].as<int>());
            touched = true;
        }

        if (!touched) {
            resp["event"] = "error";
            resp["message"] = "no_supported_fields";
            sendSerialJson(resp);
            return;
        }

        bool saved = configManager.saveConfig();
        if (saved && reapplyHardware) {
            board.reApplyConfig();
        }

        resp["event"] = saved ? "config_saved" : "error";
        resp["saved"] = saved;
        resp["reboot_required"] = rebootRequired;
        resp["reapplied"] = saved && reapplyHardware;
        fillSerialConfig(resp);
        if (!saved) resp["message"] = "save_failed";
        sendSerialJson(resp);
        return;
    }

    if (!strcmp(command, "reset_config")) {
        bool ok = configManager.resetConfig();
        if (ok) {
            board.reApplyConfig();
        }
        resp["event"] = ok ? "config_reset" : "error";
        resp["saved"] = ok;
        resp["reboot_required"] = true;
        if (!ok) resp["message"] = "reset_failed";
        sendSerialJson(resp);
        return;
    }

    if (!strcmp(command, "reboot")) {
        resp["event"] = "restarting";
        sendSerialJson(resp);
        Serial.flush();
        delay(50);
        ESP.restart();
        return;
    }

    resp["event"] = "error";
    resp["message"] = "unknown_command";
    sendSerialJson(resp);
}

static void pollSerialCommands() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            serialCmdBuffer[serialCmdLen] = '\0';
            handleSerialCommandLine(serialCmdBuffer);
            serialCmdLen = 0;
            continue;
        }
        if (serialCmdLen < (sizeof(serialCmdBuffer) - 1)) {
            serialCmdBuffer[serialCmdLen++] = c;
        } else {
            serialCmdLen = 0;
            StaticJsonDocument<256> resp;
            resp["event"] = "error";
            resp["message"] = "command_too_long";
            sendSerialJson(resp);
        }
    }
}

static int findControllerIndex(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (myControllers[i] == ctl) return i;
    }
    return -1;
}

void processMouse(ControllerPtr ctl);        // Stub, falls unbenutzt einfach leer lassen
void processKeyboard(ControllerPtr ctl);     //   "
void processBalanceBoard(ControllerPtr ctl); //   "
void processMouse(ControllerPtr) {}
void processKeyboard(ControllerPtr) {}
void processBalanceBoard(ControllerPtr) {}


// This callback gets called any time a new gamepad is connected.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Console.printf("CALLBACK: Controller is connected, index=%d\n", i);
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            board.setLED(0, 0, 255, 0); // Green
            board.showLEDs();
            if (radioMode == RadioMode::Normal) {
                wifiPauseUntilMs = millis() + wifiPauseOnConnectMs;
            }
            break;
        }
    }
    if (!foundEmptySlot) {
        Console.println("CALLBACK: Controller connected, but could not find empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Console.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            board.setLED(0, 255, 0, 0); // Red
            board.showLEDs();
            // Stop all motors as a safety measure
            for (int j = 0; j < 4; j++) {
                board.controlMotorStop(j);
            }
            // BTstack braucht ~500ms um Disconnect vollständig abzuschliessen.
            // Danach erst Scan neu starten, sonst kommt error=0x0c (Command Disallowed).
            scanRestartAfterMs = millis() + 600;
            break;
        }
    }

    if (!foundController) {
        Console.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

// Button/axis handling now driven by InputBindingManager

void processGamepad(ControllerPtr ctl) {
    // Dispatch to input binding manager
    int idx = findControllerIndex(ctl);
    if (idx >= 0) inputBindings.process(ctl, idx);

    // Update player LEDs to show battery level
    float voltage = board.getBatteryVoltage();
    if (voltage > 4.0) {
        ctl->setPlayerLEDs(0b1111);
    } else if (voltage > 3.8) {
        ctl->setPlayerLEDs(0b0111);
    } else if (voltage > 3.5) {
        ctl->setPlayerLEDs(0b0011);
    } else {
        ctl->setPlayerLEDs(0b0001);
    }
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else if (myController->isMouse()) {
                processMouse(myController);
            } else if (myController->isKeyboard()) {
                processKeyboard(myController);
            } else if (myController->isBalanceBoard()) {
                processBalanceBoard(myController);
            } else {
                Console.printf("Unsupported controller\n");
            }
        }
    }
}

// Arduino setup function. Runs in CPU 1
void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

    Serial.begin(115200);

    // DFPlayer initialization removed

    if (!configManager.init()) {
        Serial.println("Failed to init ConfigManager!");
    }
    board.begin();
    boardReady = true;
    // GPIO39 is input-only on ESP32 and has no internal pull-up.
    // The board uses external pull-up with active-low button wiring.
    pinMode(MODE_BUTTON_PIN, INPUT);
    if (handleStartupReset()) {
        return;
    }

    board.startServices();

    BP32.setup(&onConnectedController, &onDisconnectedController, true);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
    BP32.enableNewBluetoothConnections(false);
    sm_set_secure_connections_only_mode(false);                        // SC ausschalten
    uni_bt_allowlist_set_enabled(false);                             // Allowlist ausschalten

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // forgetBluetoothKeys() wird nur beim Factory-Reset (handleStartupReset) aufgerufen,
    // nicht bei jedem normalen Boot – sonst schlägt die Re-Auth aller gepairten Controller fehl.
    // BP32.forgetBluetoothKeys(); // Removed: Causes pairing issues with clones

    // Load input bindings from config
    inputBindings.reload();

    applyRadioMode(radioMode);
    emitSerialReady();
}

// Arduino loop function. Runs in CPU 1.
// Duty-cycle configuration for BT scanning (defaults overridden by ConfigManager)
static uint32_t SCAN_ON_MS_NORMAL = 500;    // balanced when WiFi is idle
static uint32_t SCAN_OFF_MS_NORMAL = 500;
static uint32_t SCAN_ON_MS_STA_CONNECT = 150;  // give STA connect more airtime
static uint32_t SCAN_OFF_MS_STA_CONNECT = 850;
static uint32_t SCAN_ON_MS_AP_ACTIVE = 100;     // AP serving clients: keep scanning minimal
static uint32_t SCAN_OFF_MS_AP_ACTIVE = 1900;

static bool scanEnabled = false;
static uint32_t nextToggleAt = 0;
static uint32_t curOnMs = SCAN_ON_MS_NORMAL;
static uint32_t curOffMs = SCAN_OFF_MS_NORMAL;
static enum { PHASE_ON, PHASE_OFF } phase = PHASE_OFF;

static void setStatusLed(uint8_t r, uint8_t g, uint8_t b) {
    if (!boardReady) return;
    board.setLED(0, r, g, b);
    board.showLEDs();
}

static bool handleStartupReset() {
    const uint32_t holdMs = 10000;
    const uint32_t stepMs = 250;
    const uint32_t armWindowMs = 2500;

    // Robust arming: allow a short window after boot where press can start.
    // This avoids missing reset when boot/init timing shifts.
    uint32_t armStart = millis();
    while ((millis() - armStart) < armWindowMs) {
        if (isModePressedStable()) break;
        delay(10);
    }
    if (!isModePressedStable()) {
        return false;
    }

    uint32_t start = millis();
    int phaseIndex = 0;
    while ((millis() - start) < holdMs) {
        if (!isModePressedStable()) {
            setStatusLed(60, 60, 60);
            return false;
        }
        if ((phaseIndex % 3) == 0) setStatusLed(255, 0, 0);       // red
        else if ((phaseIndex % 3) == 1) setStatusLed(255, 255, 255); // white
        else setStatusLed(255, 120, 0);                           // orange
        delay(stepMs);
        phaseIndex++;
    }

    bool ok = configManager.resetConfig();
    if (!ok) {
        Serial.println("Config reset failed!");
    }
    BP32.forgetBluetoothKeys();
    delay(200);
    ESP.restart();
    return true;
}

static bool isModePressedStable() {
    // Majority vote over a few samples to filter bounce/noise on input-only GPIO39.
    int lowCount = 0;
    for (int i = 0; i < 7; ++i) {
        if (digitalRead(MODE_BUTTON_PIN) == LOW) lowCount++;
        delay(2);
    }
    return lowCount >= 5;
}

static void applyRadioMode(RadioMode mode) {
    if (radioModeAppliedOnce && mode == lastAppliedMode) return;
    radioModeAppliedOnce = true;
    lastAppliedMode = mode;

    if (mode == RadioMode::Normal) {
        board.requestWifiEnable();
        BP32.enableNewBluetoothConnections(false);
        scanEnabled = false;
        phase = PHASE_OFF;
        nextToggleAt = millis();
        setStatusLed(60, 60, 60); // dim white
    } else if (mode == RadioMode::BluetoothOnly) {
        board.requestWifiDisable(false);
        BP32.enableNewBluetoothConnections(true);
        scanEnabled = true;
        phase = PHASE_ON;
        setStatusLed(0, 0, 255);
    } else if (mode == RadioMode::WifiOnly) {
        // First drop BT connections, then disable scanning, then set color
        for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
            if (myControllers[i] && myControllers[i]->isConnected()) {
                myControllers[i]->disconnect();
            }
        }
        BP32.enableNewBluetoothConnections(false);
        scanEnabled = false;
        phase = PHASE_OFF;
        board.requestWifiEnable();
        setStatusLed(255, 120, 0);
    }
}

void loop() {
    pollSerialCommands();
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();
    inputBindings.tick();
    // Refresh scan timings from config (lightweight)
    SCAN_ON_MS_NORMAL       = configManager.getBtScanOnNormal();
    SCAN_OFF_MS_NORMAL      = configManager.getBtScanOffNormal();
    SCAN_ON_MS_STA_CONNECT  = configManager.getBtScanOnSta();
    SCAN_OFF_MS_STA_CONNECT = configManager.getBtScanOffSta();
    SCAN_ON_MS_AP_ACTIVE    = configManager.getBtScanOnAp();
    SCAN_OFF_MS_AP_ACTIVE   = configManager.getBtScanOffAp();

    // Mode button handling (active-low, hold-to-switch for noise immunity)
    bool modeNow = (digitalRead(MODE_BUTTON_PIN) == LOW);
    uint32_t nowMs = millis();
    if (modeNow) {
        if (modeButtonPressStartMs == 0) {
            modeButtonPressStartMs = nowMs;
            modeButtonLongPressHandled = false;
        } else if (!modeButtonLongPressHandled && (nowMs - modeButtonPressStartMs) >= modeButtonHoldToSwitchMs) {
            modeStep = (modeStep + 1) % 4;
            if (modeStep == 0) radioMode = RadioMode::Normal;
            else if (modeStep == 1) radioMode = RadioMode::WifiOnly;
            else if (modeStep == 2) radioMode = RadioMode::BluetoothOnly;
            else radioMode = RadioMode::WifiOnly;
            modeButtonLongPressHandled = true;
        }
    } else {
        modeButtonPressStartMs = 0;
        modeButtonLongPressHandled = false;
    }
    applyRadioMode(radioMode);

    // Temporary WiFi pause to boost BT during connect (only in Normal mode)
    // Kein enableNewBluetoothConnections(true) wenn Controller bereits verbunden –
    // das Toggling stört BTstack und kann den PS3/DS3 zum Disconnect bringen.
    bool anyControllerNow = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] && myControllers[i]->isConnected()) { anyControllerNow = true; break; }
    }
    if (radioMode == RadioMode::Normal) {
        uint32_t now = millis();
        if (wifiPauseUntilMs > now) {
            if (!wifiPausedForBt) {
                board.requestWifiDisable(false);
                if (!anyControllerNow)
                    BP32.enableNewBluetoothConnections(true);
                wifiPausedForBt = true;
            }
        } else if (wifiPausedForBt) {
            board.requestWifiEnable();
            wifiPausedForBt = false;
        }
    } else if (wifiPausedForBt) {
        wifiPausedForBt = false;
        wifiPauseUntilMs = 0;
    }

    // Determine current scenario and duty cycle settings
    bool anyController = anyControllerNow;

    wifi_mode_t mode = WiFi.getMode();
    bool staConnecting = (mode == WIFI_STA || mode == WIFI_AP_STA) && (WiFi.status() != WL_CONNECTED);
    bool apActive = (mode == WIFI_AP || mode == WIFI_AP_STA) && (WiFi.softAPgetStationNum() > 0);
    bool wifiDisabledUntilRestart = (mode == WIFI_OFF);

    if (radioMode == RadioMode::BluetoothOnly) {
        if (!scanEnabled) {
            BP32.enableNewBluetoothConnections(true);
            scanEnabled = true;
        }
        vTaskDelay(10);
        return;
    }

    if (radioMode == RadioMode::WifiOnly) {
        if (scanEnabled) {
            BP32.enableNewBluetoothConnections(false);
            scanEnabled = false;
            phase = PHASE_OFF;
        }
        vTaskDelay(10);
        return;
    }

    if (wifiDisabledUntilRestart) {
        // Keep Bluetooth scanning fully enabled while Wi-Fi is down.
        if (!anyController) {
            if (!scanEnabled) {
                BP32.enableNewBluetoothConnections(true);
                scanEnabled = true;
                phase = PHASE_ON;
            }
        } else if (scanEnabled) {
            BP32.enableNewBluetoothConnections(false);
            scanEnabled = false;
            phase = PHASE_OFF;
        }
        vTaskDelay(10);
        return;
    }

    // Policy:
    // - If a controller is already connected: don't scan.
    // - Else if AP has clients: very low-duty scan.
    // - Else if STA is connecting: low-duty scan.
    // - Else: balanced scan.
    uint32_t targetOn = SCAN_ON_MS_NORMAL;
    uint32_t targetOff = SCAN_OFF_MS_NORMAL;
    if (anyController) {
        targetOn = 0; targetOff = 1000;  // keep scanning off
    } else if (apActive) {
        targetOn = SCAN_ON_MS_AP_ACTIVE; targetOff = SCAN_OFF_MS_AP_ACTIVE;
    } else if (staConnecting) {
        targetOn = SCAN_ON_MS_STA_CONNECT; targetOff = SCAN_OFF_MS_STA_CONNECT;
    }

    if (targetOn != curOnMs || targetOff != curOffMs) {
        // Scenario changed: reset scheduler
        curOnMs = targetOn; curOffMs = targetOff;
        phase = PHASE_OFF;
        scanEnabled = false;
        BP32.enableNewBluetoothConnections(false);
        nextToggleAt = millis() + curOffMs;
    }

    // Toggle scanning according to duty cycle (unless anyController)
    uint32_t now = millis();
    if (!anyController && now < scanRestartAfterMs) {
        // Kurz nach Disconnect warten bevor Scan startet (verhindert 0x0c error)
        vTaskDelay(10);
        return;
    }
    if (!anyController) {
        if (now >= nextToggleAt) {
            if (phase == PHASE_OFF) {
                // Turn scanning on for the on-duration
                scanEnabled = (curOnMs > 0);
                BP32.enableNewBluetoothConnections(scanEnabled);
                phase = PHASE_ON;
                nextToggleAt = now + (curOnMs > 0 ? curOnMs : curOffMs);
            } else {
                // Turn scanning off for the off-duration
                scanEnabled = false;
                BP32.enableNewBluetoothConnections(false);
                phase = PHASE_OFF;
                nextToggleAt = now + curOffMs;
            }
        }
    } else if (scanEnabled) {
        // Ensure scanning is off while a controller is connected
        BP32.enableNewBluetoothConnections(false);
        scanEnabled = false;
        phase = PHASE_OFF;
        nextToggleAt = now + 1000;
    }
    //board.updateWebClients();

    // Heartbeat alle 5s – zeigt dass der ESP läuft
    uint32_t nowHb = millis();
    if (nowHb - lastHeartbeatMs >= 5000) {
        lastHeartbeatMs = nowHb;
        bool ctrlConnected = false;
        for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
            if (myControllers[i] && myControllers[i]->isConnected()) { ctrlConnected = true; break; }
        }
        Console.printf("[HB] uptime=%lus  controller=%s  bat=%.2fV\n",
            nowHb / 1000,
            ctrlConnected ? "verbunden" : "getrennt",
            board.getBatteryVoltage());
    }

    // A delay is required to prevent the watchdog timer from triggering.
    vTaskDelay(10);
}

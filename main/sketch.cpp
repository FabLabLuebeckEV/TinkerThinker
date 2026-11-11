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
#include "BtPairing.h"
#include <esp_mac.h>

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

// Controller-Suche: Die Gerätesuche (Inquiry) läuft nur in einem Such-Fenster (nach dem Start,
// per MODE-Taste oder per Browser). WLAN bleibt dabei an, die Koexistenz des ESP32 teilt die
// Funkzeit auf. Gekoppelte Controller verbinden sich jederzeit ohne Suche (Page Scan ist immer an).
static const uint32_t searchWindowMs = 60000;
static uint32_t modeButtonPressStartMs = 0;
static bool modeButtonLongPressHandled = false;
static const uint32_t modeButtonHoldToSwitchMs = 700;
static bool modeButtonSeenReleased = false;
static volatile bool inquiryRunning = false;   // auch im BTstack-Thread gelesen (Abgleich)
// Periodische Suche: Länge und Perioden in Einheiten von 1,28 s (Bluepad32-Properties)
static const uint8_t kInquiryLength = 2;       // 2,56 s suchen
static const uint16_t kInquiryMinPeriod = 4;   // Periode 5,12 s ...
static const uint16_t kInquiryMaxPeriod = 5;   // ... bis 6,4 s
static uint32_t scanRestartAfterMs = 0;   // Pause nach Disconnect bevor Scan neu startet
static bool statusLedShowsSearch = false;

static void setStatusLed(uint8_t r, uint8_t g, uint8_t b);
static bool handleStartupReset();
static bool isModePressedStable();
static bool boardReady = false;
static void pollSerialCommands();
static void emitSerialReady();

static char serialCmdBuffer[16384];
static size_t serialCmdLen = 0;
static bool serialCmdOverflow = false;

static void applyWhitelistFromConfig() {
    // Geprüft wird in BtPairing (eigener Filter), nicht in der Bluepad32-Allowlist:
    // keine NVS-Schreibzugriffe, und die Browser-Suche sieht auch nicht erlaubte Controller.
    BtPairing::applyWhitelist(configManager.getBtWhitelistEnabled(), configManager.getBtWhitelist());
}

// Bluepad32-Einstellungen, die im NVS liegen, nur schreiben, wenn sie abweichen.
static void prepareBluetoothStack() {
    // Alte Allowlist aus früheren Firmware-Versionen abschalten, sie würde die Suche filtern
    if (uni_bt_allowlist_is_enabled()) {
        uni_bt_allowlist_set_enabled(false);
    }
    const bd_addr_t* addrs = nullptr;
    int total = 0;
    uni_bt_allowlist_get_all(&addrs, &total);
    static const bd_addr_t zeroAddr = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < total; i++) {
        if (memcmp(addrs[i], zeroAddr, sizeof(bd_addr_t)) != 0) {
            uni_bt_allowlist_remove_all();
            break;
        }
    }

    // Periodische Suche in Blöcken: 2,56 s suchen, dann 2,5-3,8 s Pause (Perioden 5,12-6,4 s).
    // 2,56 s reichen, um einen Controller im Pairing-Modus zu finden (beide Inquiry-Trains),
    // und die Pausen geben dem WLAN zusammenhängende Funkzeit für Anmeldung und DHCP.
    if (uni_bt_get_gap_inquiry_length() != kInquiryLength) uni_bt_set_gap_inquiry_length(kInquiryLength);
    if (uni_bt_get_gap_min_periodic_length() != kInquiryMinPeriod) uni_bt_set_gap_min_peridic_length(kInquiryMinPeriod);
    if (uni_bt_get_gap_max_periodic_length() != kInquiryMaxPeriod) uni_bt_set_gap_max_peridic_length(kInquiryMaxPeriod);
}

static void startControllerSearch(uint32_t durationMs) {
    BtPairing::startSearch(durationMs);
    Console.printf("Controller-Suche gestartet (%lu s)\n", (unsigned long)(durationMs / 1000));
}

// WLAN-MAC direkt aus dem eFuse lesen: WiFi.macAddress() liefert im reinen Hotspot-Modus
// 00:00:00:00:00:00, weil die Station-Schnittstelle dann nicht gestartet ist.
static String formatWifiMac() {
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        return String("Fehler: ") + esp_err_to_name(err);
    }
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

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
    Serial.print("TTJSON:");
    serializeJson(doc, Serial);
    Serial.println();
}

static void fillSerialInfo(JsonDocument& doc) {
    doc["wifi_mode"] = configManager.getWifiMode();
    doc["wifi_ssid"] = configManager.getWifiSSID();
    doc["hotspot_ssid"] = configManager.getHotspotSSID();
    doc["ota_enabled"] = configManager.getOTAEnabled();
    doc["speed_multiplier"] = board.getSpeedMultiplier();
    doc["wifi_mac"] = formatWifiMac();
    doc["bt_mac"] = formatBtMac();
    doc["firmware"] = BP32.firmwareVersion();
    doc["uptime_ms"] = millis();
}

static void fillSerialConfig(JsonDocument& doc) {
    fillSerialInfo(doc);
    doc["wifi_password"] = configManager.getWifiPassword();
    doc["hotspot_password"] = configManager.getHotspotPassword();

    JsonArray invArr = doc["motor_invert"].to<JsonArray>();
    for (int i = 0; i < 4; i++) invArr.add(configManager.getMotorInvert(i));

    doc["motor_swap"] = configManager.getMotorSwap();
    doc["motor_left_gui"] = configManager.getMotorLeftGUI();
    doc["motor_right_gui"] = configManager.getMotorRightGUI();
    doc["motor_gripper_gui"] = configManager.getMotorGripperGUI();
    doc["gui_mode"] = configManager.getGuiMode();
    doc["legacy_board"] = configManager.isLegacyBoard();
    doc["servo_count"] = configManager.getActiveServoCount();

    JsonArray dbArr = doc["motor_deadband"].to<JsonArray>();
    for (int i = 0; i < 4; i++) dbArr.add(configManager.getMotorDeadband(i));

    JsonArray trimArr = doc["motor_trim"].to<JsonArray>();
    for (int i = 0; i < 4; i++) trimArr.add(configManager.getMotorTrim(i));

    JsonArray freqArr = doc["motor_frequency"].to<JsonArray>();
    for (int i = 0; i < 4; i++) freqArr.add(configManager.getMotorFrequency(i));

    doc["led_count"] = configManager.getLedCount();
    doc["led_brightness"] = configManager.getLedBrightness();
    doc["led_gamma"] = configManager.getLedGamma();
    doc["ws_invert_x"] = configManager.getWsInvertX();
    doc["ws_invert_y"] = configManager.getWsInvertY();
    doc["ws_swap_sides"] = configManager.getWsSwapSides();
    doc["bt_invert_x"] = configManager.getBtInvertX();
    doc["bt_invert_y"] = configManager.getBtInvertY();
    doc["bt_swap_axes"] = configManager.getBtSwapAxes();
    doc["ota_enabled"] = configManager.getOTAEnabled();

    doc["drive_mixer"] = configManager.getDriveMixer();
    doc["drive_turn_gain"] = configManager.getDriveTurnGain();
    doc["drive_axis_deadband"] = configManager.getDriveAxisDeadband();
    doc["motor_curve_type"] = configManager.getMotorCurveType();
    doc["motor_curve_strength"] = configManager.getMotorCurveStrength();
    doc["motor_decay"] = configManager.getMotorDecay();
    doc["motor_kick_ms"] = configManager.getMotorKickMs();
    doc["motor_kick_pwm"] = configManager.getMotorKickPwm();
    doc["motor_stop_mode"] = configManager.getMotorStopMode();
    doc["motor_stall_amps"] = configManager.getMotorStallAmps();
    doc["motor_stall_ms"] = configManager.getMotorStallMs();

    doc["bt_scan_on_normal_ms"] = configManager.getBtScanOnNormal();
    doc["bt_scan_off_normal_ms"] = configManager.getBtScanOffNormal();
    doc["bt_scan_on_sta_ms"] = configManager.getBtScanOnSta();
    doc["bt_scan_off_sta_ms"] = configManager.getBtScanOffSta();
    doc["bt_scan_on_ap_ms"] = configManager.getBtScanOnAp();
    doc["bt_scan_off_ap_ms"] = configManager.getBtScanOffAp();

    JsonArray servoArr = doc["servo_settings"].to<JsonArray>();
    for (int i = 0; i < 7; i++) {
        JsonObject sObj = servoArr.add<JsonObject>();
        sObj["min_pulsewidth"] = configManager.getServoMinPulsewidth(i);
        sObj["max_pulsewidth"] = configManager.getServoMaxPulsewidth(i);
    }

    // Control bindings (JSON)
    String binds = configManager.getControlBindingsJson();
    if (binds.length() == 0) binds = String(ConfigManager::getDefaultControlBindingsJson());
    JsonDocument bd;
    if (deserializeJson(bd, binds) == DeserializationError::Ok) {
        doc["control_bindings"] = bd;
    }

    doc["bt_whitelist_enabled"] = configManager.getBtWhitelistEnabled();
    JsonArray wlArr = doc["bt_whitelist"].to<JsonArray>();
    for (const auto& mac : configManager.getBtWhitelist()) wlArr.add(mac);
}

static void emitSerialReady() {
    JsonDocument doc;
    doc["event"] = "ready";
    fillSerialInfo(doc);
    sendSerialJson(doc);
}

static void handleSerialCommandLine(const char* rawLine) {
    if (!rawLine) return;
    const char* line = rawLine;
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
    if (!*line) return;
    if (strncmp(line, "TTCMD:", 6) == 0) {
        line += 6;
        while (*line == ' ' || *line == '\t') line++;
    }
    if (!*line) return;

    JsonDocument cmd;
    DeserializationError err = deserializeJson(cmd, line);
    if (err) {
        JsonDocument resp;
        resp["event"] = "error";
        resp["message"] = "invalid_json";
        sendSerialJson(resp);
        return;
    }

    const char* command = cmd["cmd"] | "";
    JsonDocument resp;
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

        if (!cfg["wifi_mode"].isNull()) {
            const char* mode = cfg["wifi_mode"] | "AP";
            configManager.setWifiMode(String(mode));
            touched = true;
            rebootRequired = true;
        }
        if (!cfg["wifi_ssid"].isNull()) {
            const char* rawSsid = cfg["wifi_ssid"] | "";
            String ssid = String(rawSsid);
            ssid.trim();
            configManager.setWifiSSID(ssid);
            touched = true;
            rebootRequired = true;
        }
        if (!cfg["wifi_password"].isNull()) {
            const char* rawPass = cfg["wifi_password"] | "";
            String pass = String(rawPass);
            pass.trim();
            configManager.setWifiPassword(pass);
            touched = true;
            rebootRequired = true;
        }
        if (!cfg["hotspot_ssid"].isNull()) {
            const char* rawHotspot = cfg["hotspot_ssid"] | "";
            String ssid = sanitizeDeviceName(String(rawHotspot));
            if (ssid.length()) {
                configManager.setHotspotSSID(ssid);
                touched = true;
                rebootRequired = true;
            }
        }
        if (!cfg["hotspot_password"].isNull()) {
            const char* rawPass = cfg["hotspot_password"] | "";
            configManager.setHotspotPassword(String(rawPass));
            touched = true;
            rebootRequired = true;
        }
        if (!cfg["ota_enabled"].isNull()) {
            configManager.setOTAEnabled(cfg["ota_enabled"].as<bool>());
            touched = true;
        }
        if (!cfg["led_count"].isNull()) {
            configManager.setLedCount(cfg["led_count"].as<int>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["led_brightness"].isNull()) {
            configManager.setLedBrightness(cfg["led_brightness"].as<int>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["led_gamma"].isNull()) {
            configManager.setLedGamma(cfg["led_gamma"].as<bool>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["ws_invert_x"].isNull()) {
            configManager.setWsInvertX(cfg["ws_invert_x"].as<bool>());
            touched = true;
        }
        if (!cfg["ws_invert_y"].isNull()) {
            configManager.setWsInvertY(cfg["ws_invert_y"].as<bool>());
            touched = true;
        }
        if (!cfg["ws_swap_sides"].isNull()) {
            configManager.setWsSwapSides(cfg["ws_swap_sides"].as<bool>());
            touched = true;
        }
        if (!cfg["bt_invert_x"].isNull()) {
            configManager.setBtInvertX(cfg["bt_invert_x"].as<bool>());
            touched = true;
        }
        if (!cfg["bt_invert_y"].isNull()) {
            configManager.setBtInvertY(cfg["bt_invert_y"].as<bool>());
            touched = true;
        }
        if (!cfg["bt_swap_axes"].isNull()) {
            configManager.setBtSwapAxes(cfg["bt_swap_axes"].as<bool>());
            touched = true;
        }
        if (!cfg["motor_invert"].isNull()) {
            JsonArray invArr = cfg["motor_invert"].as<JsonArray>();
            for (int i = 0; i < 4 && i < (int)invArr.size(); i++) {
                configManager.setMotorInvert(i, invArr[i].as<bool>());
            }
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_swap"].isNull()) {
            configManager.setMotorSwap(cfg["motor_swap"].as<bool>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_deadband"].isNull()) {
            JsonArray dbArr = cfg["motor_deadband"].as<JsonArray>();
            for (int i = 0; i < 4 && i < (int)dbArr.size(); i++) {
                configManager.setMotorDeadband(i, dbArr[i].as<int>());
            }
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_trim"].isNull()) {
            JsonArray trimArr = cfg["motor_trim"].as<JsonArray>();
            for (int i = 0; i < 4 && i < (int)trimArr.size(); i++) {
                configManager.setMotorTrim(i, trimArr[i].as<int>());
            }
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_frequency"].isNull()) {
            JsonArray freqArr = cfg["motor_frequency"].as<JsonArray>();
            for (int i = 0; i < 4 && i < (int)freqArr.size(); i++) {
                configManager.setMotorFrequency(i, freqArr[i].as<int>());
            }
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_left_gui"].isNull()) {
            configManager.setMotorLeftGUI(constrain(cfg["motor_left_gui"].as<int>(), 0, 3));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_right_gui"].isNull()) {
            configManager.setMotorRightGUI(constrain(cfg["motor_right_gui"].as<int>(), 0, 3));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_gripper_gui"].isNull()) {
            configManager.setMotorGripperGUI(constrain(cfg["motor_gripper_gui"].as<int>(), 0, 3));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["gui_mode"].isNull()) {
            configManager.setGuiMode(String((const char*)(cfg["gui_mode"] | "basic")));
            touched = true;
        }
        if (!cfg["legacy_board"].isNull()) {
            configManager.setLegacyBoard(cfg["legacy_board"].as<bool>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["drive_mixer"].isNull()) {
            configManager.setDriveMixer(String((const char*)(cfg["drive_mixer"] | "arcade")));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["drive_turn_gain"].isNull()) {
            configManager.setDriveTurnGain(cfg["drive_turn_gain"].as<float>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["drive_axis_deadband"].isNull()) {
            configManager.setDriveAxisDeadband(cfg["drive_axis_deadband"].as<int>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_curve_type"].isNull()) {
            configManager.setMotorCurveType(String((const char*)(cfg["motor_curve_type"] | "linear")));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_curve_strength"].isNull()) {
            configManager.setMotorCurveStrength(cfg["motor_curve_strength"].as<float>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_decay"].isNull()) {
            configManager.setMotorDecay(String((const char*)(cfg["motor_decay"] | "slow")));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_kick_ms"].isNull()) {
            configManager.setMotorKickMs(cfg["motor_kick_ms"].as<int>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_kick_pwm"].isNull()) {
            configManager.setMotorKickPwm(cfg["motor_kick_pwm"].as<int>());
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_stop_mode"].isNull()) {
            configManager.setMotorStopMode(String((const char*)(cfg["motor_stop_mode"] | "brake")));
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["motor_stall_amps"].isNull()) {
            configManager.setMotorStallAmps(cfg["motor_stall_amps"].as<float>());
            touched = true;
        }
        if (!cfg["motor_stall_ms"].isNull()) {
            configManager.setMotorStallMs(cfg["motor_stall_ms"].as<int>());
            touched = true;
        }
        if (!cfg["servo_settings"].isNull()) {
            JsonArray servoArr = cfg["servo_settings"].as<JsonArray>();
            for (int i = 0; i < 7 && i < (int)servoArr.size(); i++) {
                int min_pw = servoArr[i]["min_pulsewidth"] | 500;
                int max_pw = servoArr[i]["max_pulsewidth"] | 2500;
                configManager.setServoPulsewidthRange(i, min_pw, max_pw);
            }
            touched = true;
            reapplyHardware = true;
        }
        if (!cfg["control_bindings"].isNull()) {
            String tmp;
            serializeJson(cfg["control_bindings"], tmp);
            configManager.setControlBindingsJson(tmp);
            touched = true;
        }
        if (!cfg["bt_whitelist_enabled"].isNull()) {
            configManager.setBtWhitelistEnabled(cfg["bt_whitelist_enabled"].as<bool>());
            touched = true;
        }
        if (!cfg["bt_whitelist"].isNull()) {
            std::vector<String> addrs;
            JsonArray wlArr = cfg["bt_whitelist"].as<JsonArray>();
            for (JsonVariant v : wlArr) {
                String mac = v.as<String>();
                if (mac.length() == 17) addrs.push_back(mac);
            }
            configManager.setBtWhitelist(addrs);
            touched = true;
        }
        if (!cfg["bt_scan_on_normal_ms"].isNull()) {
            configManager.setBtScanOnNormal(cfg["bt_scan_on_normal_ms"].as<int>());
            touched = true;
        }
        if (!cfg["bt_scan_off_normal_ms"].isNull()) {
            configManager.setBtScanOffNormal(cfg["bt_scan_off_normal_ms"].as<int>());
            touched = true;
        }
        if (!cfg["bt_scan_on_sta_ms"].isNull()) {
            configManager.setBtScanOnSta(cfg["bt_scan_on_sta_ms"].as<int>());
            touched = true;
        }
        if (!cfg["bt_scan_off_sta_ms"].isNull()) {
            configManager.setBtScanOffSta(cfg["bt_scan_off_sta_ms"].as<int>());
            touched = true;
        }
        if (!cfg["bt_scan_on_ap_ms"].isNull()) {
            configManager.setBtScanOnAp(cfg["bt_scan_on_ap_ms"].as<int>());
            touched = true;
        }
        if (!cfg["bt_scan_off_ap_ms"].isNull()) {
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
            if (serialCmdOverflow) {
                serialCmdOverflow = false;
                serialCmdLen = 0;
                JsonDocument resp;
                resp["event"] = "error";
                resp["message"] = "command_too_long";
                sendSerialJson(resp);
                continue;
            }
            serialCmdBuffer[serialCmdLen] = '\0';
            handleSerialCommandLine(serialCmdBuffer);
            serialCmdLen = 0;
            continue;
        }
        if (serialCmdOverflow) {
            continue;
        }
        if (serialCmdLen < (sizeof(serialCmdBuffer) - 1)) {
            serialCmdBuffer[serialCmdLen++] = c;
        } else {
            serialCmdOverflow = true;
            serialCmdLen = 0;
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
            String modelName = ctl->getModelName();
            Console.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", modelName.c_str(), properties.vendor_id,
                           properties.product_id);
            // BTstack speichert die Adresse MSB zuerst (Index 0..5), genau so wie die
            // Whitelist sie vergleicht. Früher wurde hier rückwärts formatiert.
            String mac = BtPairing::formatMac(properties.btaddr);
            Console.printf("Controller MAC: %s\n", mac.c_str());
            myControllers[i] = ctl;
            foundEmptySlot = true;
            board.notifyControllerConnected(i, mac.c_str(), modelName.c_str());
            // Ziel erreicht: Such-Fenster beenden, damit das WLAN wieder die volle Funkzeit hat
            BtPairing::stopSearch();
            statusLedShowsSearch = false;
            board.setLED(0, 0, 255, 0); // Green
            board.showLEDs();
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
            board.notifyControllerDisconnected(i);
            board.clearControllerSnapshot(i);
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
    if (idx >= 0) {
        board.updateControllerSnapshot(idx, ctl);
        inputBindings.process(ctl, idx);
    }

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
    // On v2 boards: external pull-up with active-low MODE button.
    // On legacy v1 boards: GPIO39 is the H-Bridge motor fault signal (LOW = OK, HIGH = Fault).
    pinMode(MODE_BUTTON_PIN, INPUT);
    if (!configManager.isLegacyBoard()) {
        if (handleStartupReset()) {
            return;
        }
    }

    board.startServices();

    // Suche hier nicht starten (startScanning = false): Der Stack ist noch nicht bereit, ein
    // sofortiges Start/Stopp scheitert dann mit 0x0c. Die Suche startet das Such-Fenster in loop().
    BP32.setup(&onConnectedController, &onDisconnectedController, false);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
    sm_set_secure_connections_only_mode(false);                        // SC ausschalten
    prepareBluetoothStack();
    BtPairing::begin();
    applyWhitelistFromConfig();

    board.setWhitelistApplyCallback(applyWhitelistFromConfig);

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // forgetBluetoothKeys() wird nur beim Factory-Reset (handleStartupReset) aufgerufen,
    // nicht bei jedem normalen Boot - sonst schlägt die Re-Auth aller gepairten Controller fehl.
    // BP32.forgetBluetoothKeys(); // Removed: Causes pairing issues with clones

    // Load input bindings from config
    inputBindings.reload();

    setStatusLed(60, 60, 60); // dim white
    // Nach dem Start einmal nach neuen Controllern suchen
    startControllerSearch(searchWindowMs);
    emitSerialReady();
}

// Status-LED (LED 0)
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

    // Wait up to 2.5s for button release. If GPIO39 stays permanently LOW
    // (e.g. unconfigured legacy v1 board where GPIO39 is Motor Fault = LOW when OK),
    // continue normal boot instead of entering an infinite ESP.restart() loop.
    uint32_t releaseWaitStart = millis();
    while ((millis() - releaseWaitStart) < 2500) {
        if (!isModePressedStable()) {
            delay(100);
            ESP.restart();
            return true;
        }
        delay(50);
    }

    Serial.println("GPIO39 stayed LOW after reset window - continuing boot (possible legacy v1 board).");
    board.reApplyConfig();
    return false;
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

// Suche (Inquiry) an- oder abschalten, nur bei Zustandswechsel. Die Blöcke und Pausen
// (siehe prepareBluetoothStack) plant der Bluetooth-Controller selbst, die Firmware
// schaltet nicht mehr im Sekundentakt um.
static void setInquiry(bool on) {
    if (on == inquiryRunning) return;
    inquiryRunning = on;
    BP32.enableNewBluetoothConnections(on);
}

// Abgleich mit dem tatsächlichen Zustand im BTstack, einmal pro Sekunde.
// Start und Stopp können mit 0x0c (Command Disallowed) scheitern, wenn der Stack gerade
// einen vorherigen Befehl abarbeitet (siehe Log "cannot stop inquiry (0x0c)"). Bluepad32
// merkt sich trotzdem den neuen Zustand und versucht es nicht erneut. Ohne Abgleich
// könnte die Suche nach dem Fenster dauerhaft weiterlaufen (WLAN wird ausgebremst)
// oder im Fenster nie starten. Beide Aufrufe sind ohne Wirkung, wenn der Zustand passt.
static volatile bool inquiryReconcilePending = false;
static btstack_context_callback_registration_t inquiryReconcileReg;

static void inquiryReconcileOnBtThread(void* context) {
    (void)context;
    inquiryReconcilePending = false;
    if (inquiryRunning) {
        // Gleiche Parameter wie Bluepad32 (in prepareBluetoothStack gesetzt).
        // 0x0c heißt hier: läuft bereits oder Stack im Übergang, nächste Sekunde erneut prüfen.
        if (gap_inquiry_periodic_start(kInquiryLength, kInquiryMaxPeriod, kInquiryMinPeriod) == ERROR_CODE_SUCCESS) {
            Console.println("BT-Suche per Abgleich gestartet");
        }
    } else {
        // 0x0c heißt hier: Suche ist bereits aus (oder Stack im Übergang, dann erneut)
        if (gap_inquiry_stop() == ERROR_CODE_SUCCESS) {
            Console.println("BT-Suche per Abgleich gestoppt");
        }
    }
}

static void reconcileInquiry(uint32_t now) {
    static uint32_t lastReconcileMs = 0;
    if (inquiryReconcilePending || (now - lastReconcileMs) < 1000) return;
    lastReconcileMs = now;
    inquiryReconcilePending = true;
    inquiryReconcileReg.callback = &inquiryReconcileOnBtThread;
    inquiryReconcileReg.context = nullptr;
    btstack_run_loop_execute_on_main_thread(&inquiryReconcileReg);
}

void loop() {
    pollSerialCommands();
    // This call fetches all the controllers' data.
    // Call this function in your main loop.
    bool dataUpdated = BP32.update();
    if (dataUpdated)
        processControllers();
    inputBindings.tick();
    board.updateMotors();

    uint32_t now = millis();

    if (configManager.isLegacyBoard()) {
        // Legacy board v1: GPIO39 is Motor Driver Fault (not MODE button)
        board.checkFaults();
    } else {
        // MODE-Taste ca. 0,7 s halten: Controller-Suche starten (oder laufende Suche beenden)
        bool modeNow = (digitalRead(MODE_BUTTON_PIN) == LOW);
        if (!modeNow) {
            modeButtonSeenReleased = true;
        }
        if (modeNow && modeButtonSeenReleased) {
            if (modeButtonPressStartMs == 0) {
                modeButtonPressStartMs = now;
                modeButtonLongPressHandled = false;
            } else if (!modeButtonLongPressHandled && (now - modeButtonPressStartMs) >= modeButtonHoldToSwitchMs) {
                if (BtPairing::isSearching()) {
                    BtPairing::stopSearch();
                    Console.println("Controller-Suche per MODE-Taste beendet");
                } else {
                    startControllerSearch(searchWindowMs);
                }
                modeButtonLongPressHandled = true;
            }
        } else {
            modeButtonPressStartMs = 0;
            modeButtonLongPressHandled = false;
        }
    }

    bool anyController = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] && myControllers[i]->isConnected()) { anyController = true; break; }
    }

    // Ist das WLAN bis zum Neustart abgeschaltet, gehört die Funkzeit ganz dem Bluetooth:
    // dann ohne Controller dauerhaft suchen.
    bool searching = BtPairing::isSearching();
    bool wantInquiry = searching || (board.isWifiDisabledUntilRestart() && !anyController);
    // BTstack braucht nach einem Disconnect ~500 ms, sonst kommt error=0x0c (Command Disallowed)
    if (wantInquiry && (int32_t)(now - scanRestartAfterMs) < 0) {
        wantInquiry = false;
    }
    setInquiry(wantInquiry);
    reconcileInquiry(now);

    // Status-LED: blau blinkend während der Suche, danach grün (Controller) oder schwach weiß
    static int lastBlinkState = -1;
    if (searching) {
        int on = ((now / 500) % 2) == 0 ? 1 : 0;
        if (on != lastBlinkState) {
            lastBlinkState = on;
            if (on) setStatusLed(0, 0, 255);
            else setStatusLed(0, 0, 0);
        }
        statusLedShowsSearch = true;
    } else if (statusLedShowsSearch) {
        lastBlinkState = -1;
        statusLedShowsSearch = false;
        if (anyController) setStatusLed(0, 255, 0);
        else setStatusLed(60, 60, 60);
    }

    //board.updateWebClients();

    // Heartbeat alle 5s - zeigt dass der ESP läuft
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

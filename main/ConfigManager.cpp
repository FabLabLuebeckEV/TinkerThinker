#include "ConfigManager.h"

ConfigManager::ConfigManager() {
    setDefaults();
}

bool ConfigManager::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS init failed!");
        return false;
    }
    // Laden oder bei Fehlen defaults speichern
    if (!fileExists("/config.json")) {
        saveConfig();
    } else {
        loadConfig();
    }
    return true;
}

void ConfigManager::setDefaults() {
    wifi_mode = "AP";
    wifi_ssid = "fablab";
    wifi_password = "fablabfdm";
    hotspot_ssid = "TinkerThinkerAP";
    hotspot_password = "";
    for (int i = 0; i < 4; i++) motor_invert[i] = false;
    motor_swap = false;
    motorLeftGUI = 2;
    motorRightGUI = 3;
    for (int i = 0; i < 4; i++) motor_deadband[i] = 50;
    led_count = 30;
    led_brightness = 50;
    led_gamma = false;
    ota_enabled = false;
    for (int i=0; i<4; i++) motor_frequency[i] = 5000;
    for (int i=0; i<3; i++) {
        servos[i].min_pw = 500;
        servos[i].max_pw = 2500;
    }
    drive_mixer = "arcade";
    drive_turn_gain = 1.0f;
    drive_axis_deadband = 16;
    motor_curve_type = "linear";
    motor_curve_strength = 0.0f;
    // BT scan defaults already initialized in header
    // Control bindings default: mirrors current hardcoded behavior
    control_bindings_json = String(getDefaultControlBindingsJson());
    bt_whitelist_enabled = false;
    bt_whitelist.clear();
}

bool ConfigManager::fileExists(const char* path) {
    return LittleFS.exists(path);
}

bool ConfigManager::loadConfig() {
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        Serial.println("Failed to open config file for reading");
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Serial.println("Failed to parse config.json");
        return false;
    }

    wifi_mode = doc["wifi_mode"] | "AP";
    wifi_ssid = doc["wifi_ssid"] | "MyAP";
    wifi_password = doc["wifi_password"] | "Password";
    hotspot_ssid = doc["hotspot_ssid"] | "TinkerThinkerAP";
    hotspot_password = doc["hotspot_password"] | "";

    JsonArray motorInvertArr = doc["motor_invert"].as<JsonArray>();
    for (int i=0; i<4; i++) {
        motor_invert[i] = motorInvertArr[i] | false;
    }

    motor_swap = doc["motor_swap"] | false;
    // Motor GUI
    motorLeftGUI = doc["motor_left_gui"] | 2;
    motorRightGUI = doc["motor_right_gui"] | 3;
    led_count = doc["led_count"] | 30;
    led_brightness = doc["led_brightness"] | 50;
    led_gamma = doc["led_gamma"] | false;
    ota_enabled = doc["ota_enabled"] | false;
    JsonArray motorDeadbandArr = doc["motor_deadband"].as<JsonArray>();
    for (int i=0; i<4; i++) {
        motor_deadband[i] = motorDeadbandArr[i] | 50;
    }

    JsonArray motorFreqArr = doc["motor_frequency"].as<JsonArray>();
    for (int i=0; i<4; i++) {
        motor_frequency[i] = motorFreqArr[i] | 5000;
    }

    JsonArray servoArr = doc["servo_settings"].as<JsonArray>();
    for (int i=0; i<3; i++) {
        servos[i].min_pw = servoArr[i]["min_pulsewidth"] | 500;
        servos[i].max_pw = servoArr[i]["max_pulsewidth"] | 2500;
    }

    // Optional: BT scan settings
    bt_scan_on_normal_ms  = doc["bt_scan_on_normal_ms"]  | bt_scan_on_normal_ms;
    bt_scan_off_normal_ms = doc["bt_scan_off_normal_ms"] | bt_scan_off_normal_ms;
    bt_scan_on_sta_ms     = doc["bt_scan_on_sta_ms"]     | bt_scan_on_sta_ms;
    bt_scan_off_sta_ms    = doc["bt_scan_off_sta_ms"]    | bt_scan_off_sta_ms;
    bt_scan_on_ap_ms      = doc["bt_scan_on_ap_ms"]      | bt_scan_on_ap_ms;
    bt_scan_off_ap_ms     = doc["bt_scan_off_ap_ms"]     | bt_scan_off_ap_ms;

    if (!doc["drive_mixer"].isNull()) {
        setDriveMixer(String((const char*)doc["drive_mixer"]));
    }
    if (!doc["drive_turn_gain"].isNull()) {
        setDriveTurnGain(doc["drive_turn_gain"].as<float>());
    }
    if (!doc["drive_axis_deadband"].isNull()) {
        setDriveAxisDeadband(doc["drive_axis_deadband"].as<int>());
    }
    if (!doc["motor_curve_type"].isNull()) {
        setMotorCurveType(String((const char*)doc["motor_curve_type"]));
    }
    if (!doc["motor_curve_strength"].isNull()) {
        setMotorCurveStrength(doc["motor_curve_strength"].as<float>());
    }

    // Control bindings (store raw JSON)
    if (!doc["control_bindings"].isNull()) {
        String tmp;
        serializeJson(doc["control_bindings"], tmp);
        control_bindings_json = tmp;
    }

    // Bluetooth Whitelist
    bt_whitelist_enabled = doc["bt_whitelist_enabled"] | false;
    bt_whitelist.clear();
    if (!doc["bt_whitelist"].isNull()) {
        JsonArray wlArr = doc["bt_whitelist"].as<JsonArray>();
        for (JsonVariant v : wlArr) {
            String mac = v.as<String>();
            if (mac.length() == 17) bt_whitelist.push_back(mac);
        }
    }

    return true;
}

bool ConfigManager::saveConfig() {
    JsonDocument doc;
    doc["wifi_mode"] = wifi_mode;
    doc["wifi_ssid"] = wifi_ssid;
    doc["wifi_password"] = wifi_password;
    doc["hotspot_ssid"] = hotspot_ssid;
    doc["hotspot_password"] = hotspot_password;

    JsonArray invArr = doc["motor_invert"].to<JsonArray>();
    for (int i=0; i<4; i++) invArr.add(motor_invert[i]);

    doc["motor_swap"] = motor_swap;

    // Motor GUI
    doc["motor_left_gui"] = motorLeftGUI;
    doc["motor_right_gui"] = motorRightGUI;

    JsonArray dbArr = doc["motor_deadband"].to<JsonArray>();
    for (int i=0; i<4; i++) dbArr.add(motor_deadband[i]);

    JsonArray freqArr = doc["motor_frequency"].to<JsonArray>();
    for (int i=0; i<4; i++) freqArr.add(motor_frequency[i]);

    doc["led_count"] = led_count;
    doc["led_brightness"] = led_brightness;
    doc["led_gamma"] = led_gamma;
    doc["ota_enabled"] = ota_enabled;

    JsonArray servoArr = doc["servo_settings"].to<JsonArray>();
    for (int i=0; i<3; i++){
        JsonObject sObj = servoArr.add<JsonObject>();
        sObj["min_pulsewidth"] = servos[i].min_pw;
        sObj["max_pulsewidth"] = servos[i].max_pw;
    }

    // Drive profile & motor curve
    doc["drive_mixer"] = drive_mixer;
    doc["drive_turn_gain"] = drive_turn_gain;
    doc["drive_axis_deadband"] = drive_axis_deadband;
    doc["motor_curve_type"] = motor_curve_type;
    doc["motor_curve_strength"] = motor_curve_strength;

    // BT scan settings
    doc["bt_scan_on_normal_ms"]  = bt_scan_on_normal_ms;
    doc["bt_scan_off_normal_ms"] = bt_scan_off_normal_ms;
    doc["bt_scan_on_sta_ms"]     = bt_scan_on_sta_ms;
    doc["bt_scan_off_sta_ms"]    = bt_scan_off_sta_ms;
    doc["bt_scan_on_ap_ms"]      = bt_scan_on_ap_ms;
    doc["bt_scan_off_ap_ms"]     = bt_scan_off_ap_ms;

    // Control bindings: embed stored JSON
    if (control_bindings_json.length() > 0) {
        JsonDocument binds;
        DeserializationError err = deserializeJson(binds, control_bindings_json);
        if (!err) {
            doc["control_bindings"] = binds;
        }
    }

    // Bluetooth Whitelist
    doc["bt_whitelist_enabled"] = bt_whitelist_enabled;
    JsonArray wlArr = doc["bt_whitelist"].to<JsonArray>();
    for (const auto& mac : bt_whitelist) wlArr.add(mac);

    if (doc.overflowed()) {
        Serial.println("saveConfig: JSON-Dokument zu klein (overflow) – NICHT gespeichert");
        return false;
    }

    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();
    Serial.printf("saveConfig: %u Bytes nach /config.json geschrieben\n", (unsigned)written);
    return true;
}

bool ConfigManager::resetConfig() {
    setDefaults();
    return saveConfig();
}

const char* ConfigManager::getDefaultControlBindingsJson() {
    return R"JSON([
  {"input": {"type": "axis_pair", "x": "RX", "y": "RY", "deadband": 16},
   "action": {"type": "drive_pair", "target": "gui"}},
  {"input": {"type": "axis_pair", "x": "X",  "y": "Y",  "deadband": 16},
   "action": {"type": "drive_pair", "target": "other"}},
  {"input": {"type": "dpad", "dir": "LEFT",  "edge": "hold"},
   "action": {"type": "motor_direct", "motor": 0, "pwm": -255}},
  {"input": {"type": "dpad", "dir": "RIGHT", "edge": "hold"},
   "action": {"type": "motor_direct", "motor": 0, "pwm":  255}},
  {"input": {"type": "dpad", "dir": "UP",    "edge": "hold"},
   "action": {"type": "motor_direct", "motor": 1, "pwm":  255}},
  {"input": {"type": "dpad", "dir": "DOWN",  "edge": "hold"},
   "action": {"type": "motor_direct", "motor": 1, "pwm": -255}},
  {"input": {"type": "button", "code": "BUTTON_R2", "edge": "press"},
   "action": {"type": "servo_toggle_band", "servo": 0, "bands": [0,90]}},
  {"input": {"type": "button", "code": "BUTTON_L2", "edge": "press"},
   "action": {"type": "servo_toggle_band", "servo": 0, "bands": [90,180]}},
  {"input": {"type": "button", "code": "BUTTON_R1", "edge": "press"},
   "action": {"type": "speed_adjust", "delta": 0.1}},
  {"input": {"type": "button", "code": "BUTTON_L1", "edge": "press"},
   "action": {"type": "speed_adjust", "delta": -0.1}}
])JSON";
}

// Getter
String ConfigManager::getWifiMode() { return wifi_mode; }
String ConfigManager::getWifiSSID() { return wifi_ssid; }
String ConfigManager::getWifiPassword() { return wifi_password; }
String ConfigManager::getHotspotSSID() { return hotspot_ssid; }
String ConfigManager::getHotspotPassword() { return hotspot_password; }
bool ConfigManager::getMotorInvert(int index) { return motor_invert[index]; }
bool ConfigManager::getMotorSwap() { return motor_swap; }
int ConfigManager::getMotorLeftGUI() { return motorLeftGUI; }
int ConfigManager::getMotorRightGUI() { return motorRightGUI; }
int ConfigManager::getLedCount() { return led_count; }
int ConfigManager::getLedBrightness() { return led_brightness; }
bool ConfigManager::getLedGamma() { return led_gamma; }
int ConfigManager::getMotorDeadband(int index) {return motor_deadband[index];}
int ConfigManager::getMotorFrequency(int index){return motor_frequency[index];}
bool ConfigManager::getOTAEnabled() { return ota_enabled; }
int ConfigManager::getServoMinPulsewidth(int index) { return servos[index].min_pw; }
int ConfigManager::getServoMaxPulsewidth(int index) { return servos[index].max_pw; }

// BT scan getters
int ConfigManager::getBtScanOnNormal()  { return bt_scan_on_normal_ms; }
int ConfigManager::getBtScanOffNormal() { return bt_scan_off_normal_ms; }
int ConfigManager::getBtScanOnSta()     { return bt_scan_on_sta_ms; }
int ConfigManager::getBtScanOffSta()    { return bt_scan_off_sta_ms; }
int ConfigManager::getBtScanOnAp()      { return bt_scan_on_ap_ms; }
int ConfigManager::getBtScanOffAp()     { return bt_scan_off_ap_ms; }

// Setter
void ConfigManager::setWifiMode(const String &mode) { wifi_mode = mode; }
void ConfigManager::setWifiSSID(const String &ssid) { wifi_ssid = ssid; }
void ConfigManager::setWifiPassword(const String &pass) { wifi_password = pass; }
void ConfigManager::setHotspotSSID(const String &ssid){ hotspot_ssid = ssid; }
void ConfigManager::setHotspotPassword(const String &pass){ hotspot_password = pass; }
void ConfigManager::setMotorInvert(int index, bool inv) { motor_invert[index] = inv; }
void ConfigManager::setMotorSwap(bool swap) { motor_swap = swap; }
void ConfigManager::setMotorLeftGUI(int motorIndex){ motorLeftGUI = motorIndex; }
void ConfigManager::setMotorRightGUI(int motorIndex){ motorRightGUI = motorIndex; }
void ConfigManager::setLedCount(int count){ led_count = count; }
void ConfigManager::setLedBrightness(int value){
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    led_brightness = value;
}
void ConfigManager::setLedGamma(bool enabled){ led_gamma = enabled; }
void ConfigManager::setOTAEnabled(bool enabled){ ota_enabled = enabled; }
void ConfigManager::setServoPulsewidthRange(int index, int min_pw, int max_pw){
    servos[index].min_pw = min_pw;
    servos[index].max_pw = max_pw;
}
void ConfigManager::setMotorDeadband(int index, int val){ motor_deadband[index] = val;}
void ConfigManager::setMotorFrequency(int index, int val){ motor_frequency[index]=val;}
void ConfigManager::setDriveMixer(const String& mixer){
    if (mixer == "tank") {
        drive_mixer = "tank";
    } else {
        drive_mixer = "arcade";
    }
}
void ConfigManager::setDriveTurnGain(float gain){
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 2.5f) gain = 2.5f;
    drive_turn_gain = gain;
}
void ConfigManager::setDriveAxisDeadband(int deadband){
    if (deadband < 0) deadband = 0;
    if (deadband > 256) deadband = 256;
    drive_axis_deadband = deadband;
}
void ConfigManager::setMotorCurveType(const String& type){
    if (type == "expo") {
        motor_curve_type = "expo";
    } else {
        motor_curve_type = "linear";
    }
}
void ConfigManager::setMotorCurveStrength(float strength){
    if (strength < -0.8f) strength = -0.8f;
    if (strength > 3.0f) strength = 3.0f;
    motor_curve_strength = strength;
}

// BT scan setters
void ConfigManager::setBtScanOnNormal(int v)  { bt_scan_on_normal_ms  = v; }
void ConfigManager::setBtScanOffNormal(int v) { bt_scan_off_normal_ms = v; }
void ConfigManager::setBtScanOnSta(int v)     { bt_scan_on_sta_ms     = v; }
void ConfigManager::setBtScanOffSta(int v)    { bt_scan_off_sta_ms    = v; }
void ConfigManager::setBtScanOnAp(int v)      { bt_scan_on_ap_ms      = v; }
void ConfigManager::setBtScanOffAp(int v)     { bt_scan_off_ap_ms     = v; }


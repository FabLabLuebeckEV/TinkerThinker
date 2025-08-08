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
    ota_enabled = false;
    for (int i=0; i<4; i++) motor_frequency[i] = 5000;
    for (int i=0; i<3; i++) {
        servos[i].min_pw = 500;
        servos[i].max_pw = 2500;
    }
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
    StaticJsonDocument<2048> doc;
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

    return true;
}

bool ConfigManager::saveConfig() {
    StaticJsonDocument<2048> doc;
    doc["wifi_mode"] = wifi_mode;
    doc["wifi_ssid"] = wifi_ssid;
    doc["wifi_password"] = wifi_password;
    doc["hotspot_ssid"] = hotspot_ssid;
    doc["hotspot_password"] = hotspot_password;

    JsonArray invArr = doc.createNestedArray("motor_invert");
    for (int i=0; i<4; i++) invArr.add(motor_invert[i]);

    doc["motor_swap"] = motor_swap;

    // Motor GUI
    doc["motor_left_gui"] = motorLeftGUI;
    doc["motor_right_gui"] = motorRightGUI;

    JsonArray dbArr = doc.createNestedArray("motor_deadband");
    for (int i=0; i<4; i++) dbArr.add(motor_deadband[i]);

    JsonArray freqArr = doc.createNestedArray("motor_frequency");
    for (int i=0; i<4; i++) freqArr.add(motor_frequency[i]);

    doc["led_count"] = led_count;
    doc["ota_enabled"] = ota_enabled;

    JsonArray servoArr = doc.createNestedArray("servo_settings");
    for (int i=0; i<3; i++){
        JsonObject sObj = servoArr.createNestedObject();
        sObj["min_pulsewidth"] = servos[i].min_pw;
        sObj["max_pulsewidth"] = servos[i].max_pw;
    }

    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();
    //loadConfig();
    return true;
}

bool ConfigManager::resetConfig() {
    setDefaults();
    return saveConfig();
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
int ConfigManager::getMotorDeadband(int index) {return motor_deadband[index];}
int ConfigManager::getMotorFrequency(int index){return motor_frequency[index];}
bool ConfigManager::getOTAEnabled() { return ota_enabled; }
int ConfigManager::getServoMinPulsewidth(int index) { return servos[index].min_pw; }
int ConfigManager::getServoMaxPulsewidth(int index) { return servos[index].max_pw; }

// Setter
void ConfigManager::setWifiMode(const String &mode) { wifi_mode = mode; }
void ConfigManager::setWifiSSID(const String &ssid) { wifi_ssid = ssid; }
void ConfigManager::setWifiPassword(const String &pass) { wifi_password = pass; }
void ConfigManager::setHotspotSSID(const String &ssid){ hotspot_ssid = ssid; }
void ConfigManager::setHotspotPassword(const String &pass){ hotspot_password = ""; }//pass; }
void ConfigManager::setMotorInvert(int index, bool inv) { motor_invert[index] = inv; }
void ConfigManager::setMotorSwap(bool swap) { motor_swap = swap; }
void ConfigManager::setMotorLeftGUI(int motorIndex){ motorLeftGUI = motorIndex; }
void ConfigManager::setMotorRightGUI(int motorIndex){ motorRightGUI = motorIndex; }
void ConfigManager::setLedCount(int count){ led_count = count; }
void ConfigManager::setOTAEnabled(bool enabled){ ota_enabled = enabled; }
void ConfigManager::setServoPulsewidthRange(int index, int min_pw, int max_pw){
    servos[index].min_pw = min_pw;
    servos[index].max_pw = max_pw;
}
void ConfigManager::setMotorDeadband(int index, int val){ motor_deadband[index] = val;}
void ConfigManager::setMotorFrequency(int index, int val){ motor_frequency[index]=val;}


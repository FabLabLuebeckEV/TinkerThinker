#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>

class ConfigManager {
public:
    ConfigManager();
    bool init();
    bool loadConfig();
    bool saveConfig();
    bool resetConfig();

    // Getter
    String getWifiMode();
    String getWifiSSID();
    String getWifiPassword();
    String getHotspotSSID();
    String getHotspotPassword();
    bool getMotorInvert(int index);
    bool getMotorSwap();
    int getMotorLeftGUI();
    int getMotorRightGUI();
    int getLedCount();
    bool getOTAEnabled();
    int getMotorDeadband(int index);
    int getMotorFrequency(int index);

    int getServoMinPulsewidth(int index);
    int getServoMaxPulsewidth(int index);

    // BT scan duty-cycle (ms)
    int getBtScanOnNormal();
    int getBtScanOffNormal();
    int getBtScanOnSta();
    int getBtScanOffSta();
    int getBtScanOnAp();
    int getBtScanOffAp();

    // Setter (aufgerufen wenn config-Seite geändert wird)
    void setWifiMode(const String &mode);
    void setWifiSSID(const String &ssid);
    void setWifiPassword(const String &pass);
    void setHotspotSSID(const String &ssid);
    void setHotspotPassword(const String &pass);

    void setMotorInvert(int index, bool inv);
    void setMotorSwap(bool swap);
    void setLedCount(int count);
    void setOTAEnabled(bool enabled);
    void setMotorFrequency(int index, int val);
    void setMotorDeadband(int index, int val);
    void setMotorLeftGUI(int motorIndex);
    void setMotorRightGUI(int motorIndex);

    void setServoPulsewidthRange(int index, int min_pw, int max_pw);

    void setBtScanOnNormal(int v);
    void setBtScanOffNormal(int v);
    void setBtScanOnSta(int v);
    void setBtScanOffSta(int v);
    void setBtScanOnAp(int v);
    void setBtScanOffAp(int v);

    bool fileExists(const char* path);

    const bool* getMotorInvertArray() { return motor_invert; }
    // Control bindings JSON (raw). Stored as JSON array/object string.
    String getControlBindingsJson() const { return control_bindings_json; }
    void setControlBindingsJson(const String &json) { control_bindings_json = json; }
    static const char* getDefaultControlBindingsJson();

private:
    bool motor_invert[4];
    bool motor_swap;
    int motor_deadband[4];
    int motor_frequency[4];
    int motorLeftGUI = 2;
    int motorRightGUI = 3;
    String wifi_mode;
    String wifi_ssid;
    String wifi_password;
    String hotspot_ssid;
    String hotspot_password;
    int led_count;
    bool ota_enabled;

    struct ServoConfig {
        int min_pw;
        int max_pw;
    };
    ServoConfig servos[3];

    // Bluetooth scan duty-cycle settings
    int bt_scan_on_normal_ms = 500;
    int bt_scan_off_normal_ms = 500;
    int bt_scan_on_sta_ms = 150;
    int bt_scan_off_sta_ms = 850;
    int bt_scan_on_ap_ms = 100;
    int bt_scan_off_ap_ms = 1900;

    String control_bindings_json; // raw JSON string for control mappings

    void setDefaults();
};

#endif

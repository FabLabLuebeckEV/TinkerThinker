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
    int getLedCount();
    bool getOTAEnabled();
    int getMotorDeadband(int index);
    int getMotorFrequency(int index);

    int getServoMinPulsewidth(int index);
    int getServoMaxPulsewidth(int index);

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

    void setServoPulsewidthRange(int index, int min_pw, int max_pw);

    bool fileExists(const char* path);

    const bool* getMotorInvertArray() { return motor_invert; }

private:
    bool motor_invert[4];
    bool motor_swap;
    int motor_deadband[4];
    int motor_frequency[4];
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

    void setDefaults();
};

#endif

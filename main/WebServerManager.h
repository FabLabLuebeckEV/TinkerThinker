#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <functional>

class TinkerThinkerBoard; 
class ConfigManager;

struct ConnectedControllerInfo {
    bool connected = false;
    char mac[18] = {};
    char model[32] = {};
};

class WebServerManager {
public:
    WebServerManager(TinkerThinkerBoard* board, ConfigManager* config);
    void init();
    void requestWifiDisable(bool untilRestart);
    void requestWifiEnable();
    void sendStatusUpdate();
    bool isWifiDisabled() const { return wifiDisabledUntilRestart || wifiTemporarilyDisabled; }
    bool isWifiDisabledUntilRestart() const { return wifiDisabledUntilRestart; }

    void notifyControllerConnected(int slot, const char* mac, const char* model);
    void notifyControllerDisconnected(int slot);
    void setWhitelistApplyCallback(std::function<void()> cb) { whitelistApplyCallback = cb; }

private:
    TinkerThinkerBoard* board;
    ConfigManager* config;
    AsyncWebServer server;
    AsyncWebSocket ws;
    SemaphoreHandle_t wsMutex = xSemaphoreCreateMutex();
    //Timeout als millisekunden seit dem letzten paket
    unsigned long lastPacketTime = 0;
    bool wifiDisabledUntilRestart = false;
    bool wifiTemporarilyDisabled = false;
    bool wifiShutdownInProgress = false;
    bool wifiStartupInProgress = false;
    bool _otaError = false;

    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len);
    void setupRoutes();
    void setupWebSocket();
    void startWifi();
    void handleConfig(AsyncWebServerRequest* request);
    void disableWifiUntilRestart();
    void disableWifiInternal(bool permanent);
    void enableWifiInternal();

    ConnectedControllerInfo connectedControllers[4];
    std::function<void()> whitelistApplyCallback;
};

// Helper to register control bindings REST endpoints
void registerBindingsRoutes(AsyncWebServer& server, ConfigManager* config);

#endif

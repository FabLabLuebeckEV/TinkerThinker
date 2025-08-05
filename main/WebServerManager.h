#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>

class TinkerThinkerBoard; 
class ConfigManager;

class WebServerManager {
public:
    WebServerManager(TinkerThinkerBoard* board, ConfigManager* config);
    void init();
    void sendStatusUpdate();

private:
    TinkerThinkerBoard* board;
    ConfigManager* config;
    AsyncWebServer server;
    AsyncWebSocket ws;
    //Timeout als millisekunden seit dem letzten paket
    unsigned long lastPacketTime = 0;

    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                          AwsEventType type, void *arg, uint8_t *data, size_t len);
    void setupRoutes();
    void setupWebSocket();
    void startWifi();
    void handleConfig(AsyncWebServerRequest* request);
};

#endif

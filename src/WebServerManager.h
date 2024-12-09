#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>

class MyCustomBoard; // Forward Declaration

class WebServerManager {
public:
    WebServerManager(MyCustomBoard* board, const char* apSSID = "MyBoard", const char* apPassword = "12345678");
    void init();
    void handleClient(); // Falls benötigt, in diesem Fall meist nicht, da async
    void sendStatusUpdate(); // Aufrufbar z.B. jede Sekunde, um den Clients Status zu senden
    
private:
    MyCustomBoard* board;
    const char* ssid;
    const char* password;

    AsyncWebServer server;
    AsyncWebSocket ws;

    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                          AwsEventType type, void *arg, uint8_t *data, size_t len);
    void setupRoutes();
    void setupWebSocket();
};

#endif

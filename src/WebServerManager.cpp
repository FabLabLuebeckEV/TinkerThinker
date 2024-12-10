#include "WebServerManager.h"
#include "MyCustomBoard.h" // Für den Zugriff auf den Board-Zustand

WebServerManager::WebServerManager(MyCustomBoard* board, const char* apSSID, const char* apPassword)
: board(board), ssid(apSSID), password(apPassword), server(80), ws("/ws") {}

void WebServerManager::init() {
    // Dateisystem mounten
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }

    // WLAN als Access Point starten
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    setupWebSocket();
    setupRoutes();

    // Server starten
    server.begin();
    Serial.println("Web Server started");
}

void WebServerManager::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);
}

void WebServerManager::setupRoutes() {
    // Statische Dateien aus LittleFS bereitstellen
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Beispiel für zusätzliche Routen (POST requests, etc.) können hier hinzugefügt werden
    // Hier nicht zwingend nötig, da Steuerung über WebSocket läuft.
}

void WebServerManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        String message;
        for (size_t i = 0; i < len; i++) {
            message += (char)data[i];
        }

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            if (doc.containsKey("x") && doc.containsKey("y") && doc.containsKey("servo")) {
                // Joystick und Servo-Steuerung
                float x = doc["x"].as<float>();
                float y = doc["y"].as<float>();
                int servoAngle = doc["servo"].as<int>();

                // Joystick-Rotation um 90 Grad
                float rotatedX = -y;
                float rotatedY = x;

                // Motorsteuerung mit den rotierten Werten
                board->controlMotors((int)(rotatedX * 512), (int)(rotatedY * 512));

                // Servo-Winkel setzen
                board->setServoAngle(0, servoAngle); // Annahme: Servo Index 0

            }
            else if (doc.containsKey("motorC")) {
                // Motor C Steuerung
                String command = doc["motorC"].as<String>();
                if (command == "forward") {
                    board->controlMotorForward(2); // Motor C ist Index 2
                }
                else if (command == "backward") {
                    board->controlMotorBackward(2);
                } else {
                    board->controlMotorStop(2);
                }
            }
        }
    }
}

void WebServerManager::handleClient() {
    // Bei AsyncWebServer nicht zwingend nötig
    ws.cleanupClients();
}

void WebServerManager::sendStatusUpdate() {
    if (ws.count() > 0) {
        StaticJsonDocument<512> doc;
        doc["batteryVoltage"] = board->getBatteryVoltage();
        doc["batteryPercentage"] = board->getBatteryPercentage();
        
        // Servo-Winkel
        JsonArray servos = doc.createNestedArray("servos");
        for (int i = 0; i < 3; i++) {
            servos.add(board->getServoAngle(i));
        }
        
        // Motor PWM Geschwindigkeiten
        JsonArray motorPWMs = doc.createNestedArray("motorPWMs");
        for (int i = 0; i < 4; i++) {
            motorPWMs.add(board->getMotorPWM(i));
        }
        
        // Motor Fehlerstatus
        doc["motorFaults"] = board->isMotorInFault();

        // Motorstrom
        JsonArray motorCurrents = doc.createNestedArray("motorCurrents");
        for (int i = 0; i < 2; i++) {
            motorCurrents.add(board->getHBridgeAmps(i));
        }
        
        // Farbe der ersten WS2812
        CRGB ledColor = board->getLEDColor(0);
        JsonObject firstLED = doc.createNestedObject("firstLED");
        firstLED["r"] = ledColor.r;
        firstLED["g"] = ledColor.g;
        firstLED["b"] = ledColor.b;
        
        // JSON Serialisieren
        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
    }
}

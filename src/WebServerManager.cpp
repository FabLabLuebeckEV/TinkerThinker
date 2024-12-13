#include "WebServerManager.h"
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"

WebServerManager::WebServerManager(TinkerThinkerBoard* board, ConfigManager* config)
: board(board), config(config), server(80), ws("/ws") {}

void WebServerManager::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }

    startWifi();
    setupWebSocket();
    setupRoutes();
    server.begin();
    Serial.println("Web Server started");
}

void WebServerManager::startWifi() {
    if (config->getWifiMode() == "AP") {
        WiFi.softAP(config->getHotspotSSID().c_str(), config->getHotspotPassword().c_str());
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);
    } else {
        // Client mode
        WiFi.mode(WIFI_STA);
        WiFi.begin(config->getWifiSSID().c_str(), config->getWifiPassword().c_str());
        Serial.print("Connecting to WiFi ");
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        Serial.println();
        Serial.print("Connected IP: ");
        Serial.println(WiFi.localIP());
    }
}

void WebServerManager::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);
}

void WebServerManager::setupRoutes() {
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/config.html", "text/html");
    });

    server.on("/config.css", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/config.css", "text/css");
    });

    // JSON mit aktueller Config liefern
    server.on("/getConfig", HTTP_GET, [this](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(2048);

        doc["wifi_mode"] = config->getWifiMode();
        doc["wifi_ssid"] = config->getWifiSSID();
        doc["wifi_password"] = config->getWifiPassword();
        doc["hotspot_ssid"] = config->getHotspotSSID();
        doc["hotspot_password"] = config->getHotspotPassword();

        JsonArray invArr = doc.createNestedArray("motor_invert");
        for (int i=0; i<4; i++) invArr.add(config->getMotorInvert(i));

        doc["motor_swap"] = config->getMotorSwap();

        JsonArray dbArr = doc.createNestedArray("motor_deadband");
        for (int i=0; i<4; i++) dbArr.add(config->getMotorDeadband(i));

        JsonArray freqArr = doc.createNestedArray("motor_frequency");
        for (int i=0; i<4; i++) freqArr.add(config->getMotorFrequency(i));

        doc["led_count"] = config->getLedCount();
        doc["ota_enabled"] = config->getOTAEnabled();

        JsonArray servoArr = doc.createNestedArray("servo_settings");
        for (int i=0; i<3; i++) {
            JsonObject sObj = servoArr.createNestedObject();
            sObj["min_pulsewidth"] = config->getServoMinPulsewidth(i);
            sObj["max_pulsewidth"] = config->getServoMaxPulsewidth(i);
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST config
    server.on("/config", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleConfig(request);
    });

    server.on("/resetconfig", HTTP_GET, [this](AsyncWebServerRequest* request){
        config->resetConfig();
        request->redirect("/config");
        ESP.restart();
    });

    server.on("/reboot", HTTP_GET, [this](AsyncWebServerRequest* request){
        request->send(200, "text/plain", "Rebooting...");
        delay(1000);
        ESP.restart();
    });
}


void WebServerManager::handleConfig(AsyncWebServerRequest* request) {
    // WLAN- und Hotspot-Settings
    if (request->hasParam("wifi_mode", true)) {
        config->setWifiMode(request->getParam("wifi_mode", true)->value());
    }
    if (request->hasParam("wifi_ssid", true)) {
        config->setWifiSSID(request->getParam("wifi_ssid", true)->value());
    }
    if (request->hasParam("wifi_password", true)) {
        config->setWifiPassword(request->getParam("wifi_password", true)->value());
    }
    if (request->hasParam("hotspot_ssid", true)) {
        config->setHotspotSSID(request->getParam("hotspot_ssid", true)->value());
    }
    if (request->hasParam("hotspot_password", true)) {
        config->setHotspotPassword(request->getParam("hotspot_password", true)->value());
    }

    // Motor Swap
    if (request->hasParam("motor_swap", true)) {
        bool sw = (request->getParam("motor_swap", true)->value() == "on");
        config->setMotorSwap(sw);
    } else {
        config->setMotorSwap(false);
    }

    // Motoren (invert, deadband, frequency) - wir gehen von Feldern wie motor_invert_0, motor_deadband_0, motor_frequency_0 etc. aus
    for (int i = 0; i < 4; i++) {
        // Motor Invert
        {
            String field = "motor_invert_" + String(i);
            bool inv = false;
            if (request->hasParam(field, true)) {
                inv = (request->getParam(field, true)->value() == "on");
            }
            config->setMotorInvert(i, inv);
        }

        // Motor Deadband
        {
            String field = "motor_deadband_" + String(i);
            int dbVal = config->getMotorDeadband(i); // Falls nicht gesetzt, Default beibehalten
            if (request->hasParam(field, true)) {
                dbVal = request->getParam(field, true)->value().toInt();
            }
            config->setMotorDeadband(i, dbVal);
        }

        // Motor Frequency
        {
            String field = "motor_frequency_" + String(i);
            int freqVal = config->getMotorFrequency(i); // Falls nicht gesetzt, Default beibehalten
            if (request->hasParam(field, true)) {
                freqVal = request->getParam(field, true)->value().toInt();
            }
            config->setMotorFrequency(i, freqVal);
        }
    }

    // LED Count
    if (request->hasParam("led_count", true)) {
        config->setLedCount(request->getParam("led_count", true)->value().toInt());
    }

    // OTA Enabled
    if (request->hasParam("ota_enabled", true)) {
        bool ota = (request->getParam("ota_enabled", true)->value() == "on");
        config->setOTAEnabled(ota);
    } else {
        config->setOTAEnabled(false);
    }

    // Servos
    for (int i = 0; i < 3; i++) {
        String minField = "servo" + String(i) + "_min";
        String maxField = "servo" + String(i) + "_max";
        int min_pw = config->getServoMinPulsewidth(i);
        int max_pw = config->getServoMaxPulsewidth(i);

        if (request->hasParam(minField, true)) {
            min_pw = request->getParam(minField, true)->value().toInt();
        }
        if (request->hasParam(maxField, true)) {
            max_pw = request->getParam(maxField, true)->value().toInt();
        }
        config->setServoPulsewidthRange(i, min_pw, max_pw);
    }

    // Config speichern
    config->saveConfig();

    // Redirect und Neustart
    request->redirect("/config");
    delay(1000);
    ESP.restart();
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
            // Vorhandene Steuerung für Joystick & Servo
            if (doc.containsKey("x") && doc.containsKey("y") && doc.containsKey("servo")) {
                float x = doc["x"].as<float>();
                float y = doc["y"].as<float>();
                int servoAngle = doc["servo"].as<int>();

                // Achsenrotation
                float rotatedX = -y;
                float rotatedY = x;

                board->controlMotors((int)(rotatedX * 512), (int)(rotatedY * 512));
                board->setServoAngle(0, servoAngle);
            } 
            // Bestehende Motor C Steuerung
            else if (doc.containsKey("motorC")) {
                String command = doc["motorC"].as<String>();
                if (command == "forward") {
                    board->controlMotorForward(2);
                } else if (command == "backward") {
                    board->controlMotorBackward(2);
                } else {
                    board->controlMotorStop(2);
                }
            } 
            // Neue Einstellung zum direkten Setzen des PWM-Werts eines Motors
            // Erwartetes Format: {"motor": <index>, "pwm": <0-255>}
            else if (doc.containsKey("motor") && doc.containsKey("pwm")) {
                int motorIndex = doc["motor"].as<int>();
                int pwmValue = doc["pwm"].as<int>();

                // Grenzen prüfen, falls nötig
                pwmValue = constrain(pwmValue, 0, 255);

                // Direktansteuerung des Motors mit dem angegebenen PWM-Wert
                // Du kannst hier eine neue Methode in der Board-Klasse verwenden,
                // z. B. board->controlMotorDirect(motorIndex, pwmValue);
                // oder falls es schon eine Methode gibt, die Motoren direkt steuert:
                board->controlMotorDirect(motorIndex, pwmValue);
            }
            // Optionale Einstellung zum Setzen des Swap-Flags, falls von der UI gesendet
            // z.B. {"swap":true} oder {"swap":false}
            else if (doc.containsKey("swap")) {
                bool sw = doc["swap"].as<bool>();
                // Config entsprechend setzen
                config->setMotorSwap(sw);
                config->saveConfig();
            }
        }
    }
}


void WebServerManager::sendStatusUpdate() {
    if (ws.count() > 0) {
        StaticJsonDocument<512> doc;
        doc["batteryVoltage"] = board->getBatteryVoltage();
        doc["batteryPercentage"] = board->getBatteryPercentage();

        JsonArray servos = doc.createNestedArray("servos");
        for (int i = 0; i < 3; i++) {
            servos.add(board->getServoAngle(i));
        }

        JsonArray motorPWMs = doc.createNestedArray("motorPWMs");
        for (int i = 0; i < 4; i++) {
            motorPWMs.add(board->getMotorPWM(i));
        }

        doc["motorFaults"] = board->isMotorInFault();

        JsonArray motorCurrents = doc.createNestedArray("motorCurrents");
        for (int i = 0; i < 2; i++) {
            motorCurrents.add(board->getHBridgeAmps(i));
        }

        CRGB ledColor = board->getLEDColor(0);
        JsonObject firstLED = doc.createNestedObject("firstLED");
        firstLED["r"] = ledColor.r;
        firstLED["g"] = ledColor.g;
        firstLED["b"] = ledColor.b;

        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
    }
}

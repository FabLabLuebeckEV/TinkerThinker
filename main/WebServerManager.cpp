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
    // Register control bindings REST endpoints
    registerBindingsRoutes(server, config);
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

void WebServerManager::disableWifiUntilRestart() {
    requestWifiDisable(true);
}

void WebServerManager::requestWifiDisable(bool untilRestart) {
    if (wifiDisabledUntilRestart || wifiShutdownInProgress) {
        return;
    }
    if (untilRestart) {
        wifiDisabledUntilRestart = true;
    } else {
        wifiTemporarilyDisabled = true;
    }
    wifiShutdownInProgress = true;

    xTaskCreate([](void* arg) {
        WebServerManager* self = static_cast<WebServerManager*>(arg);
        self->disableWifiInternal();
        self->wifiShutdownInProgress = false;
        vTaskDelete(NULL);
    }, "WifiShutdown", 4096, this, 1, NULL);
}

void WebServerManager::requestWifiEnable() {
    if (wifiDisabledUntilRestart || wifiStartupInProgress) {
        return;
    }
    if (!wifiTemporarilyDisabled && WiFi.getMode() != WIFI_OFF) {
        return;
    }
    wifiTemporarilyDisabled = false;
    wifiStartupInProgress = true;

    xTaskCreate([](void* arg) {
        WebServerManager* self = static_cast<WebServerManager*>(arg);
        self->enableWifiInternal();
        self->wifiStartupInProgress = false;
        vTaskDelete(NULL);
    }, "WifiStartup", 6144, this, 1, NULL);
}

void WebServerManager::disableWifiInternal() {
    Serial.println("Disabling WiFi per user request...");
    delay(150);
    ws.closeAll();
    server.end();

    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_STA || mode == WIFI_AP_STA) {
        WiFi.disconnect(true, true);
    }
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        WiFi.softAPdisconnect(true);
    }
    WiFi.mode(WIFI_OFF);
    delay(50);
    Serial.println("WiFi disabled. Web UI unavailable until restart; Bluetooth scanning forced on.");
}

void WebServerManager::enableWifiInternal() {
    Serial.println("Enabling WiFi per user request...");
    startWifi();
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
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/config.html", "text/html");
    });

    // Controls mapping page (raw JSON editor)
    server.on("/controls", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/controls.html", "text/html");
    });
    server.on("/controls.js", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/controls.js", "application/javascript");
    });

    server.on("/config.css", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/config.css", "text/css");
    });

    server.on("/setup", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup.html", "text/html");
    });

    server.on("/wifi/disable", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (wifiDisabledUntilRestart || WiFi.getMode() == WIFI_OFF) {
            request->send(409, "application/json", "{\"error\":\"already_disabled\"}");
            return;
        }
        AsyncWebServerResponse* res = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
        res->addHeader("Connection", "close");
        request->send(res);
        disableWifiUntilRestart();
    });

    // JSON mit aktueller Config liefern
    server.on("/getConfig", HTTP_GET, [this](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(8192);

        doc["wifi_mode"] = config->getWifiMode();
        doc["wifi_ssid"] = config->getWifiSSID();
        doc["wifi_password"] = config->getWifiPassword();
        doc["hotspot_ssid"] = config->getHotspotSSID();
        doc["hotspot_password"] = config->getHotspotPassword();
        doc["wifi_disabled_until_restart"] = wifiDisabledUntilRestart || (WiFi.getMode() == WIFI_OFF);

        JsonArray invArr = doc.createNestedArray("motor_invert");
        for (int i=0; i<4; i++) invArr.add(config->getMotorInvert(i));

        doc["motor_swap"] = config->getMotorSwap();

        JsonArray dbArr = doc.createNestedArray("motor_deadband");
        for (int i=0; i<4; i++) dbArr.add(config->getMotorDeadband(i));

        JsonArray freqArr = doc.createNestedArray("motor_frequency");
        for (int i=0; i<4; i++) freqArr.add(config->getMotorFrequency(i));

        doc["led_count"] = config->getLedCount();
        doc["ota_enabled"] = config->getOTAEnabled();

        doc["drive_mixer"] = config->getDriveMixer();
        doc["drive_turn_gain"] = config->getDriveTurnGain();
        doc["drive_axis_deadband"] = config->getDriveAxisDeadband();
        doc["motor_curve_type"] = config->getMotorCurveType();
        doc["motor_curve_strength"] = config->getMotorCurveStrength();

        // Bluetooth scan duty-cycle
        doc["bt_scan_on_normal_ms"]  = config->getBtScanOnNormal();
        doc["bt_scan_off_normal_ms"] = config->getBtScanOffNormal();
        doc["bt_scan_on_sta_ms"]     = config->getBtScanOnSta();
        doc["bt_scan_off_sta_ms"]    = config->getBtScanOffSta();
        doc["bt_scan_on_ap_ms"]      = config->getBtScanOnAp();
        doc["bt_scan_off_ap_ms"]     = config->getBtScanOffAp();

        JsonArray servoArr = doc.createNestedArray("servo_settings");
        for (int i=0; i<3; i++) {
            JsonObject sObj = servoArr.createNestedObject();
            sObj["min_pulsewidth"] = config->getServoMinPulsewidth(i);
            sObj["max_pulsewidth"] = config->getServoMaxPulsewidth(i);
        }

        // Control bindings: embed JSON
        {
            String binds = config->getControlBindingsJson();
            if (binds.length() == 0) binds = String(ConfigManager::getDefaultControlBindingsJson());
            DynamicJsonDocument bd(8192);
            if (deserializeJson(bd, binds) == DeserializationError::Ok) {
                doc["control_bindings"] = bd;
            }
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
    bool wifiChanged = false;

    // WLAN- und Hotspot-Settings
    if (request->hasParam("wifi_mode", true)) {
        String newMode = request->getParam("wifi_mode", true)->value();
        if (newMode != config->getWifiMode()) {
            wifiChanged = true;
        }
        config->setWifiMode(newMode);
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

    // Motoren (invert, deadband, frequency)
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
            int dbVal = config->getMotorDeadband(i);
            if (request->hasParam(field, true)) {
                dbVal = request->getParam(field, true)->value().toInt();
            }
            config->setMotorDeadband(i, dbVal);
        }

        // Motor Frequency
        {
            String field = "motor_frequency_" + String(i);
            int freqVal = config->getMotorFrequency(i);
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

    // BT scan timings (optional fields)
    auto setIntIf = [&](const char* name, void(*setter)(ConfigManager*, int)){
        if (request->hasParam(name, true)) {
            int v = request->getParam(name, true)->value().toInt();
            setter(config, v);
        }
    };
    setIntIf("bt_scan_on_normal_ms",  [](ConfigManager* c,int v){ c->setBtScanOnNormal(v); });
    setIntIf("bt_scan_off_normal_ms", [](ConfigManager* c,int v){ c->setBtScanOffNormal(v); });
    setIntIf("bt_scan_on_sta_ms",     [](ConfigManager* c,int v){ c->setBtScanOnSta(v); });
    setIntIf("bt_scan_off_sta_ms",    [](ConfigManager* c,int v){ c->setBtScanOffSta(v); });
    setIntIf("bt_scan_on_ap_ms",      [](ConfigManager* c,int v){ c->setBtScanOnAp(v); });
    setIntIf("bt_scan_off_ap_ms",     [](ConfigManager* c,int v){ c->setBtScanOffAp(v); });

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

    if (request->hasParam("drive_mixer", true)) {
        config->setDriveMixer(request->getParam("drive_mixer", true)->value());
    }
    if (request->hasParam("drive_turn_gain", true)) {
        String v = request->getParam("drive_turn_gain", true)->value();
        v.trim();
        if (v.length() > 0) {
            config->setDriveTurnGain(v.toFloat());
        }
    }
    if (request->hasParam("drive_axis_deadband", true)) {
        String v = request->getParam("drive_axis_deadband", true)->value();
        v.trim();
        if (v.length() > 0) {
            config->setDriveAxisDeadband(v.toInt());
        }
    }
    if (request->hasParam("motor_curve_type", true)) {
        config->setMotorCurveType(request->getParam("motor_curve_type", true)->value());
    }
    if (request->hasParam("motor_curve_strength", true)) {
        String v = request->getParam("motor_curve_strength", true)->value();
        v.trim();
        if (v.length() > 0) {
            config->setMotorCurveStrength(v.toFloat());
        }
    }

    // Config speichern
    config->saveConfig();
    // Apply new config
    board->reApplyConfig();

    if (wifiChanged) {
        // Sende eine Nachricht über WebSocket, dass ein Neustart erfolgt
        StaticJsonDocument<200> doc;
        doc["restart"] = true;
        String json;
        serializeJson(doc, json);
        ws.textAll(json);
    }

    // Redirect
    request->redirect("/config");
}

// New endpoints for control bindings JSON
// GET current bindings
// POST new bindings as raw JSON body (Content-Type: application/json)
// Minimal validation and persistence
void registerBindingsRoutes(AsyncWebServer& server, ConfigManager* config) {
    server.on("/getBindings", HTTP_GET, [config](AsyncWebServerRequest* request){
        String binds = config->getControlBindingsJson();
        if (binds.length() == 0) binds = String(ConfigManager::getDefaultControlBindingsJson());
        request->send(200, "application/json", binds);
    });
    server.on("/control_bindings", HTTP_POST,
        [config](AsyncWebServerRequest* request){
            // completed in body handler
            request->send(200, "text/plain", "OK");
        },
        NULL,
        [config](AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total){
            String body;
            body.reserve(total);
            body.concat((const char*)data, len);
            // naive: assume single chunk
            DynamicJsonDocument bd(8192);
            if (deserializeJson(bd, body) == DeserializationError::Ok) {
                // reserialize normalized
                String normalized;
                serializeJson(bd, normalized);
                config->setControlBindingsJson(normalized);
                config->saveConfig();
            }
        }
    );
}



void WebServerManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        lastPacketTime = millis();
        String message;
        for (size_t i = 0; i < len; i++) {
            message += (char)data[i];
        }

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            // Vorhandene Steuerung für Joystick & Servo
            if (doc.containsKey("x") && doc.containsKey("y")) {
                float x = doc["x"].as<float>();
                float y = doc["y"].as<float>();

                // Achsenrotation
                float rotatedX = -y;
                float rotatedY = x;

                board->requestDriveFromWS((int)(rotatedX * 512), (int)(rotatedY * 512));
            }

            // Bestehende Motor Steuerung (A, B, C, D)
            for (int i = 0; i < 4; i++) {
                String motorKey = "motor" + String((char)('A' + i));
                if (doc.containsKey(motorKey)) {
                    String command = doc[motorKey].as<String>();
                    if (command == "forward") {
                        board->requestMotorDirectFromWS(i, 255);
                    } else if (command == "backward") {
                        board->requestMotorDirectFromWS(i, -255);
                    } else if (command == "stop") {
                        board->requestMotorStopFromWS(i);
                    } else {// Direkte PWM-Steuerung
                        int pwmValue = command.toInt();
                        pwmValue = constrain(pwmValue, -255, 255);
                        board->requestMotorDirectFromWS(i, pwmValue);
                    }
                }
            }

            // Raw Motor PWM (bypasses deadband, used by setup)
            if (doc.containsKey("motor_raw")) {
                JsonObject raw = doc["motor_raw"].as<JsonObject>();
                int idx = raw["motor"] | -1;
                int pwm = raw["pwm"] | 0;
                pwm = constrain(pwm, -255, 255);
                if (idx >= 0 && idx < 4) {
                    board->controlMotorRaw(idx, pwm);
                }
            }

            // Servo setzen
            // Erwartetes Format: {"servo0": <angle>, "servo1": <angle>, "servo2": <angle>}
            for (int i = 0; i < 3; i++) {
                String servoKey = "servo" + String(i);
                if (doc.containsKey(servoKey)) {
                    Serial.println(servoKey + ": " + String(doc[servoKey].as<int>()));
                    int angle = doc[servoKey].as<int>();
                    board->setServoAngle(i, angle);
                }
            }

            // LED set (expects {"led_set":{"start":0,"count":1,"color":"#RRGGBB"}})
            if (doc.containsKey("led_set")) {
                JsonObject led = doc["led_set"].as<JsonObject>();
                int start = led["start"] | 0;
                int count = led["count"] | 1;
                const char* color = led["color"] | "#000000";
                long rgb = strtol(color + 1, nullptr, 16);
                uint8_t r = (rgb >> 16) & 0xFF;
                uint8_t g = (rgb >> 8) & 0xFF;
                uint8_t b = (rgb) & 0xFF;
                for (int i = start; i < start + count; i++) {
                    board->setLED(i, r, g, b);
                }
                board->showLEDs();
            }
            // Optionale Einstellung zum Setzen des Swap-Flags, falls von der UI gesendet
            // z.B. {"swap":true} oder {"swap":false}
            if (doc.containsKey("swap")) {
                bool sw = doc["swap"].as<bool>();
                // Config entsprechend setzen
                config->setMotorSwap(sw);
                config->saveConfig();
            }
        }
    } else if (type == WS_EVT_CONNECT) {
        Serial.println("Websocket client connected");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.println("Websocket client disconnected");
        // Do not force stop if BT is controlling; respect arbiter
        for (int i = 0; i < 4; i++) {
            board->requestMotorStopFromWS(i);
        }
    }
}


void WebServerManager::sendStatusUpdate() {
    if (millis() - lastPacketTime > 5000) {
        // Kein Paket seit 1 Sekunde, stoppe alle Motoren
        for (int i = 0; i < 4; i++) {
            //TODO: Check if controller is connected or Wifi
            //board->controlMotorStop(i);
        }
    }
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

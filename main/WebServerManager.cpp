#include "WebServerManager.h"
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"
#include <utility>

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
        WiFi.mode(WIFI_AP);
        WiFi.softAP(config->getHotspotSSID().c_str(), config->getHotspotPassword().c_str());
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);
    } else {
        // Client mode
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        String staSsid = config->getWifiSSID();
        String staPass = config->getWifiPassword();
        staSsid.trim();
        staPass.trim();
        Serial.printf("STA target SSID='%s' (passLen=%u)\n", staSsid.c_str(), (unsigned)staPass.length());
        WiFi.begin(staSsid.c_str(), staPass.c_str());
        Serial.print("Connecting to WiFi ");
        uint32_t staStart = millis();
        const uint32_t STA_TIMEOUT_MS = 10000;
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - staStart > STA_TIMEOUT_MS) {
                Serial.println("\nSTA connect timeout – falling back to AP mode");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP);
                WiFi.softAP(config->getHotspotSSID().c_str(), config->getHotspotPassword().c_str());
                Serial.print("Fallback AP IP: ");
                Serial.println(WiFi.softAPIP());
                return;
            }
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

    bool permanent = untilRestart;
    auto* ctx = new std::pair<WebServerManager*, bool>(this, permanent);
    xTaskCreatePinnedToCore([](void* arg) {
        auto* p = static_cast<std::pair<WebServerManager*, bool>*>(arg);
        WebServerManager* self = p->first;
        bool perm = p->second;
        delete p;
        self->disableWifiInternal(perm);
        self->wifiShutdownInProgress = false;
        vTaskDelete(NULL);
    }, "WifiShutdown", 4096, ctx, 1, NULL, 1);
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

    xTaskCreatePinnedToCore([](void* arg) {
        WebServerManager* self = static_cast<WebServerManager*>(arg);
        self->enableWifiInternal();
        self->wifiStartupInProgress = false;
        vTaskDelete(NULL);
    }, "WifiStartup", 6144, this, 1, NULL, 1);
}

void WebServerManager::disableWifiInternal(bool permanent) {
    Serial.println("Disabling WiFi...");
    delay(150);

    // Close WebSocket connections but do NOT call server.end() —
    // ESPAsyncWebServer does not support stop/restart; calling begin() a second
    // time after end() leaves the TCP listener in an undefined state and causes
    // the main loop to hang until the next BT disconnect.
    if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        ws.closeAll();
        xSemaphoreGive(wsMutex);
    }

    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_STA || mode == WIFI_AP_STA) {
        // eraseap=false — keep credentials so STA can reconnect later
        WiFi.disconnect(true, false);
    }
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        WiFi.softAPdisconnect(true);
    }

    if (permanent) {
        // Full de-init — no re-enable expected
        WiFi.mode(WIFI_OFF);
    }
    // Temporary: skip WIFI_OFF to keep the netif/stack alive for re-enable
    // (a full WIFI_OFF here breaks the later re-init with
    //  "netstack cb reg failed 12308 / ESP_ERR_WIFI_STOP_STATE")

    delay(50);
    Serial.println(permanent ? "WiFi disabled permanently." : "WiFi paused temporarily.");
}

void WebServerManager::enableWifiInternal() {
    Serial.println("Enabling WiFi per user request...");
    startWifi();
    // Do NOT call server.begin() here — the server was never stopped,
    // only WiFi was disconnected. Calling begin() again would create a
    // second TCP listener and cause hangs.
    Serial.println("WiFi re-enabled, server still running.");
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

    // ── OTA Update ────────────────────────────────────────────────────────────
    server.on("/ota", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!config->getOTAEnabled()) {
            request->send(403, "text/plain", "OTA ist in der Konfiguration deaktiviert.");
            return;
        }
        String html = R"HTML(<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><title>Firmware Update</title>
<style>
  body{font-family:'Segoe UI',sans-serif;background:#0f1b2d;color:#e0e0e0;display:flex;
       justify-content:center;align-items:center;min-height:100vh;margin:0;}
  .card{background:#16213e;border-radius:12px;padding:40px;width:380px;text-align:center;
        box-shadow:0 4px 24px #0005;}
  h2{color:#5292c4;margin-bottom:24px;}
  input[type=file]{background:#0f2a4a;border:1px dashed #5292c4;border-radius:8px;
                   padding:20px;width:100%;cursor:pointer;color:#bdcee8;margin-bottom:20px;}
  button{background:#0169b3;color:#fff;border:none;border-radius:8px;
         padding:12px 36px;font-size:1rem;cursor:pointer;width:100%;}
  button:disabled{background:#2a4a6a;cursor:not-allowed;}
  #bar{width:100%;margin-top:16px;height:14px;border-radius:7px;display:none;
       accent-color:#5292c4;}
  #msg{margin-top:16px;font-size:0.9rem;min-height:1.2em;}
  .ok{color:#2ecc71;} .err{color:#e74c3c;}
</style></head><body><div class="card">
<h2>Firmware Update</h2>
<form id="f">
  <input type="file" id="fw" accept=".bin" required>
  <button id="btn" type="submit">Hochladen & Flashen</button>
</form>
<progress id="bar" value="0" max="100"></progress>
<div id="msg"></div>
</div>
<script>
document.getElementById('f').onsubmit=function(e){
  e.preventDefault();
  var file=document.getElementById('fw').files[0];
  if(!file)return;
  var btn=document.getElementById('btn'),bar=document.getElementById('bar'),msg=document.getElementById('msg');
  btn.disabled=true; bar.style.display='block'; msg.textContent='Upload läuft…';
  var fd=new FormData(); fd.append('firmware',file);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/update');
  xhr.upload.onprogress=function(e){if(e.lengthComputable)bar.value=Math.round(e.loaded/e.total*100);};
  xhr.onload=function(){
    if(xhr.status===200){
      msg.innerHTML='<span class="ok">✅ Flash erfolgreich! ESP startet neu…</span>';
      setTimeout(function(){window.location='/';},8000);
    } else {
      msg.innerHTML='<span class="err">❌ Fehler: '+xhr.responseText+'</span>';
      btn.disabled=false;
    }
  };
  xhr.onerror=function(){msg.innerHTML='<span class="err">❌ Verbindung unterbrochen.</span>';btn.disabled=false;};
  xhr.send(fd);
};
</script></body></html>)HTML";
        request->send(200, "text/html", html);
    });

    server.on("/update", HTTP_POST,
        [this](AsyncWebServerRequest *request){
            _otaError = Update.hasError();
            AsyncWebServerResponse* res = request->beginResponse(
                _otaError ? 500 : 200,
                "text/plain",
                _otaError ? Update.errorString() : "OK"
            );
            res->addHeader("Connection", "close");
            request->send(res);
            if (!_otaError) {
                request->onDisconnect([](){
                    Serial.println("OTA: Neustart…");
                    esp_restart();
                });
            }
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            if (!config->getOTAEnabled()) {
                request->send(403, "text/plain", "OTA deaktiviert");
                return;
            }
            if (index == 0) {
                Serial.printf("OTA Start: %s (%u bytes)\n", filename.c_str(), request->contentLength());
                _otaError = false;
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Serial.print("OTA begin error: "); Update.printError(Serial);
                    _otaError = true;
                }
            }
            if (!_otaError && len) {
                if (Update.write(data, len) != len) {
                    Serial.print("OTA write error: "); Update.printError(Serial);
                    _otaError = true;
                }
                Serial.printf("OTA: %u / %u bytes\n", index + len, request->contentLength());
            }
            if (final) {
                if (!_otaError && !Update.end(true)) {
                    Serial.print("OTA end error: "); Update.printError(Serial);
                    _otaError = true;
                }
                Serial.printf("OTA %s: %u bytes total\n", _otaError ? "FEHLER" : "OK", index + len);
            }
        }
    );

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
        JsonDocument doc;

        doc["wifi_mode"] = config->getWifiMode();
        doc["wifi_ssid"] = config->getWifiSSID();
        doc["wifi_password"] = config->getWifiPassword();
        doc["hotspot_ssid"] = config->getHotspotSSID();
        doc["hotspot_password"] = config->getHotspotPassword();
        doc["wifi_disabled_until_restart"] = wifiDisabledUntilRestart || (WiFi.getMode() == WIFI_OFF);

        JsonArray invArr = doc["motor_invert"].to<JsonArray>();
        for (int i=0; i<4; i++) invArr.add(config->getMotorInvert(i));

        doc["motor_swap"] = config->getMotorSwap();
        doc["motor_left_gui"] = config->getMotorLeftGUI();
        doc["motor_right_gui"] = config->getMotorRightGUI();

        JsonArray dbArr = doc["motor_deadband"].to<JsonArray>();
        for (int i=0; i<4; i++) dbArr.add(config->getMotorDeadband(i));

        JsonArray freqArr = doc["motor_frequency"].to<JsonArray>();
        for (int i=0; i<4; i++) freqArr.add(config->getMotorFrequency(i));

        doc["led_count"] = config->getLedCount();
        doc["led_brightness"] = config->getLedBrightness();
        doc["led_gamma"] = config->getLedGamma();
        doc["ws_invert_x"] = config->getWsInvertX();
        doc["ws_invert_y"] = config->getWsInvertY();
        doc["ws_swap_sides"] = config->getWsSwapSides();
        doc["bt_invert_x"] = config->getBtInvertX();
        doc["bt_invert_y"] = config->getBtInvertY();
        doc["bt_swap_axes"] = config->getBtSwapAxes();
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

        JsonArray servoArr = doc["servo_settings"].to<JsonArray>();
        for (int i=0; i<3; i++) {
            JsonObject sObj = servoArr.add<JsonObject>();
            sObj["min_pulsewidth"] = config->getServoMinPulsewidth(i);
            sObj["max_pulsewidth"] = config->getServoMaxPulsewidth(i);
        }

        // Control bindings: embed JSON
        {
            String binds = config->getControlBindingsJson();
            if (binds.length() == 0) binds = String(ConfigManager::getDefaultControlBindingsJson());
            JsonDocument bd;
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

    // GET connected controllers (MAC + model)
    server.on("/bt/controllers", HTTP_GET, [this](AsyncWebServerRequest *request){
        JsonDocument doc;
        JsonArray arr = doc["controllers"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
            if (connectedControllers[i].connected) {
                JsonObject obj = arr.add<JsonObject>();
                obj["slot"] = i;
                obj["mac"] = connectedControllers[i].mac;
                obj["model"] = connectedControllers[i].model;
            }
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // GET whitelist config
    server.on("/bt/whitelist", HTTP_GET, [this](AsyncWebServerRequest *request){
        JsonDocument doc;
        doc["enabled"] = config->getBtWhitelistEnabled();
        JsonArray arr = doc["addresses"].to<JsonArray>();
        for (const auto& mac : config->getBtWhitelist()) arr.add(mac);
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST whitelist config (body: {"enabled": bool, "addresses": [...]})
    server.on("/bt/whitelist", HTTP_POST,
        [this](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "OK");
        },
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            String body;
            body.reserve(total);
            body.concat((const char*)data, len);
            JsonDocument doc;
            if (deserializeJson(doc, body) != DeserializationError::Ok) return;
            if (!doc["enabled"].isNull()) {
                config->setBtWhitelistEnabled(doc["enabled"].as<bool>());
            }
            if (!doc["addresses"].isNull()) {
                std::vector<String> addrs;
                JsonArray arr = doc["addresses"].as<JsonArray>();
                for (JsonVariant v : arr) {
                    String mac = v.as<String>();
                    if (mac.length() == 17) addrs.push_back(mac);
                }
                config->setBtWhitelist(addrs);
            }
            config->saveConfig();
            if (whitelistApplyCallback) whitelistApplyCallback();
        }
    );

    server.on("/reboot", HTTP_GET, [this](AsyncWebServerRequest* request){
        request->send(200, "text/plain", "Rebooting...");
        delay(1000);
        ESP.restart();
    });

    // LED-Einstellungen (Helligkeit/Gamma) dauerhaft speichern
    server.on("/saveLedSettings", HTTP_GET, [this](AsyncWebServerRequest* request){
        if (request->hasParam("brightness")) {
            config->setLedBrightness(request->getParam("brightness")->value().toInt());
        }
        if (request->hasParam("gamma")) {
            config->setLedGamma(request->getParam("gamma")->value() == "1");
        }
        bool ok = config->saveConfig();
        board->setLedBrightness((uint8_t)config->getLedBrightness());
        board->setLedGamma(config->getLedGamma());
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
    });

    // Website-Joystick-Steuerung (unabhängig vom BT-Controller) dauerhaft speichern
    server.on("/setWsDrive", HTTP_GET, [this](AsyncWebServerRequest* request){
        if (request->hasParam("x"))    config->setWsInvertX(request->getParam("x")->value() == "1");
        if (request->hasParam("y"))    config->setWsInvertY(request->getParam("y")->value() == "1");
        if (request->hasParam("swap")) config->setWsSwapSides(request->getParam("swap")->value() == "1");
        bool ok = config->saveConfig();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
    });

    // BT-Controller-Joystick-Richtung (unabhängig von der Website) dauerhaft speichern
    server.on("/setBtDrive", HTTP_GET, [this](AsyncWebServerRequest* request){
        if (request->hasParam("x"))    config->setBtInvertX(request->getParam("x")->value() == "1");
        if (request->hasParam("y"))    config->setBtInvertY(request->getParam("y")->value() == "1");
        if (request->hasParam("swap")) config->setBtSwapAxes(request->getParam("swap")->value() == "1");
        bool ok = config->saveConfig();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
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
        String ssid = request->getParam("wifi_ssid", true)->value();
        ssid.trim();
        config->setWifiSSID(ssid);
    }
    if (request->hasParam("wifi_password", true)) {
        String pass = request->getParam("wifi_password", true)->value();
        pass.trim();
        config->setWifiPassword(pass);
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

    if (request->hasParam("motor_left_gui", true)) {
        int left = request->getParam("motor_left_gui", true)->value().toInt();
        left = constrain(left, 0, 3);
        config->setMotorLeftGUI(left);
    }
    if (request->hasParam("motor_right_gui", true)) {
        int right = request->getParam("motor_right_gui", true)->value().toInt();
        right = constrain(right, 0, 3);
        config->setMotorRightGUI(right);
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
        JsonDocument doc;
        doc["restart"] = true;
        String json;
        serializeJson(doc, json);
        if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            ws.textAll(json);
            xSemaphoreGive(wsMutex);
        }
    }

    // Redirect
    request->redirect("/config");
}

void WebServerManager::notifyControllerConnected(int slot, const char* mac, const char* model) {
    if (slot < 0 || slot >= 4) return;
    connectedControllers[slot].connected = true;
    strncpy(connectedControllers[slot].mac, mac ? mac : "", 17);
    connectedControllers[slot].mac[17] = '\0';
    strncpy(connectedControllers[slot].model, model ? model : "", 31);
    connectedControllers[slot].model[31] = '\0';
}

void WebServerManager::notifyControllerDisconnected(int slot) {
    if (slot < 0 || slot >= 4) return;
    connectedControllers[slot].connected = false;
    connectedControllers[slot].mac[0] = '\0';
    connectedControllers[slot].model[0] = '\0';
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
        // onRequest läuft NACH dem Body-Handler: hier parsen, speichern und antworten.
        [config](AsyncWebServerRequest* request){
            char* body = reinterpret_cast<char*>(request->_tempObject);
            if (!body) {
                request->send(400, "text/plain", "no body");
                return;
            }
            Serial.printf("control_bindings: %u Bytes empfangen\n", (unsigned)strlen(body));
            JsonDocument bd;
            // const char* erzwingt Kopie der Strings ins Dokument (kein Zero-Copy),
            // damit der Puffer direkt danach freigegeben werden darf.
            DeserializationError err = deserializeJson(bd, (const char*)body);
            // Puffer freigeben (free, da mit malloc angelegt – passt zum Framework-Destruktor).
            free(body);
            request->_tempObject = nullptr;
            if (err) {
                Serial.printf("control_bindings: Parse-Fehler: %s\n", err.c_str());
                request->send(400, "text/plain", "invalid json");
                return;
            }
            String normalized;
            serializeJson(bd, normalized);
            config->setControlBindingsJson(normalized);
            bool ok = config->saveConfig();
            Serial.printf("control_bindings: gespeichert=%s\n", ok ? "true" : "false");
            request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
        },
        NULL,
        // Body-Handler: nur in einen malloc-Puffer akkumulieren (free-kompatibel mit
        // ~AsyncWebServerRequest, falls die Verbindung vor onRequest abbricht).
        [](AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total){
            if (index == 0) {
                char* buf = (char*)malloc(total + 1);
                if (buf) buf[total] = '\0';
                request->_tempObject = buf;
            }
            char* buf = reinterpret_cast<char*>(request->_tempObject);
            if (!buf) return;
            memcpy(buf + index, data, len);
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

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            // Vorhandene Steuerung für Joystick & Servo
            if (!doc["x"].isNull() && !doc["y"].isNull()) {
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
                if (!doc[motorKey].isNull()) {
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
            if (!doc["motor_raw"].isNull()) {
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
                if (!doc[servoKey].isNull()) {
                    Serial.println(servoKey + ": " + String(doc[servoKey].as<int>()));
                    int angle = doc[servoKey].as<int>();
                    board->setServoAngle(i, angle);
                }
            }

            // LED set (expects {"led_set":{"start":0,"count":1,"color":"#RRGGBB"}})
            if (!doc["led_set"].isNull()) {
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
            // LED-Helligkeit live setzen (Vorschau, ohne Speichern): {"led_brightness":0-255}
            if (!doc["led_brightness"].isNull()) {
                int b = doc["led_brightness"].as<int>();
                if (b < 0) b = 0; if (b > 255) b = 255;
                board->setLedBrightness((uint8_t)b);
            }
            // Gamma live umschalten: {"led_gamma":true|false}
            if (!doc["led_gamma"].isNull()) {
                board->setLedGamma(doc["led_gamma"].as<bool>());
            }
            // Optionale Einstellung zum Setzen des Swap-Flags, falls von der UI gesendet
            // z.B. {"swap":true} oder {"swap":false}
            if (!doc["swap"].isNull()) {
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
    // Nicht auf den WebSocket/Netz-Stack zugreifen, während WiFi pausiert/abgeschaltet
    // wird – sonst Spinlock-Crash beim gleichzeitigen Teardown (v. a. bei 10 Hz Telemetrie).
    if (isWifiDisabled() || wifiShutdownInProgress) return;

    if (millis() - lastPacketTime > 5000) {
        // Kein Paket seit 1 Sekunde, stoppe alle Motoren
        for (int i = 0; i < 4; i++) {
            //TODO: Check if controller is connected or Wifi
            //board->controlMotorStop(i);
        }
    }
    if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (ws.count() > 0) {
            JsonDocument doc;
            doc["batteryVoltage"] = board->getBatteryVoltage();
            doc["batteryPercentage"] = board->getBatteryPercentage();

            JsonArray servos = doc["servos"].to<JsonArray>();
            for (int i = 0; i < 3; i++) {
                servos.add(board->getServoAngle(i));
            }

            JsonArray motorPWMs = doc["motorPWMs"].to<JsonArray>();
            for (int i = 0; i < 4; i++) {
                motorPWMs.add(board->getMotorPWM(i));
            }

            JsonArray motorCurrents = doc["motorCurrents"].to<JsonArray>();
            for (int i = 0; i < 2; i++) {
                motorCurrents.add(board->getHBridgeAmps(i));
            }

            CRGB ledColor = board->getLEDColor(0);
            JsonObject firstLED = doc["firstLED"].to<JsonObject>();
            firstLED["r"] = ledColor.r;
            firstLED["g"] = ledColor.g;
            firstLED["b"] = ledColor.b;

            JsonArray ctrls = doc["controllers"].to<JsonArray>();
            for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
                const ControllerInputSnapshot& s = board->getControllerSnapshot(i);
                if (!s.connected) continue;
                JsonObject c = ctrls.add<JsonObject>();
                c["idx"]     = i;
                c["buttons"] = s.buttons;
                c["dpad"]    = s.dpad;
                c["x"]       = s.axisX;
                c["y"]       = s.axisY;
                c["rx"]      = s.axisRX;
                c["ry"]      = s.axisRY;
            }
            String jsonString;
            serializeJson(doc, jsonString);
            ws.textAll(jsonString);
        }
        xSemaphoreGive(wsMutex);
    }
}

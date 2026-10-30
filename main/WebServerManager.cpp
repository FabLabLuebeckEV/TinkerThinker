#include "WebServerManager.h"
#include "TinkerThinkerBoard.h"
#include "ConfigManager.h"
#include <esp_wifi.h>
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

#include <lwip/def.h>

// POST-Bodies kommen bei groesseren Daten in mehreren Stuecken (index/total). Sie werden in
// einem malloc-Puffer gesammelt (free-kompatibel mit ~AsyncWebServerRequest, falls die
// Verbindung vor onRequest abbricht) und erst im onRequest-Handler ausgewertet.
static void accumulateBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        char* buf = (char*)malloc(total + 1);
        if (buf) buf[total] = '\0';
        request->_tempObject = buf;
    }
    char* buf = reinterpret_cast<char*>(request->_tempObject);
    if (!buf || index + len > total) return;
    memcpy(buf + index, data, len);
}

// Antwort senden und erst danach neu starten: sobald der Browser die Verbindung schliesst,
// spaetestens aber nach 3 s (falls der Client die Verbindung offen haelt).
// Kein delay()/ESP.restart() direkt im Handler, sonst kommt die Antwort nie beim Browser an.
static void sendAndRestart(AsyncWebServerRequest* request, AsyncWebServerResponse* response) {
    response->addHeader("Connection", "close");
    request->onDisconnect([](){
        Serial.println("Neustart nach Antwort an den Browser...");
        esp_restart();
    });
    request->send(response);
    xTaskCreate([](void*){
        vTaskDelay(pdMS_TO_TICKS(3000));
        Serial.println("Neustart (Fallback nach 3 s)...");
        esp_restart();
    }, "restartFallback", 2048, nullptr, 1, nullptr);
}

// Uebernimmt den gesammelten Body; der Aufrufer muss ihn mit free() freigeben.
static char* takeBody(AsyncWebServerRequest* request) {
    char* body = reinterpret_cast<char*>(request->_tempObject);
    request->_tempObject = nullptr;
    return body;
}

static String sanitizeMdnsHost(const String& raw) {
    String s = raw;
    s.trim();
    s.toLowerCase();
    s.replace("ä", "ae");
    s.replace("ö", "oe");
    s.replace("ü", "ue");
    s.replace("ß", "ss");
    String out;
    out.reserve(s.length());
    bool lastHyphen = false;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out += c;
            lastHyphen = false;
        } else if ((c == '-' || c == '_' || c == ' ') && out.length() > 0 && !lastHyphen) {
            out += '-';
            lastHyphen = true;
        }
    }
    while (out.endsWith("-")) {
        out.remove(out.length() - 1);
    }
    if (out.length() == 0) {
        out = "tinkerthinker";
    }
    return out;
}

void WebServerManager::startWifi() {
    if (config->getWifiMode() == "AP") {
        WiFi.mode(WIFI_AP);
        // Fester Kanal, max. 4 Clients; kein Power-Save gegen Timeouts/Abbrueche
        WiFi.softAP(config->getHotspotSSID().c_str(), config->getHotspotPassword().c_str(), 6, 0, 4);
        esp_wifi_set_ps(WIFI_PS_NONE);
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);
        startMdnsAndLocalDns();
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
                Serial.println("\nSTA connect timeout - falling back to AP mode");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP);
                WiFi.softAP(config->getHotspotSSID().c_str(), config->getHotspotPassword().c_str(), 6, 0, 4);
                esp_wifi_set_ps(WIFI_PS_NONE);
                Serial.print("Fallback AP IP: ");
                Serial.println(WiFi.softAPIP());
                startMdnsAndLocalDns();
                return;
            }
            Serial.print(".");
            delay(500);
        }
        Serial.println();
        Serial.print("Connected IP: ");
        Serial.println(WiFi.localIP());
        startMdnsAndLocalDns();
    }
}

void WebServerManager::startMdnsAndLocalDns() {
    stopMdnsAndLocalDns();

    mdnsHostname = sanitizeMdnsHost(config->getHotspotSSID());
    if (MDNS.begin(mdnsHostname.c_str())) {
        MDNS.setInstanceName(config->getHotspotSSID());
        MDNS.addService("http", "tcp", 80);

        // Falls der Roboter umbenannt wurde, zusätzlich tinkerthinker.local und tinker.local als mDNS-Alias registrieren
        IPAddress activeIp = (WiFi.getMode() & WIFI_AP) ? WiFi.softAPIP() : WiFi.localIP();
        mdns_ip_addr_t delegateAddr = {};
        delegateAddr.addr.type = ESP_IPADDR_TYPE_V4;
        delegateAddr.addr.u_addr.ip4.addr = static_cast<uint32_t>(activeIp);
        delegateAddr.next = nullptr;
        if (mdnsHostname != "tinkerthinker") {
            mdns_delegate_hostname_add("tinkerthinker", &delegateAddr);
        }
        if (mdnsHostname != "tinker") {
            mdns_delegate_hostname_add("tinker", &delegateAddr);
        }
        Serial.printf("mDNS started: http://%s.local (alias: http://tinkerthinker.local)\n", mdnsHostname.c_str());
    } else {
        Serial.println("mDNS start failed");
    }

    // Im Hotspot-Modus (AP) zusätzlich lokalen Unicast-DNS auf Port 53 starten:
    // Beantwortet NUR die lokalen Roboternamen (.local / Kurzname) mit 192.168.4.1
    // und lehnt Internet-Domains sofort sauber mit NXDOMAIN ab (verhindert Captive-Portal-Abbruch nach X Minuten).
    if ((WiFi.getMode() & WIFI_AP) != 0) {
        if (localDnsUdp.listen(53)) {
            localDnsUdp.onPacket([this](AsyncUDPPacket& pkt) {
                this->handleLocalDnsPacket(pkt);
            });
            Serial.println("Local Hotspot DNS (Port 53) active for .local hostnames");
        }
    }
}

void WebServerManager::stopMdnsAndLocalDns() {
    localDnsUdp.close();
    MDNS.end();
}

void WebServerManager::handleLocalDnsPacket(AsyncUDPPacket& pkt) {
    const size_t len = pkt.length();
    const uint8_t* data = pkt.data();
    if (len < 17 || !data) return;

    // DNS Header prüfen (QR == 0 -> Query, QDCount == 1)
    uint16_t flags = (uint16_t(data[2]) << 8) | data[3];
    if ((flags & 0x8000) != 0) return; // Keine Query
    uint16_t qdCount = (uint16_t(data[4]) << 8) | data[5];
    if (qdCount != 1) return;

    // QName parsen
    size_t pos = 12;
    String domain;
    domain.reserve(64);
    while (pos < len && data[pos] != 0) {
        uint8_t labelLen = data[pos++];
        if ((labelLen & 0xC0) != 0 || pos + labelLen > len) return; // Komprimiert oder ungültig
        if (domain.length() > 0) domain += '.';
        for (uint8_t i = 0; i < labelLen; i++) {
            char c = (char)data[pos++];
            if (c >= 'A' && c <= 'Z') c = char(c + 32);
            domain += c;
        }
    }
    if (pos + 5 > len) return; // Nullbyte + QType(2) + QClass(2)
    pos++; // Nullbyte überspringen
    size_t questionEnd = pos + 4;
    uint16_t qType = (uint16_t(data[pos]) << 8) | data[pos + 1];

    if (domain.startsWith("www.")) {
        domain.remove(0, 4);
    }

    bool isLocalMatch =
        (domain == mdnsHostname + ".local") ||
        (domain == mdnsHostname) ||
        (domain == "tinkerthinker.local") ||
        (domain == "tinkerthinker") ||
        (domain == "tinker.local") ||
        (domain == "tinker");

    // Captive-Portal-Erkennungsdomains von Android, iOS, Samsung, Xiaomi, Huawei & Windows
    bool isCaptiveProbe =
        (domain == "connectivitycheck.gstatic.com") ||
        (domain == "connectivitycheck.android.com") ||
        (domain == "clients3.google.com") ||
        (domain == "clients4.google.com") ||
        (domain == "captive.apple.com") ||
        (domain == "apple.com") ||
        (domain == "msftconnecttest.com") ||
        (domain == "msftncsi.com") ||
        (domain == "detectportal.firefox.com") ||
        (domain == "connect.rom.miui.com") ||
        (domain == "connectivitycheck.platform.hicloud.com");

    if (!isLocalMatch && !isCaptiveProbe) {
        // Alle anderen Internet-Domains (WhatsApp, Social Media usw.) sofort mit NXDOMAIN (RCode = 3) ablehnen
        AsyncUDPMessage rpl;
        rpl.write(data[0]);
        rpl.write(data[1]);
        rpl.write(uint8_t(0x81)); // QR=1, RD=1
        rpl.write(uint8_t(0x83)); // RA=1, RCode=3 (NXDOMAIN)
        rpl.write(data[4]);
        rpl.write(data[5]);
        rpl.write(uint8_t(0)); rpl.write(uint8_t(0)); // ANCount = 0
        rpl.write(uint8_t(0)); rpl.write(uint8_t(0)); // NSCount = 0
        rpl.write(uint8_t(0)); rpl.write(uint8_t(0)); // ARCount = 0
        rpl.write(data + 12, questionEnd - 12);
        pkt.send(rpl);
        return;
    }

    // Lokaler Treffer oder Captive-Portal-Probe: Bei A-Record (1) oder ANY (255) mit AP-IP antworten
    IPAddress ip = WiFi.softAPIP();
    AsyncUDPMessage rpl;
    rpl.write(data[0]);
    rpl.write(data[1]);
    rpl.write(uint8_t(0x85)); // QR=1, AA=1, RD=1
    rpl.write(uint8_t(0x80)); // RA=1, RCode=0 (NoError)
    rpl.write(uint8_t(0)); rpl.write(uint8_t(1)); // QDCount = 1
    if (qType == 1 || qType == 255) {
        rpl.write(uint8_t(0)); rpl.write(uint8_t(1)); // ANCount = 1
    } else {
        rpl.write(uint8_t(0)); rpl.write(uint8_t(0)); // ANCount = 0
    }
    rpl.write(uint8_t(0)); rpl.write(uint8_t(0)); // NSCount = 0
    rpl.write(uint8_t(0)); rpl.write(uint8_t(0)); // ARCount = 0
    rpl.write(data + 12, questionEnd - 12);

    if (qType == 1 || qType == 255) {
        // Name pointer auf Offset 12 (0xC00C)
        rpl.write(uint8_t(0xC0)); rpl.write(uint8_t(0x0C));
        // Type A (0x0001), Class IN (0x0001)
        rpl.write(uint8_t(0x00)); rpl.write(uint8_t(0x01));
        rpl.write(uint8_t(0x00)); rpl.write(uint8_t(0x01));
        // TTL = 15s
        rpl.write(uint8_t(0x00)); rpl.write(uint8_t(0x00));
        rpl.write(uint8_t(0x00)); rpl.write(uint8_t(0x0F));
        // RDLENGTH = 4 + IPv4
        rpl.write(uint8_t(0x00)); rpl.write(uint8_t(0x04));
        rpl.write(ip[0]); rpl.write(ip[1]); rpl.write(ip[2]); rpl.write(ip[3]);
    }
    pkt.send(rpl);
}

bool WebServerManager::isPortalUnlocked() const {
    if (portalUnlockedUntilMs != 0 && (int32_t)(portalUnlockedUntilMs - millis()) > 0) return true;
    return portalAutoUnlockAtMs != 0 && (int32_t)(millis() - portalAutoUnlockAtMs) >= 0;
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

    stopMdnsAndLocalDns();

    // Close WebSocket connections but do NOT call server.end() -
    // ESPAsyncWebServer does not support stop/restart; calling begin() a second
    // time after end() leaves the TCP listener in an undefined state and causes
    // the main loop to hang until the next BT disconnect.
    if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        ws.closeAll();
        xSemaphoreGive(wsMutex);
    }

    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_STA || mode == WIFI_AP_STA) {
        // eraseap=false - keep credentials so STA can reconnect later
        WiFi.disconnect(true, false);
    }
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        WiFi.softAPdisconnect(true);
    }

    if (permanent) {
        // Full de-init - no re-enable expected
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
    // Do NOT call server.begin() here - the server was never stopped,
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
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        String mode = config->getGuiMode();
        if (mode == "drive") {
            request->send(LittleFS, "/index.html", "text/html");
        } else if (mode == "arm") {
            request->send(LittleFS, "/arm.html", "text/html");
        } else {
            request->send(LittleFS, "/basic.html", "text/html");
        }
    });

    server.on("/basic", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/basic.html", "text/html");
    });

    server.on("/drive", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/arm", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/arm.html", "text/html");
    });

    // Smart Captive-Portal Launchpad (mit Countdown, Handy-Erkennung & Browser-Absprung)
    server.on("/portal", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/portal.html", "text/html");
    });

    auto unlockHandler = [this](AsyncWebServerRequest *request){
        portalUnlockedUntilMs = millis() + 3600000UL; // 1 Stunde freigeschaltet
        request->send(200, "application/json", "{\"status\":\"unlocked\"}");
    };
    server.on("/portal/unlock", HTTP_POST, unlockHandler);
    server.on("/portal/unlock", HTTP_GET, unlockHandler);

    // Android / Chrome / Samsung / Xiaomi Captive-Portal-Checks
    auto androidProbeHandler = [this](AsyncWebServerRequest *request){
        if (isPortalUnlocked()) {
            request->send(204);
        } else {
            request->redirect("http://192.168.4.1/portal");
        }
    };
    server.on("/generate_204", HTTP_GET, androidProbeHandler);
    server.on("/gen_204", HTTP_GET, androidProbeHandler);

    // Apple iOS / iPadOS / macOS Captive-Portal-Checks
    auto appleProbeHandler = [this](AsyncWebServerRequest *request){
        if (isPortalUnlocked()) {
            request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        } else {
            request->redirect("http://192.168.4.1/portal");
        }
    };
    server.on("/hotspot-detect.html", HTTP_GET, appleProbeHandler);
    server.on("/library/test/success.html", HTTP_GET, appleProbeHandler);

    // Windows / Firefox Captive-Portal-Checks
    server.on("/connecttest.txt", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (isPortalUnlocked()) {
            request->send(200, "text/plain", "Microsoft Connect Test");
        } else {
            request->redirect("http://192.168.4.1/portal");
        }
    });
    server.on("/ncsi.txt", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (isPortalUnlocked()) {
            request->send(200, "text/plain", "Microsoft NCSI");
        } else {
            request->redirect("http://192.168.4.1/portal");
        }
    });
    server.on("/redirect", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->redirect("http://192.168.4.1/portal");
    });
    server.on("/canonical.html", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->redirect("http://192.168.4.1/portal");
    });

    // Statische Dateien aus LittleFS. Bewusst kein serveStatic: Dessen ETag besteht nur aus der
    // Dateigroesse, eine Änderung gleicher Größe wuerde nach einem Update als "304 unveraendert"
    // gelten. send(LittleFS, ...) nutzt fuer .gz den CRC32 aus dem gzip-Trailer als ETag und setzt
    // "Cache-Control: no-cache" - der Browser fragt jedes Mal nach und laedt bei neuer Version neu.
    server.onNotFound([](AsyncWebServerRequest *request){
        String path = request->url();
        if (request->method() != HTTP_GET || path.endsWith("/") || path.indexOf("..") >= 0) {
            request->send(404);
            return;
        }
        if (LittleFS.exists(path + ".gz")) {
            request->send(LittleFS, path);
            return;
        }
        if (LittleFS.exists(path)) {
            // Unkomprimierte Dateien (config.json, Bilder): nie veraltet ausliefern
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, String());
            response->addHeader("Cache-Control", "no-cache, must-revalidate");
            request->send(response);
            return;
        }
        request->send(404);
    });

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

    // OTA Update
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
        doc["mdns_host"] = mdnsHostname;
        doc["wifi_disabled_until_restart"] = wifiDisabledUntilRestart || (WiFi.getMode() == WIFI_OFF);

        JsonArray invArr = doc["motor_invert"].to<JsonArray>();
        for (int i=0; i<4; i++) invArr.add(config->getMotorInvert(i));

        doc["motor_swap"] = config->getMotorSwap();
        doc["motor_left_gui"] = config->getMotorLeftGUI();
        doc["motor_right_gui"] = config->getMotorRightGUI();
        doc["motor_gripper_gui"] = config->getMotorGripperGUI();
        doc["gui_mode"] = config->getGuiMode();
        doc["legacy_board"] = config->isLegacyBoard();
        doc["servo_count"] = config->getActiveServoCount();

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

        int activeServos = config->getActiveServoCount();
        JsonArray servoArr = doc["servo_settings"].to<JsonArray>();
        for (int i=0; i<activeServos; i++) {
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

        // Robot Arm configuration: embed JSON if saved
        {
            String armCfg = config->getArmConfigJson();
            if (armCfg.length() > 0) {
                JsonDocument ad;
                if (deserializeJson(ad, armCfg) == DeserializationError::Ok) {
                    doc["arm_config"] = ad;
                }
            }
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // POST /arm/config (speichert die Roboter-Arm-Zuordnung & Sensoren persistent im config.json)
    server.on("/arm/config", HTTP_POST,
        [this](AsyncWebServerRequest *request){
            char* body = takeBody(request);
            if (!body) {
                request->send(400, "application/json", "{\"error\":\"no body\"}");
                return;
            }
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, (const char*)body);
            free(body);
            if (err) {
                Serial.printf("arm/config: Parse-Fehler: %s\n", err.c_str());
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            String normalized;
            serializeJson(doc, normalized);
            config->setArmConfigJson(normalized);
            bool ok = config->saveConfig();
            request->send(ok ? 200 : 500, "application/json", ok ? "{\"status\":\"ok\"}" : "{\"error\":\"save failed\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            accumulateBody(request, data, len, index, total);
        }
    );

    // POST config
    server.on("/config", HTTP_POST, [this](AsyncWebServerRequest *request){
        handleConfig(request);
    });

    server.on("/resetconfig", HTTP_GET, [this](AsyncWebServerRequest* request){
        config->resetConfig();
        AsyncWebServerResponse* res = request->beginResponse(302);
        res->addHeader("Location", "/config");
        sendAndRestart(request, res);
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
            char* body = takeBody(request);
            if (!body) {
                request->send(400, "text/plain", "no body");
                return;
            }
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, (const char*)body);
            free(body);
            if (err) {
                Serial.printf("bt/whitelist: Parse-Fehler: %s\n", err.c_str());
                request->send(400, "text/plain", "invalid json");
                return;
            }
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
            bool ok = config->saveConfig();
            if (whitelistApplyCallback) whitelistApplyCallback();
            request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            accumulateBody(request, data, len, index, total);
        }
    );

    server.on("/reboot", HTTP_GET, [this](AsyncWebServerRequest* request){
        sendAndRestart(request, request->beginResponse(200, "text/plain", "Rebooting..."));
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

    // BT-Controller-Joystick-Richtung (unabhaengig von der Website) dauerhaft speichern
    server.on("/setBtDrive", HTTP_GET, [this](AsyncWebServerRequest* request){
        if (request->hasParam("x"))    config->setBtInvertX(request->getParam("x")->value() == "1");
        if (request->hasParam("y"))    config->setBtInvertY(request->getParam("y")->value() == "1");
        if (request->hasParam("swap")) config->setBtSwapAxes(request->getParam("swap")->value() == "1");
        bool ok = config->saveConfig();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
    });

    // GUI-Modus direkt umschalten und speichern
    server.on("/setGuiMode", HTTP_GET, [this](AsyncWebServerRequest* request){
        if (request->hasParam("mode")) {
            config->setGuiMode(request->getParam("mode")->value());
        }
        bool ok = config->saveConfig();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
    });

    // Legacy-Board-Modus (v1: Motor-Fault an Pin 39 statt MODE-Button, 3 Servos) direkt umschalten
    server.on("/setLegacyBoard", HTTP_GET, [this](AsyncWebServerRequest* request){
        if (request->hasParam("enabled")) {
            String val = request->getParam("enabled")->value();
            config->setLegacyBoard(val == "1" || val == "true" || val == "on");
        }
        bool ok = config->saveConfig();
        board->reApplyConfig();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
    });
}


void WebServerManager::handleConfig(AsyncWebServerRequest* request) {
    bool wifiChanged = false;

    if (request->hasParam("gui_mode", true)) {
        config->setGuiMode(request->getParam("gui_mode", true)->value());
    }

    if (request->hasParam("arm_config", true)) {
        String armJson = request->getParam("arm_config", true)->value();
        if (armJson.length() > 0) {
            config->setArmConfigJson(armJson);
        }
    }

    if (request->hasParam("legacy_board", true)) {
        String v = request->getParam("legacy_board", true)->value();
        config->setLegacyBoard(v == "on" || v == "1" || v == "true");
    } else {
        config->setLegacyBoard(false);
    }

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
    if (request->hasParam("motor_gripper_gui", true)) {
        int grip = request->getParam("motor_gripper_gui", true)->value().toInt();
        grip = constrain(grip, 0, 3);
        config->setMotorGripperGUI(grip);
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

    // Servos (alle bis zu 7 speichern, falls uebergeben)
    for (int i = 0; i < 7; i++) {
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
        // Sende eine Nachricht ueber WebSocket, dass ein Neustart erfolgt
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
        // onRequest laeuft NACH dem Body-Handler: hier parsen, speichern und antworten.
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
            // Puffer freigeben (free, da mit malloc angelegt - passt zum Framework-Destruktor).
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
            // Vorhandene Steuerung fuer Joystick & Servo
            if (!doc["x"].isNull() && !doc["y"].isNull()) {
                float x = doc["x"].as<float>();
                float y = doc["y"].as<float>();

                // Achsenrotation
                float rotatedX = -y;
                float rotatedY = x;

                lastWsDriveMs = millis();
                wsDriveActive = (x != 0.0f || y != 0.0f);

                board->requestDriveFromWS((int)(rotatedX * 512), (int)(rotatedY * 512));
            }

            // Greifer-Steuerung ueber konfigurierten motor_gripper_gui
            if (!doc["gripper"].isNull()) {
                String cmd = doc["gripper"].as<String>();
                int gripIdx = constrain(config->getMotorGripperGUI(), 0, 3);
                if (cmd == "open" || cmd == "forward") {
                    board->requestMotorDirectFromWS(gripIdx, 255);
                } else if (cmd == "close" || cmd == "backward") {
                    board->requestMotorDirectFromWS(gripIdx, -255);
                } else if (cmd == "stop") {
                    board->requestMotorStopFromWS(gripIdx);
                }
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
            // Erwartetes Format: {"servo0": <angle>, ... "servo6": <angle>}
            for (int i = 0; i < 7; i++) {
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
                // Auf die konfigurierten LEDs begrenzen (sonst lange Schleife bei grossem count)
                int start = constrain((int)(led["start"] | 0), 0, config->getLedCount());
                int count = constrain((int)(led["count"] | 1), 0, config->getLedCount() - start);
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
    // Nicht auf den WebSocket/Netz-Stack zugreifen, waehrend WiFi pausiert/abgeschaltet
    // wird - sonst Spinlock-Crash beim gleichzeitigen Teardown (v. a. bei 10 Hz Telemetrie).
    if (isWifiDisabled() || wifiShutdownInProgress) return;

    // Im AP-Modus prüfen, ob sich ein neues Handy mit dem Hotspot verbunden hat -> Portal-Popup wieder scharfschalten
    if ((WiFi.getMode() & WIFI_AP) != 0) {
        wifi_sta_list_t staList;
        if (esp_wifi_ap_get_sta_list(&staList) == ESP_OK) {
            if (staList.num != lastApStaCount) {
                if (staList.num > lastApStaCount || staList.num == 0) {
                    portalUnlockedUntilMs = 0;
                    // Popup 15 s lang zeigen, danach Checks wieder bestehen lassen
                    portalAutoUnlockAtMs = millis() + 15000UL;
                    if (portalAutoUnlockAtMs == 0) portalAutoUnlockAtMs = 1;
                }
                lastApStaCount = staList.num;
            }
        }
    }

    // Fahr-Watchdog: Joystick ausgelenkt, aber seit 1 s kein Fahrbefehl mehr -> Website-Fahrt
    // neutral setzen. Laeuft ueber den Arbiter, ein aktiver BT-Controller wird nicht gestoppt.
    if (wsDriveActive && (millis() - lastWsDriveMs > 1000)) {
        wsDriveActive = false;
        Serial.println("WS-Fahr-Watchdog: keine Joystick-Daten seit 1 s, stoppe Website-Fahrt");
        board->requestDriveFromWS(0, 0);
    }
    if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (ws.count() > 0) {
            JsonDocument doc;
            doc["batteryVoltage"] = board->getBatteryVoltage();
            doc["batteryPercentage"] = board->getBatteryPercentage();

            // WLAN-Signalstaerke (RSSI in dBm) ermitteln (STA oder AP-Clients)
            int rssi = 0;
            wifi_mode_t mode = WiFi.getMode();
            if (mode == WIFI_STA) {
                rssi = WiFi.RSSI();
            } else if (mode == WIFI_AP || mode == WIFI_AP_STA) {
                wifi_sta_list_t staList;
                if (esp_wifi_ap_get_sta_list(&staList) == ESP_OK && staList.num > 0) {
                    rssi = staList.sta[0].rssi;
                    for (int i = 1; i < staList.num; i++) {
                        if (staList.sta[i].rssi > rssi) {
                            rssi = staList.sta[i].rssi;
                        }
                    }
                }
            }
            doc["wifiRssi"] = rssi;

            int activeServos = config->getActiveServoCount();
            JsonArray servos = doc["servos"].to<JsonArray>();
            for (int i = 0; i < activeServos; i++) {
                servos.add(board->getServoAngle(i));
            }

            if (config->isLegacyBoard()) {
                doc["motorFaults"] = board->isMotorInFault();
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

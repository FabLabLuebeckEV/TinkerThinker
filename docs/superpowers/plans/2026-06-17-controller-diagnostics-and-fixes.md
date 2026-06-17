# Controller-Diagnose, Persistenz-, LED- & Servo/Motor-Verbesserungen — Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bindings zuverlässig speichern, Button-Zuordnung (A/B/X/Y) korrigieren, LED-Farben (Helligkeit/Gamma) reparieren, eine Live-Button-Ansicht im Editor ergänzen und Servo/Motor per Live-Slider sowie neue Binding-Aktionen komfortabler steuern.

**Architecture:** ESP32-Firmware (ESP-IDF + Arduino-Komponente via PlatformIO) mit AsyncWebServer + WebSocket (`/ws`). Konfiguration in `ConfigManager` (LittleFS `config.json`). Eingaben über Bluepad32 → `InputBindingManager`. Web-Frontend statisch in `data/` (LittleFS). Die WS-Backends für Live-Servo/Motor/LED existieren bereits; mehrere Features sind daher überwiegend Frontend-Arbeit.

**Tech Stack:** C++ (ESP-IDF/Arduino), FastLED, Bluepad32, ArduinoJson v6, ESPAsyncWebServer, Vanilla-JS/HTML/CSS.

---

## Hinweise zur Verifikation (Firmware ohne Test-Harness)

Dieses Projekt hat **keine** automatisierte Test-Suite. Verifikation pro Task:

- **Build-Check (automatisch):** `pio run -e esp32dev` muss fehlerfrei kompilieren.
- **Frontend-Dateien:** `pio run -e esp32dev -t buildfs` baut das LittleFS-Image aus `data/`.
- **Manueller Hardware-Test (durch den Nutzer):** Flashen mit `pio run -e esp32dev -t upload`
  (Firmware) bzw. `pio run -e esp32dev -t uploadfs` (Web-Dateien), dann Verhalten prüfen.
  Der Assistent kann **nicht** flashen — manuelle Schritte sind klar markiert.

Commit-Konvention: Conventional Commits, am Ende jeder Commit-Message:
`Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`

---

## Task 1: Persistenz – Overflow in saveConfig/loadConfig erkennen (A2)

**Files:**
- Modify: `main/ConfigManager.cpp` (`saveConfig`, `loadConfig`)

- [ ] **Step 1: `loadConfig` auf DynamicJsonDocument umstellen**

In `main/ConfigManager.cpp`, in `loadConfig()` die Zeile

```cpp
    StaticJsonDocument<8192> doc;
```

ersetzen durch:

```cpp
    DynamicJsonDocument doc(16384);
```

- [ ] **Step 2: `saveConfig` auf DynamicJsonDocument + Overflow-Check umstellen**

In `saveConfig()` die Zeile

```cpp
    StaticJsonDocument<8192> doc;
```

ersetzen durch:

```cpp
    DynamicJsonDocument doc(16384);
```

Und am Ende von `saveConfig()` den Schreibblock erweitern. Aktuell:

```cpp
    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();
    //loadConfig();
    return true;
```

ersetzen durch:

```cpp
    if (doc.overflowed()) {
        Serial.println("saveConfig: JSON-Dokument zu klein (overflow) – NICHT gespeichert");
        return false;
    }

    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();
    Serial.printf("saveConfig: %u Bytes nach /config.json geschrieben\n", (unsigned)written);
    return true;
```

- [ ] **Step 3: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 4: Commit**

```bash
git add main/ConfigManager.cpp
git commit -m "fix: detect config JSON overflow instead of writing truncated file

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Persistenz – Chunked POST-Handler für Bindings (A1)

**Files:**
- Modify: `main/WebServerManager.cpp` (`registerBindingsRoutes`, Route `POST /control_bindings`)

- [ ] **Step 1: Body-Handler auf Multi-Chunk-Akkumulation umbauen**

In `main/WebServerManager.cpp`, in `registerBindingsRoutes()` den `POST /control_bindings`-Block

```cpp
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
```

vollständig ersetzen durch:

```cpp
    server.on("/control_bindings", HTTP_POST,
        [config](AsyncWebServerRequest* request){
            // Antwort erst nach vollständigem Body (siehe Body-Handler) – hier nur Fallback.
            if (!request->_tempObject) {
                request->send(400, "text/plain", "no body");
            }
        },
        NULL,
        [config](AsyncWebServerRequest* request, uint8_t *data, size_t len, size_t index, size_t total){
            // Body über alle Chunks in einem String akkumulieren.
            if (index == 0) {
                String* buf = new String();
                buf->reserve(total);
                request->_tempObject = buf;
            }
            String* buf = reinterpret_cast<String*>(request->_tempObject);
            if (!buf) return;
            buf->concat((const char*)data, len);

            if (index + len == total) {
                Serial.printf("control_bindings: %u Bytes empfangen\n", (unsigned)total);
                DynamicJsonDocument bd(16384);
                DeserializationError err = deserializeJson(bd, *buf);
                if (err) {
                    Serial.printf("control_bindings: Parse-Fehler: %s\n", err.c_str());
                    request->send(400, "text/plain", "invalid json");
                } else {
                    String normalized;
                    serializeJson(bd, normalized);
                    config->setControlBindingsJson(normalized);
                    bool ok = config->saveConfig();
                    Serial.printf("control_bindings: gespeichert=%s\n", ok ? "true" : "false");
                    request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "save failed");
                }
                delete buf;
                request->_tempObject = nullptr;
            }
        }
    );
```

- [ ] **Step 2: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 3: Manueller Test (Nutzer)**

Flashen (`pio run -e esp32dev -t upload`), Editor öffnen (`/controls`), ein Binding ändern,
„Speichern" klicken (Status „Gespeichert ✓"), Seite neu laden → Binding bleibt erhalten.
Serial-Monitor zeigt `control_bindings: ... Bytes` und `gespeichert=true`.

- [ ] **Step 4: Commit**

```bash
git add main/WebServerManager.cpp
git commit -m "fix: accumulate chunked POST body for control_bindings

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Button-Masken A/B/X/Y korrigieren (C)

**Files:**
- Modify: `main/InputBindingManager.cpp` (`buttonMaskFromString`)

- [ ] **Step 1: Korrekte Bluepad32-Bits eintragen**

In `main/InputBindingManager.cpp` in `buttonMaskFromString` die vier ersten Zeilen

```cpp
    if (code == "BTN_A") return 2;
    if (code == "BTN_B") return 1;
    if (code == "BTN_X") return 8;
    if (code == "BTN_Y") return 4;
```

ersetzen durch (echte Bluepad32-Werte: A=BIT0, B=BIT1, X=BIT2, Y=BIT3):

```cpp
    if (code == "BTN_A") return 1;
    if (code == "BTN_B") return 2;
    if (code == "BTN_X") return 4;
    if (code == "BTN_Y") return 8;
```

- [ ] **Step 2: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 3: Manueller Test (Nutzer)**

Nach Flashen ein Binding auf „Kreuz ×" (BTN_A) legen → Drücken von Kreuz löst nun die
Aktion aus (vorher Kreis). X/Y analog. (Endgültig bestätigt mit der Live-Ansicht aus Task 11.)

- [ ] **Step 4: Commit**

```bash
git add main/InputBindingManager.cpp
git commit -m "fix: correct A/B/X/Y button bit masks to match Bluepad32

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: LED-Helligkeit & Gamma in ConfigManager persistieren (D1/D2)

**Files:**
- Modify: `main/ConfigManager.h` (Member, Getter/Setter-Deklarationen)
- Modify: `main/ConfigManager.cpp` (`setDefaults`, `loadConfig`, `saveConfig`, Getter/Setter)

- [ ] **Step 1: Member + Deklarationen in `ConfigManager.h`**

In `main/ConfigManager.h` bei den LED-bezogenen Membern (nach `int led_count;`) ergänzen:

```cpp
    int led_brightness = 50;   // 0..255 globale FastLED-Helligkeit
    bool led_gamma = false;    // Gamma-Korrektur an/aus
```

Bei den öffentlichen Gettern (nach `int getLedCount();`) ergänzen:

```cpp
    int getLedBrightness();
    bool getLedGamma();
```

Bei den öffentlichen Settern (nach `void setLedCount(int count);`) ergänzen:

```cpp
    void setLedBrightness(int value);
    void setLedGamma(bool enabled);
```

- [ ] **Step 2: Defaults setzen**

In `main/ConfigManager.cpp` in `setDefaults()` nach `led_count = 30;` ergänzen:

```cpp
    led_brightness = 50;
    led_gamma = false;
```

- [ ] **Step 3: Laden**

In `loadConfig()` nach `led_count = doc["led_count"] | 30;` ergänzen:

```cpp
    led_brightness = doc["led_brightness"] | 50;
    led_gamma = doc["led_gamma"] | false;
```

- [ ] **Step 4: Speichern**

In `saveConfig()` nach `doc["led_count"] = led_count;` ergänzen:

```cpp
    doc["led_brightness"] = led_brightness;
    doc["led_gamma"] = led_gamma;
```

- [ ] **Step 5: Getter/Setter implementieren**

In `main/ConfigManager.cpp` nach `int ConfigManager::getLedCount() { return led_count; }` ergänzen:

```cpp
int ConfigManager::getLedBrightness() { return led_brightness; }
bool ConfigManager::getLedGamma() { return led_gamma; }
```

Und nach `void ConfigManager::setLedCount(int count){ led_count = count; }` ergänzen:

```cpp
void ConfigManager::setLedBrightness(int value){
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    led_brightness = value;
}
void ConfigManager::setLedGamma(bool enabled){ led_gamma = enabled; }
```

- [ ] **Step 6: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 7: Commit**

```bash
git add main/ConfigManager.h main/ConfigManager.cpp
git commit -m "feat: persist LED brightness and gamma in config

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: LEDController um Helligkeit & Gamma erweitern (D1/D2)

**Files:**
- Modify: `main/LEDController.h` (Methoden + Member)
- Modify: `main/LEDController.cpp`

- [ ] **Step 1: Deklarationen in `LEDController.h`**

In `main/LEDController.h` bei den public-Methoden (nach `CRGB getLEDColor(int ledIndex);`) ergänzen:

```cpp
    void setBrightness(uint8_t value);
    void setGamma(bool enabled);
```

Bei den privaten Membern (nach `int dataPin = 2;`) ergänzen:

```cpp
    bool gammaEnabled = false;
```

- [ ] **Step 2: Gamma in `setPixelColor` anwenden**

In `main/LEDController.cpp` die Funktion `setPixelColor` ersetzen durch:

```cpp
void LEDController::setPixelColor(int led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led < 0 || led >= ledCount) {
        Serial.printf("LED index out of range: %d\n", led);
        return;
    }
    CRGB c(red, green, blue);
    if (gammaEnabled) c.napplyGamma_video(2.2f);
    ledsArray[led] = c;
}
```

- [ ] **Step 3: `setBrightness` und `setGamma` implementieren**

In `main/LEDController.cpp` am Dateiende ergänzen:

```cpp
void LEDController::setBrightness(uint8_t value) {
    FastLED.setBrightness(value);
    FastLED.show();
}

void LEDController::setGamma(bool enabled) {
    gammaEnabled = enabled;
}
```

- [ ] **Step 4: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 5: Commit**

```bash
git add main/LEDController.h main/LEDController.cpp
git commit -m "feat: add brightness and gamma control to LEDController

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Board-Anbindung, Boot-Default & WS-Befehle für LED (D1/D2/D3/D4-Backend)

**Files:**
- Modify: `main/TinkerThinkerBoard.h` (Methoden)
- Modify: `main/TinkerThinkerBoard.cpp` (Boot-Default, Apply-Brightness, Delegations)
- Modify: `main/WebServerManager.cpp` (WS-Befehle `led_brightness`/`led_gamma`, Endpoint `/saveLedSettings`)
- Modify: `main/WebServerManager.h` (falls Hilfsdeklaration nötig – hier keine)

- [ ] **Step 1: Board-Methoden deklarieren**

In `main/TinkerThinkerBoard.h` nach `CRGB getLEDColor(int ledIndex);` ergänzen:

```cpp
    void setLedBrightness(uint8_t value);
    void setLedGamma(bool enabled);
```

- [ ] **Step 2: Boot-Default auf „aus" + gespeicherte Helligkeit/Gamma anwenden**

In `main/TinkerThinkerBoard.cpp` den LED-Init-Block (aktuell)

```cpp
    ledController = new LEDController(config->getLedCount());
    ledController->init();
    for (int i = 0; i < config->getLedCount(); i++) {
        ledController->setPixelColor(i, 60, 60, 60);
    }
    FastLED.show();
```

ersetzen durch:

```cpp
    ledController = new LEDController(config->getLedCount());
    ledController->init();
    ledController->setGamma(config->getLedGamma());
    FastLED.setBrightness(config->getLedBrightness());
    for (int i = 0; i < config->getLedCount(); i++) {
        ledController->setPixelColor(i, 0, 0, 0); // Boot-Default: aus (nicht „weiß")
    }
    FastLED.show();
```

- [ ] **Step 3: Delegations-Methoden implementieren**

In `main/TinkerThinkerBoard.cpp` nach `CRGB TinkerThinkerBoard::getLEDColor(int ledIndex) { return ledController->getLEDColor(ledIndex); }` ergänzen:

```cpp
void TinkerThinkerBoard::setLedBrightness(uint8_t value) {
    if (ledController) ledController->setBrightness(value);
}

void TinkerThinkerBoard::setLedGamma(bool enabled) {
    if (ledController) ledController->setGamma(enabled);
}
```

- [ ] **Step 4: WS-Befehle `led_brightness` und `led_gamma` (live)**

In `main/WebServerManager.cpp` in `onWebSocketEvent`, direkt nach dem `led_set`-Block
(nach dessen abschließendem `}`) ergänzen:

```cpp
            // LED-Helligkeit live setzen (Vorschau, ohne Speichern): {"led_brightness":0-255}
            if (doc.containsKey("led_brightness")) {
                int b = doc["led_brightness"].as<int>();
                if (b < 0) b = 0; if (b > 255) b = 255;
                board->setLedBrightness((uint8_t)b);
            }
            // Gamma live umschalten: {"led_gamma":true|false}
            if (doc.containsKey("led_gamma")) {
                board->setLedGamma(doc["led_gamma"].as<bool>());
            }
```

- [ ] **Step 5: Persistier-Endpoint `/saveLedSettings`**

In `main/WebServerManager.cpp` in `setupRoutes()` (bei den übrigen `server.on(...)`-Routen)
ergänzen:

```cpp
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
```

- [ ] **Step 6: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 7: Manueller Test (Nutzer)**

Nach Flashen: Beim Booten leuchten die LEDs **nicht** weiß (aus). (Live-Steuerung folgt im
Frontend, Task 12.) Per WS-Tool / Browser-Konsole `ws.send(JSON.stringify({led_brightness:30}))`
und `{led_set:{start:0,count:5,color:"#ff8000"}}` → Orange erscheint deutlich gesättigter/dunkler.

- [ ] **Step 8: Commit**

```bash
git add main/TinkerThinkerBoard.h main/TinkerThinkerBoard.cpp main/WebServerManager.cpp
git commit -m "feat: LED brightness/gamma live WS + save endpoint, boot default off

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: Controller-Input-Snapshot im Board (B1)

**Files:**
- Modify: `main/TinkerThinkerBoard.h` (Struct, Array, Methoden)
- Modify: `main/TinkerThinkerBoard.cpp` (Implementierung)

- [ ] **Step 1: Header-Include für Bluepad32 + Struct + API in `TinkerThinkerBoard.h`**

Oben in `main/TinkerThinkerBoard.h` nach `#include <functional>` ergänzen:

```cpp
#include <Bluepad32.h>
```

Vor `class TinkerThinkerBoard {` ergänzen:

```cpp
struct ControllerInputSnapshot {
    bool     connected = false;
    uint32_t buttons   = 0;
    uint8_t  dpad      = 0;
    int16_t  axisX = 0, axisY = 0, axisRX = 0, axisRY = 0;
    uint32_t updatedMs = 0;
};
```

In der public-Sektion (nach `void notifyControllerDisconnected(int slot);`) ergänzen:

```cpp
    void updateControllerSnapshot(int idx, ControllerPtr ctl);
    void clearControllerSnapshot(int idx);
    const ControllerInputSnapshot& getControllerSnapshot(int idx) const;
```

In der private-Sektion (nach `int motorRightGUI = 3;`) ergänzen:

```cpp
    ControllerInputSnapshot controllerSnapshots[BP32_MAX_GAMEPADS];
```

- [ ] **Step 2: Implementierung in `TinkerThinkerBoard.cpp`**

Am Dateiende von `main/TinkerThinkerBoard.cpp` ergänzen:

```cpp
void TinkerThinkerBoard::updateControllerSnapshot(int idx, ControllerPtr ctl) {
    if (idx < 0 || idx >= BP32_MAX_GAMEPADS || !ctl) return;
    ControllerInputSnapshot& s = controllerSnapshots[idx];
    s.connected = true;
    s.buttons   = ctl->buttons();
    s.dpad      = ctl->dpad();
    s.axisX     = ctl->axisX();
    s.axisY     = ctl->axisY();
    s.axisRX    = ctl->axisRX();
    s.axisRY    = ctl->axisRY();
    s.updatedMs = millis();
}

void TinkerThinkerBoard::clearControllerSnapshot(int idx) {
    if (idx < 0 || idx >= BP32_MAX_GAMEPADS) return;
    controllerSnapshots[idx] = ControllerInputSnapshot();
}

const ControllerInputSnapshot& TinkerThinkerBoard::getControllerSnapshot(int idx) const {
    static const ControllerInputSnapshot empty;
    if (idx < 0 || idx >= BP32_MAX_GAMEPADS) return empty;
    return controllerSnapshots[idx];
}
```

- [ ] **Step 3: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 4: Commit**

```bash
git add main/TinkerThinkerBoard.h main/TinkerThinkerBoard.cpp
git commit -m "feat: store latest controller input snapshot on board

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: Snapshot aus processGamepad/Disconnect befüllen (B1)

**Files:**
- Modify: `main/sketch.cpp` (`processGamepad`, `onDisconnectedController`)

- [ ] **Step 1: Snapshot in `processGamepad` aktualisieren**

In `main/sketch.cpp` in `processGamepad` den Block

```cpp
    int idx = findControllerIndex(ctl);
    if (idx >= 0) inputBindings.process(ctl, idx);
```

ersetzen durch:

```cpp
    int idx = findControllerIndex(ctl);
    if (idx >= 0) {
        board.updateControllerSnapshot(idx, ctl);
        inputBindings.process(ctl, idx);
    }
```

- [ ] **Step 2: Snapshot bei Disconnect löschen**

In `onDisconnectedController` nach `board.notifyControllerDisconnected(i);` ergänzen:

```cpp
            board.clearControllerSnapshot(i);
```

- [ ] **Step 3: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 4: Commit**

```bash
git add main/sketch.cpp
git commit -m "feat: update controller snapshot per frame and clear on disconnect

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 9: Telemetrie um `controllers`-Array erweitern (B2)

**Files:**
- Modify: `main/WebServerManager.cpp` (`sendStatusUpdate`)

- [ ] **Step 1: Dokument vergrößern + Controller-Array senden**

In `main/WebServerManager.cpp` in `sendStatusUpdate()` die Zeile

```cpp
            StaticJsonDocument<512> doc;
```

ersetzen durch:

```cpp
            StaticJsonDocument<1024> doc;
```

Direkt vor `String jsonString;` (nach dem `firstLED`-Block) ergänzen:

```cpp
            JsonArray ctrls = doc.createNestedArray("controllers");
            for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
                const ControllerInputSnapshot& s = board->getControllerSnapshot(i);
                if (!s.connected) continue;
                JsonObject c = ctrls.createNestedObject();
                c["idx"]     = i;
                c["buttons"] = s.buttons;
                c["dpad"]    = s.dpad;
                c["x"]       = s.axisX;
                c["y"]       = s.axisY;
                c["rx"]      = s.axisRX;
                c["ry"]      = s.axisRY;
            }
```

- [ ] **Step 2: Sicherstellen, dass der Bluepad32-Header verfügbar ist**

`main/WebServerManager.cpp` inkludiert `TinkerThinkerBoard.h` (das nun `<Bluepad32.h>` zieht);
falls der Compiler `BP32_MAX_GAMEPADS` nicht kennt, oben in `WebServerManager.cpp` nach den
vorhandenen Includes `#include "TinkerThinkerBoard.h"` ergänzen (nur falls noch nicht vorhanden).

- [ ] **Step 3: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 4: Commit**

```bash
git add main/WebServerManager.cpp
git commit -m "feat: include raw controller input in WS telemetry

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 10: Telemetrie-Rate auf 10 Hz erhöhen (B2)

**Files:**
- Modify: `main/TinkerThinkerBoard.cpp` (`startServices`, WebClientTask-Delay)

- [ ] **Step 1: Task-Delay senken**

In `main/TinkerThinkerBoard.cpp` in `startServices()` die Zeile

```cpp
            vTaskDelay(1000 / portTICK_PERIOD_MS);
```

(im `WebClientTask`-Lambda) ersetzen durch:

```cpp
            vTaskDelay(100 / portTICK_PERIOD_MS);
```

- [ ] **Step 2: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 3: Commit**

```bash
git add main/TinkerThinkerBoard.cpp
git commit -m "perf: raise web telemetry rate to 10 Hz for live input view

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 11: Live-Button-Highlight im Editor (B3)

**Files:**
- Modify: `data/controls.html` (CSS `.live-pressed`, Status-Element)
- Modify: `data/controls.js` (WebSocket, Dekodierung, Highlight)

- [ ] **Step 1: CSS + Live-Status in `controls.html`**

In `data/controls.html` im `<style>`-Block (z.B. nach der `.ctrl-btn.s-sel`-Regel) ergänzen:

```css
    .ctrl-btn.live-pressed { box-shadow:0 0 0 3px #e74c3c inset; background:#ffd9d4 !important; }
    .live-status { margin:8px 0; font-size:13px; }
    .live-status .dot { display:inline-block; width:10px; height:10px; border-radius:50%; background:#bbb; margin-right:6px; vertical-align:middle; }
    .live-status.on .dot { background:#2ecc71; }
```

Direkt nach der `<div class="ctrl-legend">…</div>` (vor dem schließenden `</div>` der
`controller-wrap`) ergänzen:

```html
        <div class="live-status" id="liveStatus"><span class="dot"></span><span id="liveStatusText">Kein Controller (Live)</span></div>
```

- [ ] **Step 2: WebSocket + Live-Highlight in `controls.js`**

In `data/controls.js` vor der abschließenden `})();`-Zeile (nach `load();`) ergänzen:

```javascript
  // ── Live-Button-Ansicht ───────────────────────────────────
  // Echte Bluepad32-Bits (NICHT die Engine-Masken) → zeigt den physischen Button.
  const LIVE_BUTTON_BITS = {
    'button_BTN_A':1, 'button_BTN_B':2, 'button_BTN_X':4, 'button_BTN_Y':8,
    'button_BUTTON_L1':16, 'button_BUTTON_R1':32, 'button_BUTTON_L2':64, 'button_BUTTON_R2':128,
    'button_BUTTON_STICK_L':256, 'button_BUTTON_STICK_R':512
  };
  const LIVE_DPAD_BITS = { 'dpad_UP':1, 'dpad_DOWN':2, 'dpad_RIGHT':4, 'dpad_LEFT':8 };
  const AXIS_DEADBAND = 80;

  let liveTimeout = null;

  function setLivePressed(key, pressed) {
    const el = document.querySelector(`.ctrl-btn[data-key="${key}"]`);
    if (el) el.classList.toggle('live-pressed', pressed);
  }

  function applyLive(ctrl) {
    for (const [key, bit] of Object.entries(LIVE_BUTTON_BITS)) {
      setLivePressed(key, (ctrl.buttons & bit) !== 0);
    }
    for (const [key, bit] of Object.entries(LIVE_DPAD_BITS)) {
      setLivePressed(key, (ctrl.dpad & bit) !== 0);
    }
    const lActive = Math.abs(ctrl.x) > AXIS_DEADBAND || Math.abs(ctrl.y) > AXIS_DEADBAND;
    const rActive = Math.abs(ctrl.rx) > AXIS_DEADBAND || Math.abs(ctrl.ry) > AXIS_DEADBAND;
    setLivePressed('axis_LStick', lActive);
    setLivePressed('axis_RStick', rActive);
  }

  function clearLive() {
    document.querySelectorAll('.ctrl-btn.live-pressed').forEach(el => el.classList.remove('live-pressed'));
  }

  function setLiveStatus(connected) {
    const box = document.getElementById('liveStatus');
    const txt = document.getElementById('liveStatusText');
    if (!box || !txt) return;
    box.classList.toggle('on', connected);
    txt.textContent = connected ? 'Controller verbunden (Live)' : 'Kein Controller (Live)';
    if (!connected) clearLive();
  }

  function connectLiveWS() {
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    ws.onmessage = (event) => {
      let msg;
      try { msg = JSON.parse(event.data); } catch (_) { return; }
      const ctrls = msg.controllers;
      if (!Array.isArray(ctrls) || ctrls.length === 0) { setLiveStatus(false); return; }
      setLiveStatus(true);
      applyLive(ctrls[0]);
      if (liveTimeout) clearTimeout(liveTimeout);
      liveTimeout = setTimeout(() => setLiveStatus(false), 1500);
    };
    ws.onclose = () => { setLiveStatus(false); setTimeout(connectLiveWS, 1000); };
    ws.onerror = () => { try { ws.close(); } catch (_) {} };
  }
  connectLiveWS();
```

- [ ] **Step 3: Build-Check (Frontend-Image)**

Run: `pio run -e esp32dev -t buildfs`
Expected: Baut das LittleFS-Image ohne Fehler.

- [ ] **Step 4: Manueller Test (Nutzer)**

`pio run -e esp32dev -t uploadfs`, dann `/controls` öffnen, Controller verbinden. Beim Drücken
von Tasten/Sticks leuchtet das entsprechende Element rot auf. Damit lässt sich der A/B/X/Y-Fix
aus Task 3 visuell bestätigen (Kreuz drücken → „×"-Feld leuchtet).

- [ ] **Step 5: Commit**

```bash
git add data/controls.html data/controls.js
git commit -m "feat: live controller input highlighting in editor

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 12: Live-Werkzeuge-Panel (LED + Servo/Motor-Test) (D4 + E1)

**Files:**
- Modify: `data/controls.html` (neuer Bereich „Live-Werkzeuge")
- Modify: `data/controls.js` (WS-Sender, Slider-Handler; nutzt die WS-Instanz aus Task 11)

- [ ] **Step 1: Live-WS-Instanz aus Task 11 wiederverwendbar machen**

In `data/controls.js` die in Task 11 angelegte Funktion `connectLiveWS` so anpassen, dass die
Socket-Instanz in einer äußeren Variable gehalten wird. Die Zeile

```javascript
  let liveTimeout = null;
```

ersetzen durch:

```javascript
  let liveTimeout = null;
  let liveWS = null;
  function liveSend(obj) {
    if (liveWS && liveWS.readyState === WebSocket.OPEN) { liveWS.send(JSON.stringify(obj)); return true; }
    return false;
  }
```

Und in `connectLiveWS` die Zeile

```javascript
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
```

ersetzen durch:

```javascript
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    liveWS = ws;
```

- [ ] **Step 2: HTML-Bereich „Live-Werkzeuge" in `controls.html`**

In `data/controls.html` direkt vor `<script src="/controls.js"></script>` ergänzen:

```html
  <div class="controls" style="margin-top:16px;">
    <h2>Live-Werkzeuge (Test)</h2>
    <p class="hint" style="color:#a00;">Achtung: Aktionen werden sofort ausgeführt – Roboter ggf. aufbocken!</p>

    <h3 style="margin:10px 0 4px;">LED-Test</h3>
    <div class="frow">
      <label>Farbe:</label> <input type="color" id="ledColor" value="#ff8000">
      <button class="control-button" id="ledApply" style="padding:6px 12px;">Auf alle anwenden</button>
      <button class="control-button" id="ledOff" style="padding:6px 12px;">Aus</button>
    </div>
    <div class="frow">
      <label>Helligkeit:</label> <input type="range" id="ledBrightness" min="0" max="255" value="50">
      <span id="ledBrightnessVal">50</span>
      <label><input type="checkbox" id="ledGamma"> Gamma</label>
      <button class="control-button" id="ledSave" style="padding:6px 12px;">Als Standard speichern</button>
    </div>

    <h3 style="margin:14px 0 4px;">Servo-Test</h3>
    <div class="frow" id="servoTest"></div>

    <h3 style="margin:14px 0 4px;">Motor-Test (roh, ohne Deadband)</h3>
    <div class="frow" id="motorTest"></div>
    <button class="control-button" id="motorStopAll" style="padding:6px 12px; margin-top:6px;">Alle Motoren stop</button>

    <span id="liveStatus2" style="margin-left:8px; font-size:12px; color:#888;"></span>
  </div>
```

- [ ] **Step 3: Handler in `controls.js`**

In `data/controls.js` direkt nach `connectLiveWS();` (Ende des in Task 11 ergänzten Blocks)
ergänzen:

```javascript
  // ── Live-Werkzeuge: LED ───────────────────────────────────
  const ledColor = document.getElementById('ledColor');
  const ledBrightness = document.getElementById('ledBrightness');
  const ledBrightnessVal = document.getElementById('ledBrightnessVal');
  const ledGamma = document.getElementById('ledGamma');
  let ledCount = 30;
  fetch('/getConfig').then(r=>r.json()).then(cfg=>{
    if (cfg.led_count) ledCount = cfg.led_count;
    if (typeof cfg.led_brightness === 'number') { ledBrightness.value = cfg.led_brightness; ledBrightnessVal.textContent = cfg.led_brightness; }
    if (typeof cfg.led_gamma === 'boolean') ledGamma.checked = cfg.led_gamma;
  }).catch(()=>{});

  document.getElementById('ledApply').addEventListener('click', () => {
    liveSend({led_set:{start:0, count:ledCount, color:ledColor.value}});
  });
  document.getElementById('ledOff').addEventListener('click', () => {
    liveSend({led_set:{start:0, count:ledCount, color:'#000000'}});
  });
  ledBrightness.addEventListener('input', () => {
    ledBrightnessVal.textContent = ledBrightness.value;
    liveSend({led_brightness: parseInt(ledBrightness.value, 10)});
  });
  ledGamma.addEventListener('change', () => {
    liveSend({led_gamma: ledGamma.checked});
    liveSend({led_set:{start:0, count:ledCount, color:ledColor.value}}); // sichtbar neu zeichnen
  });
  document.getElementById('ledSave').addEventListener('click', () => {
    const g = ledGamma.checked ? '1' : '0';
    fetch(`/saveLedSettings?brightness=${parseInt(ledBrightness.value,10)}&gamma=${g}`)
      .then(r => { document.getElementById('liveStatus2').textContent = r.ok ? 'LED gespeichert ✓' : 'Fehler'; });
  });

  // ── Live-Werkzeuge: Servos ────────────────────────────────
  const servoTest = document.getElementById('servoTest');
  for (let i = 0; i < 3; i++) {
    const lbl = document.createElement('label'); lbl.textContent = `Servo ${i}:`;
    const sl = document.createElement('input');
    sl.type = 'range'; sl.min = '0'; sl.max = '180'; sl.value = '90';
    const val = document.createElement('span'); val.textContent = '90';
    sl.addEventListener('input', () => { val.textContent = sl.value; liveSend({["servo"+i]: parseInt(sl.value,10)}); });
    servoTest.append(lbl, sl, val, document.createTextNode(' '));
  }

  // ── Live-Werkzeuge: Motoren (roh) ─────────────────────────
  const motorTest = document.getElementById('motorTest');
  for (let i = 0; i < 4; i++) {
    const lbl = document.createElement('label'); lbl.textContent = `Motor ${i}:`;
    const sl = document.createElement('input');
    sl.type = 'range'; sl.min = '-255'; sl.max = '255'; sl.value = '0';
    const val = document.createElement('span'); val.textContent = '0';
    sl.addEventListener('input', () => { val.textContent = sl.value; liveSend({motor_raw:{motor:i, pwm:parseInt(sl.value,10)}}); });
    sl.addEventListener('change', () => { sl.value = '0'; val.textContent = '0'; liveSend({motor_raw:{motor:i, pwm:0}}); }); // Loslassen → stop
    motorTest.append(lbl, sl, val, document.createTextNode(' '));
  }
  document.getElementById('motorStopAll').addEventListener('click', () => {
    for (let i = 0; i < 4; i++) liveSend({motor_raw:{motor:i, pwm:0}});
    motorTest.querySelectorAll('input[type="range"]').forEach(sl => sl.value = '0');
  });
```

- [ ] **Step 4: Build-Check (Frontend-Image)**

Run: `pio run -e esp32dev -t buildfs`
Expected: Baut ohne Fehler.

- [ ] **Step 5: Manueller Test (Nutzer)**

`pio run -e esp32dev -t uploadfs`, `/controls` öffnen:
- LED-Farbe wählen + „Auf alle anwenden" → Streifen zeigt die Farbe; Helligkeits-Slider regelt
  live; Gamma-Häkchen verändert Sättigung; „Als Standard speichern" persistiert (Reload prüfen).
- Servo-Slider bewegt den Servo live; Motor-Slider dreht den Motor (stoppt beim Loslassen);
  „Alle Motoren stop" funktioniert.

- [ ] **Step 6: Commit**

```bash
git add data/controls.html data/controls.js
git commit -m "feat: live test tools for LED, servos and motors in editor

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 13: Erweiterte Binding-Aktionen – Firmware (E2)

Neue Aktionstypen: `servo_sweep` (Pendeln zwischen zwei Winkeln) und `motor_ramp` (weiches
Anfahren auf Ziel-PWM). Beide laufen über die bestehende `tick()`-Schleife.

**Files:**
- Modify: `main/InputBindingManager.h` (State-Arrays + Methoden)
- Modify: `main/InputBindingManager.cpp` (`applyAction`, `tick`, Sweep-/Ramp-Logik)

- [ ] **Step 1: State + Deklarationen in `InputBindingManager.h`**

In `main/InputBindingManager.h` in der private-Sektion (nach `int servoBand[3] = {0,0,0};`) ergänzen:

```cpp
    // servo_sweep state (pro Servo 0..2)
    bool    sweepActive[3]   = {false,false,false};
    int     sweepFrom[3]     = {0,0,0};
    int     sweepTo[3]       = {180,180,180};
    int     sweepStep[3]     = {2,2,2};
    int     sweepPos[3]      = {0,0,0};
    int     sweepDir[3]      = {1,1,1};
    uint32_t sweepNextMs[3]  = {0,0,0};

    // motor_ramp state (pro Motor 0..3)
    bool     rampActive[4]   = {false,false,false,false};
    int      rampTarget[4]   = {0,0,0,0};
    int      rampCurrent[4]  = {0,0,0,0};
    int      rampStep[4]     = {10,10,10,10};
    uint32_t rampNextMs[4]   = {0,0,0,0};
```

Bei den Helper-Deklarationen (nach `void applyMotorHold(JsonObject action, uint32_t tickMs);`) ergänzen:

```cpp
    void startServoSweep(JsonObject action);
    void startMotorRamp(JsonObject action);
    void tickSweepsAndRamps(uint32_t now);
```

- [ ] **Step 2: Aktionen in `applyAction` ergänzen**

In `main/InputBindingManager.cpp` in `applyAction`, vor der schließenden `}` der
`else if`-Kette (direkt nach dem `motor_direct`-Block) ergänzen:

```cpp
    } else if (!strcmp(type, "servo_sweep")) {
        startServoSweep(action);
    } else if (!strcmp(type, "motor_ramp")) {
        startMotorRamp(action);
    }
```

- [ ] **Step 3: Sweep/Ramp-Logik implementieren**

In `main/InputBindingManager.cpp` nach `applyMotorHold(...)` (vor `tick()`) ergänzen:

```cpp
void InputBindingManager::startServoSweep(JsonObject action) {
    int idx = action["servo"] | 0;
    if (idx < 0 || idx >= 3) return;
    int from = action["from"] | 0;
    int to   = action["to"]   | 180;
    int step = action["step"] | 2;
    if (step < 1) step = 1;
    // Toggle: aktiver Sweep wird gestoppt
    if (sweepActive[idx]) { sweepActive[idx] = false; return; }
    sweepFrom[idx] = constrain(from, 0, 180);
    sweepTo[idx]   = constrain(to,   0, 180);
    sweepStep[idx] = step;
    sweepPos[idx]  = sweepFrom[idx];
    sweepDir[idx]  = (sweepTo[idx] >= sweepFrom[idx]) ? 1 : -1;
    sweepNextMs[idx] = millis();
    sweepActive[idx] = true;
}

void InputBindingManager::startMotorRamp(JsonObject action) {
    int idx = action["motor"] | 0;
    if (idx < 0 || idx >= 4) return;
    int target = action["pwm"]  | 0;
    int step   = action["step"] | 10;
    if (step < 1) step = 1;
    rampTarget[idx]  = constrain(target, -255, 255);
    rampStep[idx]    = step;
    rampCurrent[idx] = board->getMotorPWM(idx);
    rampNextMs[idx]  = millis();
    rampActive[idx]  = true;
}

void InputBindingManager::tickSweepsAndRamps(uint32_t now) {
    // Servo-Sweeps (alle 20 ms ein Schritt)
    for (int i = 0; i < 3; i++) {
        if (!sweepActive[i]) continue;
        if (now < sweepNextMs[i]) continue;
        sweepNextMs[i] = now + 20;
        sweepPos[i] += sweepDir[i] * sweepStep[i];
        if (sweepDir[i] > 0 && sweepPos[i] >= sweepTo[i])   { sweepPos[i] = sweepTo[i];   sweepDir[i] = -1; }
        else if (sweepDir[i] < 0 && sweepPos[i] <= sweepFrom[i]) { sweepPos[i] = sweepFrom[i]; sweepDir[i] = 1; }
        board->setServoAngle(i, sweepPos[i]);
    }
    // Motor-Rampen (alle 20 ms ein Schritt)
    for (int i = 0; i < 4; i++) {
        if (!rampActive[i]) continue;
        if (now < rampNextMs[i]) continue;
        rampNextMs[i] = now + 20;
        if (rampCurrent[i] < rampTarget[i]) {
            rampCurrent[i] = min(rampCurrent[i] + rampStep[i], rampTarget[i]);
        } else if (rampCurrent[i] > rampTarget[i]) {
            rampCurrent[i] = max(rampCurrent[i] - rampStep[i], rampTarget[i]);
        }
        board->controlMotorDirect(i, rampCurrent[i]);
        if (rampCurrent[i] == rampTarget[i]) rampActive[i] = false;
    }
}
```

- [ ] **Step 4: `tick()` erweitern**

In `main/InputBindingManager.cpp` in `tick()` direkt nach `uint32_t now = millis();` ergänzen:

```cpp
    tickSweepsAndRamps(now);
```

- [ ] **Step 5: Build-Check**

Run: `pio run -e esp32dev`
Expected: Kompiliert ohne Fehler.

- [ ] **Step 6: Manueller Test (Nutzer)**

Nach Flashen ein Binding `servo_sweep` (z.B. Servo 0, from 0, to 180, step 2) auf eine Taste
legen → Servo pendelt; erneuter Druck stoppt. `motor_ramp` (Motor 0, pwm 200, step 10) → Motor
fährt weich hoch.

- [ ] **Step 7: Commit**

```bash
git add main/InputBindingManager.h main/InputBindingManager.cpp
git commit -m "feat: add servo_sweep and motor_ramp binding actions

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 14: Editor-Unterstützung für neue Aktionen (E2 Frontend)

**Files:**
- Modify: `data/controls.js` (`ACTION_TYPES`, Action-Editor `renderPanel`, `syncPanelToModel`, Label-Funktion)

- [ ] **Step 1: Neue Typen registrieren**

In `data/controls.js` die Zeile

```javascript
  const ACTION_TYPES = ['motor_direct','drive_pair','servo_set','servo_toggle_band','servo_nudge','servo_axes','led_set','gpio_set','speed_adjust'];
```

ersetzen durch:

```javascript
  const ACTION_TYPES = ['motor_direct','drive_pair','servo_set','servo_toggle_band','servo_nudge','servo_axes','servo_sweep','motor_ramp','led_set','gpio_set','speed_adjust'];
```

- [ ] **Step 2: Editor-Felder ergänzen**

In `data/controls.js` im Action-Editor (Render-Funktion, die je nach `t` die Felder in
`fieldsDiv` baut) direkt **vor** dem Zweig `} else if (t === 'led_set') {` ergänzen. Verwende
die im File vorhandenen Helfer `mkNum(min, max, value, width)`, `el(...)`, `frow(...)`,
`hint(...)` und `fieldsDiv.append(...)` — exakt wie in den benachbarten Zweigen:

```javascript
      } else if (t === 'servo_sweep') {
        refs.idxEl  = mkNum(0, 2, action.servo ?? 0, 60);
        refs.fromEl = mkNum(0, 180, action.from ?? 0, 60);
        refs.toEl   = mkNum(0, 180, action.to ?? 180, 60);
        refs.stepEl = mkNum(1, 30, action.step ?? 2, 60);
        fieldsDiv.append(
          frow([el('label',{},'Servo:'), refs.idxEl, el('label',{},'Von:'), refs.fromEl, el('label',{},'Bis:'), refs.toEl, el('label',{},'Schritt:'), refs.stepEl]),
          hint('Pendelt den Servo zwischen Von/Bis · erneuter Tastendruck stoppt')
        );
      } else if (t === 'motor_ramp') {
        refs.motorEl = mkNum(0, 3, action.motor ?? 0, 60);
        refs.pwmEl   = mkNum(-255, 255, action.pwm ?? 200, 72);
        refs.stepEl  = mkNum(1, 50, action.step ?? 10, 60);
        fieldsDiv.append(
          frow([el('label',{},'Motor:'), refs.motorEl, el('label',{},'Ziel-PWM:'), refs.pwmEl, el('label',{},'Schritt:'), refs.stepEl]),
          hint('Fährt den Motor weich auf die Ziel-PWM (Schritt pro 20 ms)')
        );
```

- [ ] **Step 3: `collect()`-switch erweitern**

In `data/controls.js` in der `collect()`-Methode des Action-Editors (dort wo
`case 'led_set':` steht) direkt **vor** `case 'led_set':` ergänzen:

```javascript
          case 'servo_sweep':
            a.servo = parseInt(refs.idxEl.value || '0');
            a.from  = parseInt(refs.fromEl.value || '0');
            a.to    = parseInt(refs.toEl.value || '180');
            a.step  = parseInt(refs.stepEl.value || '2');
            break;
          case 'motor_ramp':
            a.motor = parseInt(refs.motorEl.value || '0');
            a.pwm   = parseInt(refs.pwmEl.value || '0');
            a.step  = parseInt(refs.stepEl.value || '10');
            break;
```

- [ ] **Step 4: Listen-Label ergänzen**

In `data/controls.js` bei der Label-Funktion (dort wo `if (act.type === 'led_set') return ...`)
direkt davor ergänzen:

```javascript
    if (act.type === 'servo_sweep')   return `sweep S${act.servo} ${act.from}↔${act.to}`;
    if (act.type === 'motor_ramp')    return `ramp M${act.motor} →${act.pwm}`;
```

- [ ] **Step 5: Build-Check (Frontend-Image)**

Run: `pio run -e esp32dev -t buildfs`
Expected: Baut ohne Fehler.

- [ ] **Step 6: Manueller Test (Nutzer)**

`pio run -e esp32dev -t uploadfs`, `/controls` öffnen, Binding hinzufügen, Aktionstyp
`servo_sweep` bzw. `motor_ramp` wählen → Felder erscheinen, speichern, Reload → bleibt erhalten,
und auf dem Gerät verhält es sich wie in Task 13 beschrieben.

- [ ] **Step 7: Commit**

```bash
git add data/controls.js
git commit -m "feat: editor support for servo_sweep and motor_ramp actions

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Abschluss

- [ ] **Gesamt-Build:** `pio run -e esp32dev` und `pio run -e esp32dev -t buildfs` fehlerfrei.
- [ ] **Manueller End-to-End-Test (Nutzer):** Persistenz, A/B/X/Y, Live-Ansicht, LED-Farben/
  Helligkeit, Servo/Motor-Test, neue Aktionen.
- [ ] **Branch abschließen** über `superpowers:finishing-a-development-branch` (Merge/PR).

## Reihenfolge & Abhängigkeiten

Empfohlene Reihenfolge = Task-Nummerierung. Abhängigkeiten:
- Task 12 nutzt die `liveSend`/`liveWS`-Hilfen aus Task 11 → Task 11 zuerst.
- Task 14 setzt Task 13 (Firmware-Aktionen) voraus.
- Tasks 4–6 (LED) hängen aneinander (Config → Controller → Board/WS).
- Tasks 1–3 sind unabhängig und können zuerst erledigt werden (reine Bugfixes).

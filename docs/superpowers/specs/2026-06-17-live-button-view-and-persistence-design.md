# Design: Controller-Diagnose, Persistenz-Fix, LED- & Servo/Motor-Komfort

**Datum:** 2026-06-17
**Status:** Genehmigt (Design)
**Scope dieser Session (vollständig):**
1. Persistenz-Bug der Control-Bindings beheben (A)
2. Live-Button-Ansicht im Steuerungs-Editor (B)
3. A/B/X/Y-Masken-Fix (C)
4. LED-Farb-Problem („alles weiß") + Live-LED-Steuerung (D)
5. Servo/Motor-Komfort: Live-Test-Slider + erweiterte Aktionen (E)

## Kontext / Problem

Über die Weboberfläche (`controls.html`) lassen sich Controller-Buttons mit Aktionen
belegen. Beim Test traten mehrere Probleme auf:

1. **Bindings werden nicht gespeichert** — nach Reload sind die Einstellungen weg.
2. **A/B/X/Y vertauscht** — Buttons wie Kreuz/Viereck lösen die falsche Aktion aus.
3. **Keine Sichtbarkeit** — man sieht nicht, welcher physische Button welcher ist.
4. **LED-Farben „eher weiß"** — gewählte Farben passen nicht zum LED-Bild.

Zusätzlich soll die Servo-/Motor-Steuerung komfortabler werden.

### Wichtiger Architektur-Befund

Die Firmware verarbeitet in `WebServerManager::onWebSocketEvent()` **bereits** Live-Befehle
über WebSocket (`/ws`):
- Servo: `{"servoN": winkel}` → `board->setServoAngle()`
- Motor (roh, ohne Deadband): `{"motor_raw":{"motor":idx,"pwm":val}}` → `board->controlMotorRaw()`
- LED: `{"led_set":{"start":s,"count":c,"color":"#RRGGBB"}}` → `board->setLED()` + `showLEDs()`

Die Live-Test- und Live-LED-Funktionen (D, E) sind daher überwiegend **Frontend-Arbeit**.

## Teil A — Persistenz-Fix (Bindings speichern)

Gewählter Ansatz: **Minimal-Fix** — Bindings bleiben in `config.json` eingebettet,
aber die zwei Fehlerquellen werden robust gemacht.

### A1. Chunked-POST-Handler robust machen

**Datei:** `main/WebServerManager.cpp`, `registerBindingsRoutes()`, Route `POST /control_bindings`.

Aktuell verarbeitet der Body-Handler nur den ersten Chunk und ignoriert `index`/`total`:

```cpp
String body;
body.concat((const char*)data, len);   // nur 1. Chunk
DynamicJsonDocument bd(8192);
if (deserializeJson(bd, body) == ...) { ... }   // bei mehreren Chunks: kaputt
```

**Fix:** Body über mehrere Chunks akkumulieren (Puffer über `request->_tempObject`),
erst bei `index + len == total` parsen + `setControlBindingsJson()` + `saveConfig()`.
Ergebnis per `Serial.println` loggen (A3).

### A2. JSON-Overflow in `saveConfig()` erkennen statt still abschneiden

**Datei:** `main/ConfigManager.cpp`, `saveConfig()` / `loadConfig()`.

`saveConfig()` baut die gesamte Konfiguration inkl. Deep-Copy der Bindings in einem
`StaticJsonDocument<8192>`. Bei vielen Bindings → Overflow → unvollständiges JSON →
`loadConfig()` schlägt beim Parsen fehl → Defaults bleiben → Bindings „verschwinden".

**Fix (minimal):**
- `StaticJsonDocument<8192>` → `DynamicJsonDocument(16384)` für Save und Load.
- Vor dem Schreiben prüfen: `if (doc.overflowed()) { Serial.println(...); return false; }`
  — niemals stillschweigend eine abgeschnittene Datei schreiben.
- Parse-Fehler in `loadConfig()` klar als Fehler loggen (Logging existiert bereits).

### A3. Verifizierbarkeit

Serial-Logs an den entscheidenden Stellen (POST empfangen/Bytes, Parse-OK/-Fehler,
Save-OK/Overflow, Load-OK/Fehler). Nutzer flasht und verifiziert über Serial-Monitor +
Reload-Test (Flashen kann der Assistent nicht selbst).

## Teil B — Live-Button-Ansicht im Steuerungs-Editor

Anzeige in `controls.html`: das gedrückte Element wird im Controller-Layout hervorgehoben.
Aktionen laufen dabei **normal weiter** (kein Testmodus — bewusst einfachste Variante).

### B1. Firmware — Input-Snapshot im Board

**Dateien:** `main/TinkerThinkerBoard.h/.cpp`, `main/sketch.cpp`.

```cpp
struct ControllerInputSnapshot {
    bool     connected = false;
    uint32_t buttons   = 0;
    uint8_t  dpad      = 0;
    int16_t  axisX = 0, axisY = 0, axisRX = 0, axisRY = 0;
    uint32_t updatedMs = 0;
};
ControllerInputSnapshot latestInput[BP32_MAX_GAMEPADS];
```

- Setter `updateControllerSnapshot(int idx, ControllerPtr ctl)`, aufgerufen in
  `processGamepad()` pro Frame.
- Disconnect → `latestInput[idx].connected = false`.
- Getter für `sendStatusUpdate()`.

### B2. Firmware — Telemetrie erweitern

**Datei:** `main/WebServerManager.cpp`, `sendStatusUpdate()`.
- `StaticJsonDocument<512>` → `<1024>`.
- Neues Feld `controllers`: Array mit `{idx, buttons, dpad, x, y, rx, ry}` je verbundenem Controller.
- **Cadence:** WebClientTask-Delay in `TinkerThinkerBoard.cpp::startServices()` von `1000 ms`
  auf `100 ms` (10 Hz) senken.

### B3. Frontend — controls.js / controls.html

**Dateien:** `data/controls.js`, `data/controls.html`, `data/styles.css`.
- WebSocket zu `ws://<host>/ws` (Muster aus `script.js`/`setup.js`), mit Auto-Reconnect.
- `onmessage`: `controllers` lesen; Buttons mit den **echten** Bluepad32-Bits dekodieren
  (`A=1,B=2,X=4,Y=8,L1=16,R1=32,L2=64,R2=128,STICK_L=256,STICK_R=512`; D-Pad `UP=1,DOWN=2,RIGHT=4,LEFT=8`).
- Gesetzte Bits → Element im Layout mit CSS-Klasse `.live-pressed` hervorheben.
- Achsen/Sticks visualisieren; „verbunden/getrennt"-Status (mit Timeout).

## Teil C — A/B/X/Y-Masken-Fix

**Datei:** `main/InputBindingManager.cpp`, `buttonMaskFromString()`.

Falsche Bit-Werte gegenüber Bluepad32 (`components/bluepad32/include/controller/uni_gamepad.h`):

| UI-Code (Label)    | Firmware (falsch) | Bluepad32 korrekt |
|--------------------|-------------------|-------------------|
| `BTN_A` (Kreuz ×)  | 2                 | **1** (`BIT(0)`)  |
| `BTN_B` (Kreis ○)  | 1                 | **2** (`BIT(1)`)  |
| `BTN_X` (Viereck □)| 8                 | **4** (`BIT(2)`)  |
| `BTN_Y` (Dreieck △)| 4                 | **8** (`BIT(3)`)  |

A↔B und X↔Y sind paarweise vertauscht; L1/R1/L2/R2/Stick und D-Pad sind korrekt.
**Fix:** Werte auf `A=1,B=2,X=4,Y=8` korrigieren. Nach dem Fix stimmen Live-Ansicht (B)
und Binding-Engine überein — direkt mit der Live-View verifizierbar.

## Teil D — LED-Farben: Ursache + Live-Steuerung

**Hardware bestätigt:** WS2812 (RGB) → die Treiber-Konfiguration `WS2812, GRB` in
`LEDController.cpp:10` ist **korrekt**, der Software-Pfad ist sauber (kein Gamma/Mischen).

**Ursache des „alles eher weiß":** In `TinkerThinkerBoard.cpp:61-62` werden beim Boot
**alle** LEDs fest auf `(60,60,60)` (blasses Weiß) gesetzt. Ohne aktive `led_set`-Aktion
bleiben sie so. Es gibt zudem keine direkte Live-Farbsteuerung im UI — nur die
Binding-Aktion `led_set`, die einen Bereich `start..start+count` setzt; nicht erfasste
LEDs bleiben weiß.

### D1. Boot-Default anpassen

**Datei:** `main/TinkerThinkerBoard.cpp`. Boot-Default von `(60,60,60)` auf **aus**
(`0,0,0`) bzw. eine dezente, klar nicht-weiße Farbe ändern (Standard: aus). Damit
startet der Streifen nicht „alles weiß".

### D2. Live-LED-Steuerung im Frontend

**Dateien:** `data/controls.html`/`controls.js` — eigener Bereich „Live-Werkzeuge" (enthält
auch die Servo/Motor-Test-Slider aus E1), Panel „LED-Test":
- Farbwähler (`<input type="color">`) + „auf alle anwenden" + „aus".
- Sendet vorhandenes `{"led_set":{"start":0,"count":<ledCount>,"color":"#rrggbb"}}` über WS.
- Optional: Helligkeits-Slider. Dafür Firmware-seitig WS-Befehl `{"led_brightness":0-255}`
  → `FastLED.setBrightness()` ergänzen (kleiner Zusatz in `onWebSocketEvent` + `LEDController`).

## Teil E — Servo/Motor-Komfort

Backend (WS-Befehle) existiert bereits; Schwerpunkt Frontend + einige Binding-Aktionen.

### E1. Live-Test-Slider im Frontend

**Dateien:** `data/controls.html`/`controls.js` (Panel „Servo/Motor-Test"):
- Servo-Slider 0–180° pro Servo → `{"servoN": winkel}`.
- Motor-Slider −255…255 pro Motor → `{"motor_raw":{"motor":idx,"pwm":val}}`; „Stop"-Button → 0.
- Sicherheits-Hinweis/Not-Stop, da Aktionen real ausgeführt werden.

### E2. Erweiterte Binding-Aktionen (Firmware)

**Datei:** `main/InputBindingManager.cpp` (+ ggf. `ConfigManager` Default-Bindings,
`controls.js` Editor-Optionen):
- `servo_axes` verbessern: konfigurierbares Limit (min/max Winkel), Mittelstellung,
  Geschwindigkeit/Glättung statt harter Sprünge.
- `servo_sweep`: Servo zwischen zwei Winkeln pendeln (Start/Stop per Edge).
- `motor_ramp`: weiches Hoch-/Runterfahren der Motor-PWM statt sofortigem Sprung.
- Editor (`controls.js`) um die neuen Aktionstypen/Parameter erweitern.

> Hinweis: E2 ist der umfangreichste Block. Falls der Plan zu groß wird, kann E2 in einen
> Folge-Durchgang ausgelagert werden; E1 (Live-Test) ist günstig und steht zuerst.

## Testing

- **Firmware:** Build + Flash durch den Nutzer; Verifikation über Serial-Monitor
  (Persistenz-Logs A3) und manuelle Tests (Buttons, LED, Servo, Motor).
- **Frontend:** Im Browser gegen das Gerät; Live-Highlights, LED-Farbe, Test-Slider prüfen.
  Optional simulierte WS-Nachricht für UI-Tests ohne Hardware.

## Out of Scope (diese Session)

- Umstieg auf separate Binding-Datei (Minimal-Fix gewählt).
- Tieferer LED-Animationsrahmen (nur statische Farbe + Helligkeit).
- E2 ggf. in Folge-Session, falls Umfang zu groß (Entscheidung beim Planen).

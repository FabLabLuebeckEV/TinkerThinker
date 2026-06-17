# Design: Live-Button-Ansicht & Binding-Persistenz-Fix

**Datum:** 2026-06-17
**Status:** Genehmigt (Design)
**Scope dieser Session:** Persistenz-Bug der Control-Bindings beheben + Live-Button-Ansicht im Steuerungs-Editor.

## Kontext / Problem

Über die Weboberfläche (`controls.html`) lassen sich Controller-Buttons mit Aktionen
belegen. Beim Test traten drei Probleme auf:

1. **Bindings werden nicht gespeichert** — nach einem Reload sind die Einstellungen weg.
2. **A/B/X/Y vertauscht** — Buttons wie Kreuz/Viereck lösen die falsche Aktion aus.
3. **Keine Sichtbarkeit** — man kann nicht sehen, welcher physische Button welcher ist.

Diese Session behandelt **(1)** und **(3)**. Problem **(2)** ist diagnostiziert
(siehe unten) und bewusst zurückgestellt — die Live-Ansicht aus (3) macht den Bug
sichtbar, sodass er anschließend gezielt nachgezogen werden kann.

### Diagnose von (2) — Referenz, nicht in dieser Session gefixt

In `main/InputBindingManager.cpp::buttonMaskFromString()` stehen falsche Bit-Werte
gegenüber den echten Bluepad32-Konstanten
(`components/bluepad32/include/controller/uni_gamepad.h`):

| UI-Code (Label)   | Firmware nutzt | Bluepad32 korrekt |
|-------------------|----------------|-------------------|
| `BTN_A` (Kreuz ×) | 2              | **1** (`BIT(0)`)  |
| `BTN_B` (Kreis ○) | 1              | **2** (`BIT(1)`)  |
| `BTN_X` (Viereck □)| 8             | **4** (`BIT(2)`)  |
| `BTN_Y` (Dreieck △)| 4             | **8** (`BIT(3)`)  |

A↔B und X↔Y sind paarweise vertauscht. L1/R1/L2/R2/Stick-Masken und das D-Pad
(`dpadMaskFromString`) sind korrekt.

## Teil A — Persistenz-Fix (Bindings speichern)

Gewählter Ansatz: **Minimal-Fix** — Bindings bleiben in `config.json` eingebettet,
aber die zwei Fehlerquellen werden robust gemacht.

### A1. Chunked-POST-Handler robust machen

**Datei:** `main/WebServerManager.cpp`, `registerBindingsRoutes()`, Route
`POST /control_bindings`.

Aktuell verarbeitet der Body-Handler nur den ersten Chunk und ignoriert `index`/`total`:

```cpp
String body;
body.reserve(total);
body.concat((const char*)data, len);   // nur 1. Chunk
DynamicJsonDocument bd(8192);
if (deserializeJson(bd, body) == ...) { ... }   // bei mehreren Chunks: kaputt
```

**Fix:** Body über mehrere Chunks akkumulieren und erst beim letzten Chunk
parsen + speichern.

- Puffer pro Request über `request->_tempObject` (ein `String*`), oder ein an den
  Request gebundener Akkumulator.
- Bei `index == 0`: Puffer anlegen, `reserve(total)`.
- Jeden Chunk anhängen: `buf->concat((const char*)data, len)`.
- Wenn `index + len == total`: `deserializeJson` → bei Erfolg `setControlBindingsJson()`
  + `saveConfig()`; Puffer freigeben.
- Speicher-Ergebnis (Erfolg/Fehler) per `Serial.println` loggen (für A3).

### A2. JSON-Overflow in `saveConfig()` erkennen statt still abschneiden

**Datei:** `main/ConfigManager.cpp`, `saveConfig()` / `loadConfig()`.

`saveConfig()` baut die gesamte Konfiguration inkl. Deep-Copy der Bindings in einem
`StaticJsonDocument<8192>`. Bei vielen Bindings kann das überlaufen → `serializeJson`
schreibt unvollständiges JSON → `loadConfig()` schlägt beim Parsen fehl → In-Memory
bleiben die Defaults → Bindings „verschwinden" beim Reload.

**Fix (minimal):**

- Kapazität erhöhen: `StaticJsonDocument<8192>` → `DynamicJsonDocument(16384)` für
  Save und Load (Wert nach realer Größe wählen; großzügig dimensioniert).
- Nach dem Befüllen vor dem Schreiben prüfen: `if (doc.overflowed()) { Serial.println(...); return false; }`
  — niemals stillschweigend eine abgeschnittene Datei schreiben.
- In `loadConfig()` Parse-Fehler weiterhin per Serial loggen (existiert bereits) und
  zusätzlich klar als Fehlerfall behandeln.

### A3. Verifizierbarkeit

Serial-Logs an den entscheidenden Stellen (POST empfangen, Bytes, Parse-OK/-Fehler,
Save-OK/Overflow, Load-OK/Fehler). Der Nutzer flasht und verifiziert über den
Serial-Monitor + Reload-Test (Flashen kann der Assistent nicht selbst).

## Teil B — Live-Button-Ansicht im Steuerungs-Editor

Anzeige direkt in `controls.html`: das aktuell gedrückte Element wird im vorhandenen
Controller-Layout hervorgehoben. Aktionen laufen dabei **normal weiter** (kein
Testmodus — bewusst einfachste Variante).

### B1. Firmware — Input-Snapshot im Board

**Dateien:** `main/TinkerThinkerBoard.h/.cpp`, `main/sketch.cpp`.

Neue Struktur, im Board gehalten:

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

- Setter `updateControllerSnapshot(int idx, ControllerPtr ctl)` füllt den Snapshot aus
  `ctl->buttons()`, `ctl->dpad()`, `ctl->axisX()` … und `updatedMs = millis()`.
- Aufruf in `processGamepad()` (`sketch.cpp`) pro Frame für den jeweiligen Index.
- Bei Disconnect (`onDisconnectedController`) `latestInput[idx].connected = false`.
- Getter für `sendStatusUpdate()` (z.B. `const ControllerInputSnapshot& getControllerSnapshot(int idx)`).

### B2. Firmware — Telemetrie erweitern

**Datei:** `main/WebServerManager.cpp`, `sendStatusUpdate()`.

- `StaticJsonDocument<512>` → `<1024>` (Platz für Controller-Array).
- Neues Feld `controllers`: Array mit je `{idx, buttons, dpad, x, y, rx, ry}` für jeden
  verbundenen Controller. Rohwerte (uint), Dekodierung passiert im Frontend.
- **Update-Cadence:** WebClientTask-Delay in `TinkerThinkerBoard.cpp::startServices()`
  von `1000 ms` auf `100 ms` (10 Hz) senken, damit kurze Tastendrücke sichtbar sind.
  Telemetrie bleibt leichtgewichtig (kleines JSON).

### B3. Frontend — controls.js / controls.html

**Dateien:** `data/controls.js`, `data/controls.html`, `data/styles.css`.

- WebSocket zu `ws://<host>/ws` öffnen (Muster aus `data/script.js`), mit Auto-Reconnect.
- `onmessage`: JSON parsen; falls `controllers` vorhanden, ersten/relevanten Controller nehmen.
- **Button-Dekodierung mit den ECHTEN Bluepad32-Bits** (nicht den vertauschten der Engine):
  - `BTN_A=1, BTN_B=2, BTN_X=4, BTN_Y=8, L1=16, R1=32, L2=64, R2=128, STICK_L=256, STICK_R=512`
  - D-Pad: `UP=1, DOWN=2, RIGHT=4, LEFT=8`
- Für jedes gesetzte Bit das zugehörige Element im Controller-Layout mit CSS-Klasse
  `.live-pressed` hervorheben (entfernen, wenn nicht mehr gedrückt).
- Achsen/Sticks anhand der Werte visualisieren (z.B. Stick-Punkt verschieben oder
  „aktiv"-Highlight bei Auslenkung über Deadband).
- Kleiner Statusindikator „Controller verbunden / getrennt" (aus `connected` bzw.
  Anwesenheit im `controllers`-Array; Timeout, wenn länger keine Daten).

### Diagnose-Effekt (bestätigt Problem 2)

Die Live-Ansicht dekodiert mit den korrekten Bits, die Binding-Engine mit den
vertauschten. Sichtbares Resultat: Kreuz drücken → „Kreuz" leuchtet korrekt, aber eine
Kreuz-Belegung feuert nicht (sie reagiert auf Kreis). Damit ist der A/B/X/Y-Bug
visuell belegt; der Fix (Tausch in `buttonMaskFromString`) kann anschließend gezielt
nachgezogen werden.

## Testing

- **Firmware:** Build + Flash durch den Nutzer; Verifikation über Serial-Monitor
  (Persistenz-Logs aus A3) und Reload-Test der Bindings.
- **Frontend:** Im Browser gegen das Gerät; Live-Highlights beim Drücken prüfen.
  Optional: simulierte WS-Nachricht zum UI-Test ohne Hardware.

## Out of Scope (diese Session)

- **A/B/X/Y-Masken-Fix** in `buttonMaskFromString` (zurückgestellt; durch Live-View aufgedeckt).
- **Servo/Motor-Komfortsteuerung** (eigener späterer Durchgang).
- Umstieg auf separate Binding-Datei (Minimal-Fix gewählt).

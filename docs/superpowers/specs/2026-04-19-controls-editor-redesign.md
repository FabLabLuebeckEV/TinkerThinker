# Design Spec: Controls Editor Redesign (`/controls`)

**Datum:** 2026-04-19  
**Status:** Approved

---

## Ziel

Die `/controls`-Seite wird von einem rohen Binding-Listen-Editor zu einem intuitiven, PS3-Controller-orientierten Konfigurations-Tool erweitert. Alles funktioniert vollständig offline (kein CDN, keine externen Ressourcen).

---

## Seitenstruktur (von oben nach unten)

```
1. Info-Banner (aufklappbar)
2. Controller-View + Binding-Panel (nebeneinander)
3. Binding-Liste + Toolbar (wie bisher, erweitert)
```

---

## 1. Info-Banner

Aufklappbarer `<details>`-Block ganz oben. Kein externer Style nötig – verwendet bestehendes CSS.

**Inhalt:**

### Konzepte
| Begriff | Bedeutung |
|---------|-----------|
| **Input** | Was du drückst: Button, DPAD-Richtung oder Joystick-Achsen |
| **Edge** | `press` = einmalig beim Drücken · `release` = beim Loslassen · `hold` = dauerhaft solange gehalten |
| **Action** | Was passiert: Motor, Servo, LED, GPIO, Geschwindigkeit |
| **PWM** | Motorstärke −255 bis 255 · positiv = vorwärts · negativ = rückwärts · 0 = stop |

### Beispiele (je ein „Übernehmen"-Button)

| # | Ziel | Input | Action |
|---|------|-------|--------|
| 1 | Motor 0 vorwärts halten | DPAD UP · hold | motor_direct · Motor 0 · PWM 200 |
| 2 | LED-Farbe setzen | BTN_A · press | led_set · Start 0 · Anzahl 5 · #FF0000 |
| 3 | Joystick steuert Fahrwerk | axis_pair · RX/RY | drive_pair · Ziel gui |

Klick auf „Übernehmen" → Binding wird direkt in die Liste eingefügt (noch nicht gespeichert).

---

## 2. Controller-View

### Layout (PS3-typisch, HTML/CSS, kein SVG)

```
      [L1]                        [R1]
    [L2]                            [R2]

   ↑         [SEL]  [PS]  [STA]          △
 ←   →                             □    ○
   ↓                                   ×

  [L3 ●]                          [R3 ●]
```

- Jedes anklickbare Element ist ein `<button>` mit `data-input-type` und `data-input-key`
- DPAD: 4 Richtungs-Buttons (UP/DOWN/LEFT/RIGHT)
- Face Buttons: △=BTN_Y, □=BTN_X, ○=BTN_B, ×=BTN_A
- Shoulder: L1, R1, L2, R2
- Sticks: L3 (BUTTON_STICK_L, axis X/Y), R3 (BUTTON_STICK_R, axis RX/RY)
- SELECT/START/PS: optional dargestellt, aber vorerst keine Bindung (nicht im Firmware-Mapping)

### Farb-Feedback
- **Grau** = kein Binding
- **Blau** = 1 Binding
- **Orange** = 2+ Bindings
- **Aktiv/selected** = grüner Rahmen (das aktuell im Panel angezeigte Element)

### Stick-Besonderheit
Klick auf L3/R3-Bereich öffnet Sub-Auswahl:
- „Stick-Klick (L3/R3)" → button-Binding
- „Achsen (X/Y bzw. RX/RY)" → axis_pair-Binding

---

## 3. Binding-Panel (rechts neben Controller-View)

Erscheint wenn ein Controller-Element angeklickt wird.

**Header:** Name des Elements (z.B. „DPAD LEFT")

**Bindings-Liste für dieses Element:**
- Jedes Binding zeigt: Edge-Selector + Action-Editor (bestehende `makeActionEditor`-Funktion)
- Hilfstext unter PWM-Feld: „−255 bis 255 · negativ = rückwärts"
- Hilfstext unter motor-Index: „0–3 entspricht Motor A–D"
- [× Entfernen]-Button pro Binding

**Footer:**
- [+ Binding hinzufügen] → fügt neues Binding für dieses Element ein
- [💾 Alle speichern] → speichert alle Bindings (identisch zu bisherigem Save)

---

## 4. Binding-Liste (unten)

Bleibt als kompakte Gesamtübersicht erhalten. Jedes Binding zeigt:
- Input-Zusammenfassung (z.B. „DPAD LEFT · hold")
- Action-Zusammenfassung (z.B. „motor_direct Motor 0 PWM −200")
- [Bearbeiten]-Button → scrollt zur Controller-View und selektiert das Element
- [× Entfernen]

Toolbar: [+ Neu] [Laden] [Speichern] [Standards] — wie bisher

---

## Technische Architektur

### Dateien
- `data/controls.html` — Struktur: Info-Banner, Controller-View, Panel, Liste
- `data/controls.js` — Logik (alles in einem IIFE, kein Modul-System)

### Datenmodell
Bindings bleiben identisch zum bestehenden JSON-Format:
```json
{ "input": { "type": "dpad", "dir": "LEFT", "edge": "hold" },
  "action": { "type": "motor_direct", "motor": 0, "pwm": -200 } }
```
Keine Firmware-Änderung nötig.

### Funktions-Übersicht (JS)

| Funktion | Zweck |
|----------|-------|
| `renderControllerView()` | Zeichnet Controller-Layout, setzt Farben |
| `selectControllerElement(key)` | Markiert Element, lädt Panel |
| `renderPanel(inputKey)` | Zeigt Bindings für gewähltes Element |
| `makeActionEditor(action)` | Bestehend, wiederverwendet + Hilfstexte ergänzt |
| `collectAllBindings()` | Sammelt alle Bindings aus Panel + Liste |
| `save()` | POST an `/control_bindings` |
| `load()` | GET `/getBindings`, aktualisiert Controller-Farben |
| `applyExample(n)` | Fügt Beispiel-Binding ein |

### Keine externen Abhängigkeiten
- Kein CDN, kein npm, keine Fonts
- Nur bestehendes `/styles.css`
- Controller-Layout: pure CSS mit `flexbox` und `grid`

---

## Was sich NICHT ändert
- Firmware (`InputBindingManager.cpp`) — keine Änderung
- JSON-Format der Bindings — keine Änderung  
- Andere Seiten (`/`, `/config`, `/setup`) — keine Änderung
- WebSocket-Kommunikation — keine Änderung

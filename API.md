# 📡 TinkerThinker API-Referenz (HTTP, WebSocket & mDNS)

[⬅️ Zurück zur Hauptseite (`README.md`)](README.md)

Diese Dokumentation beschreibt alle HTTP- und WebSocket-Schnittstellen der TinkerThinker-Firmware, mit denen du eigene Skripte (Python, JavaScript, Node-RED) oder eigene Apps direkt mit dem Roboter verbinden kannst.

---

## 🌐 Verbindungsdaten & Basis-URLs

- **Standard-IP im Hotspot-Modus (AP)**: `http://192.168.4.1`
- **mDNS-Hostname (im Hotspot- und Heimnetz-Modus)**: `http://tinker.local`
- **WebSocket-Endpunkt**: `ws://192.168.4.1/ws` bzw. `ws://tinker.local/ws`
- **Authentifizierung**: keine
- **Content-Type**:
  - `POST /config`: `multipart/form-data` oder `application/x-www-form-urlencoded`
  - `POST /arm/config` & `POST /control_bindings`: `application/json`

---

## ⚡ Schnellstart (WebSocket `/ws`)

1. Mit dem WLAN des Roboters (`TinkerThinkerAP`) verbinden.
2. WebSocket-Verbindung zu `ws://192.168.4.1/ws` öffnen.
3. Befehle für Motoren, Servos oder LEDs als JSON-Nachricht senden.
4. Der Roboter sendet regelmäßig Live-Telemetrie (Akkustand, Motorströme, PWM-Werte, aktive Gamepad-Tasten) als JSON zurück.

---

## 📤 Client -> Roboter (WebSocket-Befehle)

### 1) Fahren per virtuellem Joystick
```json
{ "x": 0.4, "y": -0.2 }
```
- Wertebereich: `-1.0` bis `1.0`
- Wird intern über den konfigurierten Drive-Mixer auf das gewählte Fahrpaar (`motor_left_gui` / `motor_right_gui`) umgerechnet.

### 2) Einzelmotor oder Greifer steuern
```json
{ "motorA": "forward" }
{ "motorA": "backward" }
{ "motorA": "stop" }
{ "motorA": "120" }
{ "gripper": "open" }
{ "gripper": "close" }
{ "gripper": "stop" }
```
- Gültige Einzelmotor-Keys: `motorA`, `motorB`, `motorC`, `motorD`
- `gripper`: steuert direkt den konfigurierten Greifer-Motor (`motor_gripper_gui`, `open` = vorwärts, `close` = rückwärts, `stop` = 0)
- Zahlen-Strings (z. B. `"120"` oder `"-180"`) setzen den PWM-Wert im Bereich `-255..255` unter Berücksichtigung von `motor_invert` und `motor_deadband`.

### 3) Raw-Motor (Direktes PWM für Setup, Kalibrierung & Roboter-Arm)
```json
{ "motor_raw": { "motor": 0, "pwm": 140 } }
```
- `motor`: `0..3` (`0` = Motor A, `1` = Motor B, `2` = Motor C, `3` = Motor D)
- `pwm`: `-255..255` (`0` = Stopp)
- Steuert den Motorkanal direkt an (berücksichtigt die in der Firmware hinterlegte Hardware-Invertierung `motor_invert`).

### 4) Servo-Winkel setzen
```json
{ "servo0": 90 }
{ "servo1": 45, "servo2": 120 }
```
Oder im strukturierten Objekt-Format (genutzt vom Roboter-Arm & Setup-Assistenten):
```json
{ "servo": { "channel": 0, "angle": 35 } }
```
- Kanäle: `0..6` (`servo0..servo6` auf v2-Boards) bzw. `0..2` (`servo0..servo2` auf v1-Legacy-Boards)
- Winkel: `0..180` Grad

### 5) WS2812 RGB-LEDs setzen
```json
{ "led_set": { "start": 0, "count": 8, "color": "#00FF00" } }
```
- Setzt die Farbe im Bereich `start .. start + count - 1` (`start: 0, count: 1` ist die Onboard-Status-LED).

### 6) Motor-Fahrpaar tauschen (`A/B` <-> `C/D`)
```json
{ "swap": true }
```
- Tauscht das aktive Fahrpaar und speichert die Änderung in `/config.json`.

---

## 📥 Roboter -> Client (WebSocket Live-Telemetrie)

Der ESP32 sendet zyklisch ein Status-JSON an alle verbundenen WebSocket-Clients:

```json
{
  "batteryVoltage": 7.92,
  "batteryPercentage": 63.4,
  "wifiRssi": -54,
  "servos": [90, 90, 90, 90, 90, 90, 90],
  "motorPWMs": [0, 0, 0, 0],
  "motorCurrents": [0.12, 0.10],
  "firstLED": { "r": 0, "g": 255, "b": 0 }
}
```

- **`motorCurrents`**: Array mit 2 Messwerten in Ampere `[Sensor 1 (GPIO 34 / Motor B), Sensor 2 (GPIO 36 / Motor A)]`.
- **`firstLED`**: Aktuelle RGB-Farbe der ersten WS2812-LED (`LED 0`), damit alle verbundenen Browser ihren Farbwähler synchronisieren können.
- Auf alten v1-Platinen (`legacy_board: true`) enthält das JSON zusätzlich `"motorFaults": false|true` (Status von `GPIO_FAULT_1` auf GPIO 39) und `servos` umfasst 3 Einträge.

---

## 🔗 HTTP-Endpunkte

### Weboberflächen (`GET`)
- **`GET /`**: Liefert je nach eingestelltem `gui_mode` automatisch `basic.html` (`basic`), `index.html` (`drive`) oder `arm.html` (`arm`).
- **`GET /basic`**, **`GET /drive`**, **`GET /arm`**:Direkter Aufruf der jeweiligen Steuerungsoberfläche.
- **`GET /setup`**: Geführter Einrichtungs-Assistent (inkl. nummeriertem Roboter-Arm-Assistenten ①-⑤).
- **`GET /controls`**: Visueller Bluetooth-Gamepad- & Hardware-Editor.
- **`GET /config`**: Experten-Konfiguration & Projekt-/GitHub-Übersicht.
- **`GET /portal.html`**: Smartphone-Verbindungs-Assistent (Captive Portal mit Apple/Android-Erkennung und Countdown).

### Konfiguration lesen & schreiben
- **`GET /getConfig`**:
  Liefert die gesamte aktive Konfiguration als JSON (inkl. `gui_mode`, `legacy_board`, `servo_count`, WLAN-Einstellungen, `motor_invert[]`, `motor_deadband[]`, `motor_frequency[]`, `arm_config` und `control_bindings`).
- **`POST /config`**:
  Speichert alle Konfigurationsfelder (inkl. optionalem JSON-Feld `arm_config`) dauerhaft in `/config.json` auf LittleFS.
- **`POST /arm/config`**:
  Speichert die Roboter-Arm-Konfiguration (`base`, `arm1`, `arm2`, `arm3`, `gripper` mit Motorzuordnung, Stromsensor-Auswahl `sensorMode`, Endanschlag `homeEnd`, `stallLimit`, `time90` und Servo-Winkeln) direkt als JSON in `/config.json`.
  - Antwort: `200 {"status":"ok"}`
- **`GET /setGuiMode?mode=basic|drive|arm`**:
  Schaltet die Standard-Steuerungsoberfläche um und speichert sie in `/config.json`.
- **`GET /setLegacyBoard?enabled=0|1`**:
  Schaltet den v1-Legacy-Board-Modus (`legacy_board`) zur Laufzeit um.
- **`GET /getBindings`** & **`POST /control_bindings`**:
  Liest bzw. speichert die Bluetooth-Controller-Belegungen als JSON-Array.
- **`GET /reboot`**: Startet den ESP32 neu.
- **`GET /resetconfig`**: Setzt `/config.json` auf Werkseinstellungen zurück und startet neu.

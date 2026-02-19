# TinkerThinker API (Roboter steuern)

Diese API-Doku beschreibt die aktuellen HTTP- und WebSocket-Schnittstellen der Firmware.

## Basis

- AP-Standard-IP: `http://192.168.4.1`
- WebSocket: `ws://<device-ip>/ws`
- Authentifizierung: aktuell keine
- Content-Type:
  - Konfiguration: `multipart/form-data` (wie aus dem Web-UI) oder URL-encoded Formfelder
  - Bindings API: `application/json`

## Schnellstart

1. Mit dem WLAN des Roboters verbinden.
2. WebSocket zu `/ws` aufbauen.
3. Fahr-/Motor-/Servo-/LED-Befehle als JSON senden.
4. Status kommt als JSON über denselben WebSocket zurück.

## WebSocket API (`/ws`)

## Client -> Roboter (Befehle)

### 1) Fahren per Joystick

```json
{ "x": 0.4, "y": -0.2 }
```

- Wertebereich typischerweise `-1.0 .. 1.0`
- Intern auf Motorantrieb gemappt (mit Arbitration gegenüber Bluetooth)

### 2) Einzelmotor steuern

```json
{ "motorA": "forward" }
{ "motorA": "backward" }
{ "motorA": "stop" }
{ "motorA": "120" }
```

- Gültige Keys: `motorA`, `motorB`, `motorC`, `motorD`
- String-PWM wird als Integer interpretiert und auf `-255..255` begrenzt

### 3) Raw-Motor (Setup/Kalibrierung)

```json
{ "motor_raw": { "motor": 0, "pwm": 80 } }
```

- `motor`: `0..3`
- `pwm`: `-255..255`
- Umgeht die normale Deadband-Logik

### 4) Servo setzen

```json
{ "servo0": 90 }
{ "servo1": 45, "servo2": 120 }
```

- `servo0..servo2`, Winkel `0..180` (intern begrenzt)

### 5) LEDs setzen

```json
{ "led_set": { "start": 0, "count": 8, "color": "#00FF00" } }
```

- Setzt Bereich `start .. start+count-1`

### 6) Motor-Swap-Flag ändern

```json
{ "swap": true }
```

- Persistiert direkt in `config.json`

## Roboter -> Client (Status)

Typische Statusnachricht:

```json
{
  "batteryVoltage": 7.92,
  "batteryPercentage": 63.4,
  "servos": [90, 90, 90],
  "motorPWMs": [0, 0, 0, 0],
  "motorCurrents": [0.12, 0.10],
  "firstLED": { "r": 0, "g": 255, "b": 0 }
}
```

Zusätzlich bei WLAN-relevanter Config-Änderung:

```json
{ "restart": true }
```

## HTTP API

### `GET /getConfig`

Liefert die komplette aktive Konfiguration als JSON, u. a.:

- WLAN: `wifi_mode`, `wifi_ssid`, `hotspot_ssid`, ...
- Motoren: `motor_invert[]`, `motor_deadband[]`, `motor_frequency[]`
- Fahrpaar: `motor_left_gui`, `motor_right_gui`
- Servo: `servo_settings[]`
- LEDs: `led_count`
- BT/Wi-Fi Scan-Timings
- `control_bindings`

### `POST /config`

Speichert Konfiguration aus Formfeldern und wendet sie an.

Wichtige Felder:

- WLAN: `wifi_mode`, `wifi_ssid`, `wifi_password`, `hotspot_ssid`, `hotspot_password`
- Motor GUI-Paar: `motor_left_gui`, `motor_right_gui`
- Motoren: `motor_invert_0..3`, `motor_deadband_0..3`, `motor_frequency_0..3`
- Servo: `servo0_min/max`, `servo1_min/max`, `servo2_min/max`
- LEDs: `led_count`
- Fahrprofil: `drive_mixer`, `drive_turn_gain`, `drive_axis_deadband`
- Motorkurve: `motor_curve_type`, `motor_curve_strength`
- BT/Wi-Fi: `bt_scan_on_normal_ms`, `bt_scan_off_normal_ms`, `bt_scan_on_sta_ms`, `bt_scan_off_sta_ms`, `bt_scan_on_ap_ms`, `bt_scan_off_ap_ms`

Antwort: Redirect auf `/config`.

### `POST /wifi/disable`

Deaktiviert WLAN bis zum Neustart.

- Erfolg: `200 {"status":"ok"}`
- Bereits deaktiviert: `409 {"error":"already_disabled"}`

### `GET /reboot`

Startet den ESP neu.

### `GET /resetconfig`

Setzt Config auf Defaults und rebootet.

### `GET /getBindings`

Liefert `control_bindings` als JSON-Array.

### `POST /control_bindings`

Speichert komplette Bindings-Liste als JSON-Array.

## Beispiele

### Per `curl` Fahrpaar auf A/B setzen

```bash
curl -X POST http://192.168.4.1/config \
  -F wifi_mode=AP \
  -F hotspot_ssid=TinkerThinkerAP \
  -F hotspot_password= \
  -F motor_left_gui=0 \
  -F motor_right_gui=1
```

### Per `wscat` Motor A vor/zurück/stop

```bash
wscat -c ws://192.168.4.1/ws
> {"motorA":"forward"}
> {"motorA":"backward"}
> {"motorA":"stop"}
```

## Hinweise

- Steuer-Arbitration ist aktiv: letzte nicht-neutrale Quelle gewinnt (WebSocket vs. Bluetooth).
- Für stabile Steuerung regelmäßig senden (z. B. 20-50 Hz für Joystick).
- Bei Änderungen an HTML/JS immer LittleFS neu hochladen.

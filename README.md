# TinkerThinkerBL

ESP32-based robotics controller integrating Bluepad32 gamepad input, DC motor control, servos, WS2812 LEDs, Wi‑Fi web UI (AP/STA), and basic battery/system monitoring — built on ESP-IDF with Arduino component.

This repository contains a ready-to-flash firmware and a small web app served from the ESP32 (LittleFS) for configuring and driving your board.

Key components used: Arduino-ESP32, Bluepad32, BTStack, ESPAsyncWebServer, ArduinoJson, FastLED, ESP32Servo.

Note: This project is set up for PlatformIO (recommended). ESP-IDF CLI/IDE can work, but is not the primary path here.

## Inhaltsverzeichnis
- [Features](#features)
- [Hardware Overview (default pins)](#hardware-overview-default-pins)
- [Project Layout](#project-layout)
- [Quick Start (PlatformIO)](#quick-start-platformio)
- [First Run and Web UI](#first-run-and-web-ui)
- [Seitenübersicht & Bedienung](#seitenübersicht--bedienung)
  - [Steuerung (`/`)](#steuerung-)
  - [Konfiguration (`/config`)](#konfiguration-config)
  - [Steuerungs-Editor (`/controls`)](#steuerungs-editor-controls)
  - [Setup-Assistent (`/setup`)](#setup-assistent-setup)
- [Controls-Editor Legende](#controls-editor-legende)
- [BT/Wi‑Fi Coexistence (Scan Timing)](#btwi-fi-coexistence-scan-timing)
- [Control Arbitration (Last‑Writer‑Wins)](#control-arbitration-lastwriterwins)
- [Windows CLI Build Notes](#windows-cli-build-notes)
- [Releases & CI](#releases--ci)
- [Flash From Release Assets](#flash-from-release-assets)
- [Arduino Core Note](#arduino-core-note)
- [Gamepad Controls (Bluepad32)](#gamepad-controls-bluepad32)
- [Configuration Reference](#configuration-reference)
- [Troubleshooting](#troubleshooting)
- [Development Notes](#development-notes)
- [License](#license)
- [Acknowledgements](#acknowledgements)
- [Further info](#further-info)
- [Support](#support)

## Features

- Gamepad control: Bluepad32 with automatic pairing and multiple controller types supported.
- 4 DC motors: Differential drive via joysticks; direct per-motor control APIs; invert, deadband, and swap options.
- 3 servos: Adjustable pulse width range per servo; angle control from gamepad and web UI.
- WS2812 LEDs: Configurable LED count; simple color feedback and status.
- Web UI (LittleFS): Live dashboard, motor/servo testing, and configuration at `/` and `/config`.
- Controls mapping UI: Konfigurierbarer Steuerungs‑Editor unter `/controls` (formularbasiert).
- Wi‑Fi AP/STA: Runs as hotspot or joins existing Wi‑Fi; live WebSocket updates.
- Monitoring: Battery voltage/percentage estimation and H-bridge fault/current sampling.
- Fahrprofile: Arcade/Tank-Mix mit Lenkfaktor und optionaler Expo-Motorkurve für feinfühlige Steuerung.

- BT/Wi‑Fi coexistence: Configurable Bluetooth scanning duty‑cycle to avoid starving Wi‑Fi.
- MODE button (GPIO39, active‑low): cycles radio modes (Normal → Wi‑Fi only → BT scan only → Wi‑Fi only).
- Startup factory reset: Hold MODE during boot for 10s to reset config and reboot; LED blinks red/white/orange. Release early to cancel.
- Control arbiter: Last active source (WebSocket vs. Bluetooth) owns motion; neutral inputs don't override the owner.
- Flexible driving: Right stick controls the configured GUI motor pair; left stick controls the other pair. Motor pair selection is configurable in `/config`.
- Quick servo control: D‑Pad LEFT/RIGHT nudges Servo 0 by ±10°. R2 toggles Servo 0 between 0°↔90°, L2 zwischen 90°↔180°.
- Speed scaling: R1/L1 adjust a global speed multiplier (0.2–1.5) that scales motor PWM for finer control.

## Hardware Overview (default pins)

- Motors (H-bridge channels):
  - M0: `pin1=16`, `pin2=25` (channels 0,1)
  - M1: `pin1=32`, `pin2=27` (channels 2,3)
  - M2: `pin1=4`, `pin2=12` (channels 4,5)
  - M3: `pin1=15`, `pin2=14` (channels 6,7)
- Servos:
  - S0: `pin=13`, channel 8
  - S1: `pin=33`, channel 9
  - S2: `pin=17`, channel 10
- LEDs: WS2812 data `pin=2` (count configurable)
- Battery ADC: `pin=35`
- MODE button: `pin=39` (active‑low, GND on press) **BREAKING change**
- H-bridge current sense: `pin=34`, `pin=36`

Adjust these in `main/TinkerThinkerBoard.cpp` or via runtime settings where available.

## Project Layout

- `main/`
  - `sketch.cpp`: Arduino loop with Bluepad32 input handling and high-level behavior.
  - `main.c`: Bluepad32/BTstack platform glue (IDF entrypoint when not autostarting Arduino).
  - `TinkerThinkerBoard.*`: Facade composing controllers and exposing simple control APIs.
  - `MotorController.*`, `ServoController.*`, `LEDController.*`: Device control modules.
  - `BatteryMonitor.*`, `SystemMonitor.*`: Voltage, fault, and current sampling utilities.
  - `WebServerManager.*`: Async web server, static files, WebSocket control, config routes.
- `data/`: Web UI (served from LittleFS) — `index.html`, `config.html`, JS/CSS, and `config.json`.
- `platformio.ini`: Build environments (ESP32 family) and partitioning.
 - `PCB/`: Schaltplan/Ansichten (PDF) und 3D‑Objekt der Platine (siehe PCB/README.md).
 - `CAD/`: Fusion‑360‑Modelle (Baugruppe/Teile) für Gehäuse/Mechanik (siehe CAD/README.md).

## Quick Start (PlatformIO)

1) Open in VS Code with the PlatformIO extension.

2) Select an environment:
   - Default example: `env:esp32dev` (8MB flash; custom sdkconfig)
   - Others available: `esp32-s3-devkitc-1`, `esp32-c3-devkitc-02`, `esp32-c6-devkitc-1`, `esp32-h2-devkitm-1`

3) Ensure LittleFS is used for the web assets image:

   Add the following to `platformio.ini` (if not already present):

   ```ini
   board_build.filesystem = littlefs
   ```

4) Build and flash the firmware:
   - PlatformIO: Build, then Upload (or “Upload and Monitor”).

5) Upload the web UI (LittleFS) image:
   - PlatformIO: “Upload Filesystem Image”.
   - This places the contents of `data/` onto the device. Without this step, the web pages will 404.

6) Open the serial monitor at `115200` baud to watch logs.

Notes
- The project pins an Espressif32 platform release in `platformio.ini` for stability.
- Partition scheme: `partitions_dual3mb_1m5spiffs.csv` (1.5MB FS, dual app slots).

## First Run and Web UI

Wi‑Fi defaults (change later in the config UI):

- Mode: AP
- SSID: `TinkerThinkerAP`
- Password: empty (set your own in `/config`)

Connect to the AP, then browse to `http://192.168.4.1/`:

- `/` Dashboard: Live‑Status (Batterie, Motoren, LEDs) und Grundfunktionen.
- `/config`: Vollständige Konfiguration (Motor invert/deadband/frequency, LED‑Anzahl, Wi‑Fi, OTA, Servo‑Pulse). Änderungen persistieren in LittleFS (`/config.json`).
- `/controls`: Steuerungs‑Editor für „control_bindings“ (Buttons/D‑Pad/Sticks → Aktionen wie Antriebspaare, Servo‑Winkel, LED‑Farben, GPIO, Speed‑Skalierung).
- `/setup`: Setup‑Assistent für die Motor‑Zuordnung und Deadband‑Ermittlung.

The UI communicates via WebSocket for low-latency updates and test actions.

## BT/Wi‑Fi Coexistence (Scan Timing)

The ESP32 shares a single 2.4 GHz radio between Wi‑Fi and Bluetooth. Continuous BT inquiry/page can hinder Wi‑Fi connect/AP traffic. This firmware time‑slices BT scanning so Wi‑Fi always gets airtime.

Tune these in `/config` (Advanced section):

- `bt_scan_on_normal_ms` / `bt_scan_off_normal_ms`: Balanced scanning when Wi‑Fi is idle.
- `bt_scan_on_sta_ms` / `bt_scan_off_sta_ms`: Used while STA is connecting (no IP yet) — favors Wi‑Fi association/DHCP.
- `bt_scan_on_ap_ms` / `bt_scan_off_ap_ms`: Used when AP has clients — keeps scanning minimal.

If a controller is already connected, scanning is suspended.

## Control Arbitration (Last‑Writer‑Wins)

Both a Bluetooth controller and the Web UI can drive the motors. To avoid conflicts:

- The last non‑neutral command takes ownership (Bluetooth or WebSocket) and drives the board.
- Neutral commands (sticks centered or PWM 0) only apply if they come from the current owner.
- Prevents a neutral message from one source cancelling motion commanded by the other.

Implementation lives in `TinkerThinkerBoard` via source‑aware methods like `requestDriveFromBT/WS()`.

## Seitenübersicht & Bedienung

### Steuerung (`/`)
- Live‑Status der Hardware und einfache Tests (z. B. Servo‑Slider, Motor‑Buttons).
- Nützlich für schnelle Funktionsprüfungen nach dem Flashen.

### Konfiguration (`/config`)
- Motoren: Invertierung, Deadband (mit Slider + Zahl und direktem Deadband‑Test), PWM‑Frequenz.
- Servos: Min/Max‑Pulse (Slider + Zahl) mit Test‑Buttons 0°/90°/180°.
- LEDs: Anzahl (Slider + Zahl).
- Wi‑Fi: AP/STA‑Modus, SSID/Passwörter; Neustart‑Hinweis bei Änderung.
- Erweiterte BT‑Scan‑Timings.
- Tipp: Nach UI‑Änderungen „Speichern“, die Werte werden in `/config.json` abgelegt.
- Querverweis: Für die Tasten‑ und Stick‑Belegung siehe [Steuerungs‑Editor](/controls).

### Steuerungs-Editor (`/controls`)
- Visueller Editor, um beliebige Eingaben (Buttons, D‑Pad, Sticks) auf Aktionen zu mappen.
- Bindings laden/speichern; Änderungen greifen ohne Neustart (Bindings werden zyklisch neu geladen).
- Querverweis: Die Actions nutzen die bestehenden `TinkerThinkerBoard`‑APIs und respektieren die [Control Arbitration](#control-arbitration-lastwriterwins).

### Setup-Assistent (`/setup`)
- Geführte Ersteinrichtung für Motor‑Zuordnung, Drehrichtung und Deadband‑Ermittlung.
- Querverweis: Feinabstimmung später jederzeit unter [/config](/config).

## Controls-Editor Legende

Eingaben (Input):
- Typ „button“:
  - Codes: `BTN_A`, `BTN_B`, `BTN_X`, `BTN_Y`, `BUTTON_L1`, `BUTTON_R1`, `BUTTON_L2`, `BUTTON_R2`, `BUTTON_STICK_L`, `BUTTON_STICK_R`.
  - Edge: `press` (Kante), `release`, `hold`.
- Typ „dpad“:
  - Richtung: `UP`, `DOWN`, `LEFT`, `RIGHT`.
  - Edge: `press`, `release`, `hold`.
- Typ „axis_pair“:
  - Achsen: `X`, `Y`, `RX`, `RY` (Left/Right Stick, X/Y‑Achsen).
  - Parameter: `deadband` (z. B. 16).

Aktionen (Action):
- `drive_pair`:
  - Parameter: `target` = `gui` (konfiguriertes Paar) oder `other` (übriges Paar).
- `servo_set`:
  - Parameter: `servo` (0..2), `angle` (0..180).
- `servo_toggle_band`:
  - Parameter: `servo` (0..2), `bands` (z. B. `[0,90]` oder `[90,180]`).
- `servo_nudge`:
  - Parameter: `servo` (0..2), `delta` (z. B. `+10`/`-10`).
- `servo_axes`:
  - Parameter: `servo` (0..2), `scale` (z. B. 1.0). Interpretiert Achsenwert als Winkel um 90° (neutral).
- `led_set`:
  - Parameter: `start`, `count`, `color` (Hex `#RRGGBB`).
- `gpio_set`:
  - Parameter: `pin`, `level` (`0/1`).
- `speed_adjust`:
  - Parameter: `delta` (z. B. `+0.1`/`-0.1`). Skaliert PWM‑Ausgabe global.

Hinweise:
- Achsen‑Aktionen (`axis_pair`) senden kontinuierlich; Button/D‑Pad sind ereignisgesteuert (edge/hold).
- Fahrbefehle laufen über die `requestDrive*`‑Methoden und unterliegen der [Last‑Writer‑Wins‑Arbitration](#control-arbitration-lastwriterwins).
- Bindings liegen in `/config.json` unter `control_bindings` (werden per UI geladen/gespeichert).

## Windows CLI Build Notes

If you prefer CLI over the VS Code tasks, use PowerShell and the PlatformIO penv path:

- Build: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev`
- Upload: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t upload`
- Upload FS: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t uploadfs`

## Releases & CI

- On every successful push to `main`, GitHub Actions builds the firmware and LittleFS image and creates a Release with a tag like `main-YYYYMMDD-HHMM-<shortsha>`.
- Attached artifacts:
  - `bootloader.bin` (offset 0x1000)
  - `partitions.bin` (offset 0x8000)
  - `firmware.bin` (app at 0x20000 for the default OTA layout)
  - `littlefs.bin` (filesystem image for the `spiffs` partition)
- The Release body summarizes board, platform, flash size, partition CSV, computed offsets, and artifact sizes.

Partition reference (default `partitions_dual3mb_1m5spiffs.csv` on 8MB flash):
- `ota_0`: 0x20000 size 0x300000
- `ota_1`: 0x320000 size 0x300000
- `spiffs` (used with LittleFS): 0x620000 size 0x180000

## Flash From Release Assets

Use the auto‑flasher to download the latest Release and flash with the correct offsets computed from `platformio.ini` + partition CSV.

Prerequisites:
- Python 3
- `pip install esptool requests tqdm pyserial`

Steps:
1) `python tools/auto_flasher.py`
2) The script downloads the latest artifacts, detects CH340/CP210x serial adapters, erases flash, then writes:
   - 0x1000 → bootloader.bin
   - 0x8000 → partitions.bin
   - app offset from CSV (default 0x20000) → firmware.bin
   - fs offset from CSV (default 0x620000) → littlefs.bin
3) The script refuses to flash if `littlefs.bin` exceeds the FS partition size and keeps a `blacklist.txt` of flashed MACs.

## Arduino Core Note

This project uses Arduino as an ESP‑IDF component (managed via `managed_components/espressif__arduino-esp32`).

- `main/CMakeLists.txt` lists `espressif__arduino-esp32` in `REQUIRES`.
- Do not add a second `components/arduino` — it will cause duplicate symbols and link failures.

## Gamepad Controls (Bluepad32)

- Pairing: Put your controller in pairing mode; device toggles new connections on when no controller is connected. Keys are cleared on each boot (`BP32.forgetBluetoothKeys()`), so pairing is straightforward.
- Driving: Right stick → GUI motor pair; Left stick → the other motor pair. Change the GUI pair in `/config` via `motor_left_gui` and `motor_right_gui`.
- Servo: D‑Pad LEFT/RIGHT nudges Servo 0 by −/+10°. R2 toggles 0°↔90°, L2 toggles 90°↔180° for Servo 0.
- Speed: R1 increases and L1 decreases the global speed multiplier used to scale PWM output.
- Buttons: A/B/X/Y set LED feedback colors in the default demo.
- Battery indication: Player LEDs reflect voltage bands.

Tip: Consult Bluepad32 docs for controller-specific pairing steps and supported models.

## Configuration Reference

- Motors
  - Invert per motor, global side swap (left/right), per‑motor deadband and PWM frequency.
  - Drive profile: `drive_mixer` (`arcade`/`tank`), `drive_turn_gain` (0.0–2.5) und `drive_axis_deadband` (0–256) regeln den Mix.
  - Motor curve: `motor_curve_type` (`linear` oder `expo`) samt `motor_curve_strength` (−0.8…3.0) formt die PWM-Kennlinie.
- Servos
  - Min/max pulse width per servo; angles clamped 0–180°.
- LEDs
  - Count for WS2812 strip; color rendering via FastLED.
- Wi‑Fi
  - AP or STA; SSID/password for each mode; reboot prompt on Wi‑Fi changes.
- OTA
  - Flag present in UI/config (integration point for future OTA flow).

All settings persist in `/config.json` on LittleFS. Use “Reset to defaults” in the UI to regenerate.
You can also reset on boot by holding the MODE button for 10 seconds; it will blink red/white/orange while waiting and reboot after reset.

## Troubleshooting

- Web UI 404 or blank: Upload the filesystem image (LittleFS) after flashing firmware.
- No controller connects: Re‑enter pairing on the controller; watch serial logs; ensure Bluetooth is enabled and not blocked.
- Web UI flaky right after controller connect: Wi‑Fi is paused for ~3s to boost BT pairing.
- Motors run backward: Use per‑motor invert or side swap in `/config`.
- Jerky/slow movement: Adjust per‑motor deadband and frequency; verify power and H‑bridge wiring.

## Development Notes

- Code style: Arduino APIs inside an ESP‑IDF component; main loop in `sketch.cpp`.
- PWM: 8‑bit resolution used for motor channels; servo channels run at ~50 Hz.
- Current/fault sampling: Averaging over PWM cycles; see `SystemMonitor`.
  - Note: Fault pin removed; only current sampling remains.

## License

This project includes third‑party components with their respective licenses. See `LICENSE` for details (Apache‑2.0 for most core pieces; BTStack under its own license).

## Acknowledgements

- Built on top of Bluepad32, Arduino‑ESP32, BTStack, FastLED, ArduinoJson, ESPAsyncWebServer, and related IDF components.


## Further info

* [Bluepad32 for Arduino](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
* [Arduino as ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
* [ESP-IDF VSCode plugin](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/vscode-setup.html)

## Support

* [Discord][discord]: any question? Ask them on our Discord server.

[discord]: https://discord.gg/r5aMn6Cw5q

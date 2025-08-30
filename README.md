# TinkerThinkerBL

ESP32-based robotics controller integrating Bluepad32 gamepad input, DC motor control, servos, WS2812 LEDs, Wi‑Fi web UI (AP/STA), and basic battery/system monitoring — built on ESP-IDF with Arduino component.

This repository contains a ready-to-flash firmware and a small web app served from the ESP32 (LittleFS) for configuring and driving your board.

Key components used: Arduino-ESP32, Bluepad32, BTStack, ESPAsyncWebServer, ArduinoJson, FastLED, ESP32Servo.

Note: This project is set up for PlatformIO (recommended). ESP-IDF CLI/IDE can work, but is not the primary path here.

## Features

- Gamepad control: Bluepad32 with automatic pairing and multiple controller types supported.
- 4 DC motors: Differential drive via joysticks; direct per-motor control APIs; invert, deadband, and swap options.
- 3 servos: Adjustable pulse width range per servo; angle control from gamepad and web UI.
- WS2812 LEDs: Configurable LED count; simple color feedback and status.
- Web UI (LittleFS): Live dashboard, motor/servo testing, and configuration at `/` and `/config`.
- Wi‑Fi AP/STA: Runs as hotspot or joins existing Wi‑Fi; live WebSocket updates.
- Monitoring: Battery voltage/percentage estimation and H-bridge fault/current sampling.

- BT/Wi‑Fi coexistence: Configurable Bluetooth scanning duty‑cycle to avoid starving Wi‑Fi.
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
- H-bridge fault: `pin=39`
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

- `/` Dashboard: Live status (battery, motors, LEDs) and basic controls.
- `/config`: Full configuration form (motors invert/deadband/frequency, LED count, Wi‑Fi mode/credentials, OTA flag, servo pulse ranges). Changes persist in LittleFS (`/config.json`).
- `/setup`: Additional setup page (if used).

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

## Windows CLI Build Notes

If you prefer CLI over the VS Code tasks, use PowerShell and the PlatformIO penv path:

- Build: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev`
- Upload: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t upload`
- Upload FS: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t uploadfs`

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
- Servos
  - Min/max pulse width per servo; angles clamped 0–180°.
- LEDs
  - Count for WS2812 strip; color rendering via FastLED.
- Wi‑Fi
  - AP or STA; SSID/password for each mode; reboot prompt on Wi‑Fi changes.
- OTA
  - Flag present in UI/config (integration point for future OTA flow).

All settings persist in `/config.json` on LittleFS. Use “Reset to defaults” in the UI to regenerate.

## Troubleshooting

- Web UI 404 or blank: Upload the filesystem image (LittleFS) after flashing firmware.
- No controller connects: Re‑enter pairing on the controller; watch serial logs; ensure Bluetooth is enabled and not blocked.
- Motors run backward: Use per‑motor invert or side swap in `/config`.
- Jerky/slow movement: Adjust per‑motor deadband and frequency; verify power and H‑bridge wiring.

## Development Notes

- Code style: Arduino APIs inside an ESP‑IDF component; main loop in `sketch.cpp`.
- PWM: 8‑bit resolution used for motor channels; servo channels run at ~50 Hz.
- Current/fault sampling: Averaging over PWM cycles; see `SystemMonitor`.

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

Purpose
This file briefs future AI/code assistants on key project specifics, build/run details, and recent architectural decisions so you can work efficiently without re‑discovering context.

Project Summary
- Firmware for ESP32 robotics board combining:
  - Bluepad32 gamepad control (BTStack)
  - 4 DC motors, 3 servos, WS2812 LEDs
  - Wi‑Fi AP/STA + Async Web UI over LittleFS
  - Arduino‑ESP32 as an ESP‑IDF component

Build Tooling
- Primary: PlatformIO (VS Code or CLI)
- Windows PowerShell CLI examples:
  - Build: & "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev
  - Upload: & "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t upload
  - Upload FS: & "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t uploadfs
- Serial monitor: 115200 baud

PlatformIO/IDF Notes
- We intentionally use the managed Arduino component at managed_components/espressif__arduino-esp32.
  - main/CMakeLists.txt lists espressif__arduino-esp32 in REQUIRES.
  - Do NOT add a second components/arduino; it produces duplicate symbols.
- Entry/loop split:
  - main.c (IDF entry, calls into Bluepad32 platform)
  - sketch.cpp (Arduino setup/loop, app logic)

Key Modules
- main/TinkerThinkerBoard.*: Central façade wrapping Motor/Servo/LED/Battery/System monitors and WebServerManager.
- components/bluepad32_arduino: Bridge between Bluepad32/BTStack and Arduino tasks.
- main/WebServerManager.*: Async web server, WebSocket, config endpoints.
- data/: LittleFS web app (index/config/setup pages + JS/CSS).

Recent Changes (Important)
1) BT/Wi‑Fi coexistence — duty‑cycled Bluetooth scanning
   - The ESP32 shares a single 2.4 GHz radio; continuous BT inquiry/page can starve Wi‑Fi.
   - Implementation: main/sketch.cpp toggles BP32.enableNewBluetoothConnections() via a simple scheduler.
   - Scenarios & defaults (configurable via ConfigManager and the UI):
     - Normal: 500/500 ms (on/off)
     - STA connecting: 150/850 ms
     - AP with clients: 100/1900 ms
     - Controller connected: scanning off
   - Config keys (persisted in /config.json):
     - bt_scan_on_normal_ms, bt_scan_off_normal_ms
     - bt_scan_on_sta_ms, bt_scan_off_sta_ms
     - bt_scan_on_ap_ms, bt_scan_off_ap_ms

2) Control arbitration — last‑writer‑wins
   - Both BT gamepad and WebSocket can send drive commands.
   - New API in TinkerThinkerBoard:
     - requestDriveFromBT/WS(int axisX, int axisY)
     - requestMotorDirectFromBT/WS(int motorIndex, int pwm)
     - requestMotorStopFromWS(int motorIndex)
   - Rules:
     - Non‑neutral command (axes beyond threshold or non‑zero PWM) takes ownership and applies immediately.
     - Neutral commands apply only if they come from the current owner.
     - Prevents one source’s neutral from cancelling the other’s active command.
   - Neutral threshold: 16 (see TinkerThinkerBoard::neutralThreshold).

3) Web UI additions
   - config.html: Added inputs for the six BT scan timing values.
   - config.js: Loads these values from /getConfig, included in POST /config via form submission.
   - WebServerManager: /getConfig and /config were updated to expose/accept these fields.

Editing Guidelines
- Keep changes focused; do not introduce a second Arduino core.
- Respect the control arbiter API; route all new control surfaces through TinkerThinkerBoard request* methods.
- When touching Wi‑Fi/BT behavior, prefer adjusting the duty‑cycle scheduler in sketch.cpp.
- UI fields: add in data/config.html and wire to data/config.js; expose via WebServerManager.

Common Tasks
- Adjust BT/Wi‑Fi timings: update defaults in ConfigManager or tweak at runtime via the config UI.
- Add a new control source: implement request methods in TinkerThinkerBoard and call them from the source.
- Expand config: extend ConfigManager load/save; expose in WebServerManager; add UI inputs.

Pitfalls
- Duplicate Arduino symbols: do NOT create components/arduino. Use managed_components/espressif__arduino-esp32 only.
- Web UI 404: remember to upload the LittleFS image.
- Windows shells: prefer PowerShell path shown above; plain `pio` may not be on PATH.

Contact/Context
- This repo is typically built in Windows with PlatformIO.
- Prior issues included double Arduino inclusion and BT starving Wi‑Fi during association; both addressed as above.


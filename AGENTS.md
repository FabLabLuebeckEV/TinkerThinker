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

CI/Workflow Notes (Important)
- Primary CI: .github/workflows/esp32-platformio-build-release.yml
  - Builds `env:esp32dev` via PlatformIO, creates LittleFS image, uploads artifacts.
  - On every successful push to `main`, creates a GitHub Release with tag `main-YYYYMMDD-HHMM-<shortsha>` and attaches bootloader/partitions/firmware/littlefs images.
  - Tag‑based releases are still supported and attach the same artifacts.
- Removed CI: .github/workflows/esp-idf-latest.yaml
  - Pure ESP‑IDF matrix build (esp32/s3/c3/c6/h2) removed to avoid duplication; PlatformIO CI is source of truth.
- Repo hygiene
  - `.pio/` is ignored via .gitignore; keep PlatformIO build artifacts out of Git.
  - `.github/FUNDING.yml` was removed to avoid “Sponsor this project” badge.

PlatformIO Config (Reference)
- Global defaults (in `[env]`):
  - `platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.21/platform-espressif32.zip`
  - `framework = espidf`
  - `monitor_speed = 115200`, `upload_speed = 921600`
  - `board_build.sdkconfig = sdkconfig.defaults`
  - `board_build.flash_size = 8MB`, `board_upload.flash_size = 8MB`
  - `board_build.partitions = partitions_dual3mb_1m5spiffs.csv`
- `env:esp32dev`:
  - `board = esp32dev`
  - `board_build.sdkconfig = sdkconfig.esp32dev`
  - `board_build.filesystem = littlefs`
  - Note: Keep vendor components (FastLED, Bluepad32, etc.) in `components/` as the project expects.

FastLED Notes
- FastLED headers live under `components/FastLED/src` (e.g., `fl/ui_impl.h`).
- Do not change FastLED to header‑only; it has `.cpp` implementations for IDF5 (RMT v5, RGBW utils, etc.).
- Build is driven by `components/FastLED/CMakeLists.txt` (kept as‑is from vendor).
- If CI reports missing `fl/ui_impl.h`, verify the vendor tree is complete (e.g., .gitignore didn’t exclude it) rather than adding CI‑only include hacks.

Do/Don’t Summary
- Do: Keep Arduino‑ESP32 as managed component; no duplicate `components/arduino`.
- Do: Use PlatformIO workflow; avoid separate ESP‑IDF CI unless necessary.
- Do: Keep `.pio/` out of Git.
- Don’t: Modify vendor trees (FastLED/Bluepad32) to “fix” includes; fix root cause (correct vendor files/paths).

Tools
- `tools/auto_flasher.py`:
  - Downloads latest release assets (bootloader.bin, partitions.bin, firmware.bin, littlefs.bin).
  - Reads `platformio.ini` to locate partitions CSV and flash size; parses CSV to compute correct flash offsets.
  - Flashes via `esptool` at:
    - bootloader: 0x1000
    - partitions: 0x8000
    - app: from `factory` or `ota_0` (current: 0x20000)
    - fs: from `spiffs`/`littlefs` (current: 0x620000, size 0x180000)
  - Verifies `littlefs.bin` fits the FS partition; supports auto‑scan of CH340/CP210x serial adapters.

Partition Table (Reference)
- File: `partitions_dual3mb_1m5spiffs.csv` (8MB flash)
- Layout:
  - `ota_0`: app at 0x20000 size 0x300000
  - `ota_1`: app at 0x320000 size 0x300000
  - `spiffs` (used with LittleFS): at 0x620000 size 0x180000

Release Artifacts
- bootloader.bin: ESP-IDF bootloader (offset 0x1000).
- partitions.bin: Partition table (offset 0x8000).
- firmware.bin: Main application (offset from `factory`/`ota_0`, current 0x20000).
- littlefs.bin: LittleFS image for `spiffs` partition (offset 0x620000, size 0x180000).

**How To Flash**
- Prereqs: Python 3; `pip install esptool requests tqdm pyserial`.
- Run: `python tools/auto_flasher.py`.
- Behavior: Downloads latest release assets, reads `platformio.ini` + partitions CSV to compute correct offsets, auto-detects CH340/CP210x ports, erases flash, and writes images.
- Safety: Verifies `littlefs.bin` fits the FS partition; uses `blacklist.txt` to avoid re-flashing the same MAC.
- Notes: Adjust erase behavior or port filtering by editing `tools/auto_flasher.py` if needed.


**Control Mapping UI Plan**
- Goal: Add a web UI to fully configure input→action mappings: map any gamepad button or stick direction/axis to actions such as driving specific motor pairs, setting servo positions or angles, changing LED colors, or toggling GPIO pins.

**Scope**
- Inputs: Gamepad buttons (A/B/X/Y, L1/L2/R1/R2, sticks pressed), D‑Pad directions, stick directions (up/down/left/right) and axes (X/Y, RX/RY) with thresholds.
- Actions: Motor drive (pair selection and mode), Servo set (fixed angle, band toggle, axis mapping), LED set color (single, range), GPIO set/toggle (HIGH/LOW), No‑op.
- Events: onPress, onRelease, onHold (repeat), continuous for axes with deadband and rate limits.

**Config Schema (config.json)**
- Add `control_bindings` array of objects, persisted by ConfigManager and editable via UI.
- Binding shape examples:
  - Button press → servo fixed angle:
    - input: { type: "button", code: "BUTTON_R2", edge: "press" }
    - action: { type: "servo_set", servo: 0, angle: 90 }
  - Button press → toggle servo band:
    - input: { type: "button", code: "BUTTON_L2", edge: "press" }
    - action: { type: "servo_toggle_band", servo: 0, bands: [0,90,180] }
  - D‑Pad RIGHT/LEFT → servo nudge:
    - input: { type: "dpad", dir: "RIGHT", edge: "press" }
    - action: { type: "servo_nudge", servo: 0, delta: +10 }
  - Right stick axes → drive GUI pair (continuous):
    - input: { type: "axis_pair", x: "RX", y: "RY", deadband: 16, scale: 1.0, rate_hz: 50 }
    - action: { type: "drive_pair", target: "gui" }
  - Left stick axes → drive other pair (continuous):
    - input: { type: "axis_pair", x: "X", y: "Y", deadband: 16, scale: 1.0, rate_hz: 50 }
    - action: { type: "drive_pair", target: "other" }
  - Stick direction → LED color:
    - input: { type: "stick_dir", axis: "RX", dir: "pos", edge: "press" }
    - action: { type: "led_set", start: 0, count: 10, color: "#00FF00" }
  - Button → GPIO:
    - input: { type: "button", code: "BTN_A", edge: "press" }
    - action: { type: "gpio_set", pin: 18, level: 1 }

**Backend Design**
- ConfigManager: extend to load/save `control_bindings` with validation and defaults; bump JSON doc capacity as needed.
- New `InputBindingManager` (main/InputBindingManager.*):
  - Maintains per‑controller previous states for edge detection.
  - Evaluates button/dpad events and axis values each loop.
  - Applies actions through TinkerThinkerBoard APIs, respecting last‑writer‑wins for drive commands.
  - Enforces deadband, clamps, and rate limiting (e.g., 50 Hz for continuous axis actions, min interval for LED updates).
- WebServerManager:
  - Extend `/getConfig` and `/config` to expose/accept `control_bindings`.
  - Add `/testAction` endpoint to execute a single action payload for UI previews (optional, safe‑guarded).
- sketch.cpp:
  - Replace hardcoded button/servo logic with calls into `InputBindingManager` using controller states from Bluepad32.
  - Keep existing Wi‑Fi/BT scan duty cycle scheduler intact.

**UI Design (data/controls.html + data/controls.js)**
- New Controls page (or a tab in config.html) to manage bindings:
  - Binding list with add/remove/reorder; each binding shows Input and Action editors.
  - Input editor: select type (button/dpad/stick_dir/axis_pair), specific code(s)/axis, edge (press/release/hold), thresholds/deadband.
  - Action editor: choose action type (drive_pair/servo_set/servo_toggle_band/servo_nudge/led_set/gpio_set), show relevant params with validation.
  - Presets: button to load defaults (mirrors current behavior: right stick→GUI pair, left stick→other pair, D‑Pad nudges, R2/L2 servo bands).
  - Preview: optional “Test” button to POST a single action to `/testAction`.
  - Save/Reset: integrate with existing config save flow.

**Defaults and Migration**
- On first run or missing `control_bindings`, seed defaults equivalent to current logic.
- If present, do not overwrite; allow reset to defaults via UI.

**Safety and Limits**
- Validate servo indices [0..2], angles [0..180].
- Validate motor indices/pairs [0..3]; for `drive_pair` allow `target: gui|other|[left,right]`.
- Limit LED ranges to configured `led_count` and throttle updates.
- GPIO actions whitelist pins or warn in UI; document 3V3 and current limits.
- Protect arbitration: axis‑based drive uses existing requestDrive* methods; button actions use dedicated methods and should not steal ownership unless configured as active drive.

**Phased Implementation**
- Phase 1: Data model + backend
  - Add schema + ConfigManager support; implement InputBindingManager for button/dpad and axis_pair; wire into sketch.cpp.
- Phase 2: UI minimal
  - Basic CRUD UI for bindings; JSON round‑trip; default preset.
- Phase 3: UI polish + previews
  - Validation, conditional forms, color picker, action tester.
- Phase 4: Extras
  - Per‑controller profiles, import/export config, cloning bindings.

**Acceptance Criteria**
- User can map R/L sticks to motor pairs and/or servos; map buttons/D‑Pad to servo angles, LED colors, and GPIO.
- Mappings persist in `/config.json` and apply at runtime without reflashing.
- Drive arbitration remains last‑writer‑wins; Wi‑Fi/BT coexist scheduling unaffected.

**Potential Pitfalls**
- JSON size and ArduinoJson memory: increase document capacity and test `/config.json` size.
- Update rate and CPU: cap continuous actions to sensible rates; avoid starving Wi‑Fi/BT.
- Backwards compatibility: provide defaults and migrate safely.

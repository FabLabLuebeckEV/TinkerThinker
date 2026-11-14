Purpose
This file briefs future AI/code assistants on key project specifics, build/run details, and recent architectural decisions so you can work efficiently without re-discovering context.

Project Summary
- Firmware for ESP32 robotics board combining:
  - Bluepad32 gamepad control (BTStack)
  - 4 DC motors, 3 servos, WS2812 LEDs
  - Wi-Fi AP/STA + Async Web UI over LittleFS
  - Arduino-ESP32 as an ESP-IDF component

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
1) BT/Wi-Fi coexistence - search windows instead of duty cycling (Nov 2026)
   - The ESP32 shares a single 2.4 GHz radio. Only BT inquiry (searching for new devices) is expensive;
     page scan (paired controllers reconnecting) is always on and cheap (`gap_connectable_control(1)`).
   - Inquiry runs only in a 60 s search window: after boot, MODE button hold (~0.7 s) or `POST /bt/scan`.
     The window ends early when a controller connects. Wi-Fi stays on the whole time.
   - Inquiry is periodic with 2.56 s blocks and 2.5-3.8 s gaps (Bluepad32 properties: inquiry length 2,
     min/max period 4/5, set once in `prepareBluetoothStack()` only if different), so Wi-Fi gets
     contiguous airtime for association/DHCP. The old 100-500 ms host toggling never completed an inquiry.
   - Wi-Fi is no longer paused on controller connect (it dropped all phones for 3 s).
   - `reconcileInquiry()` re-applies the wanted inquiry state on the BTstack thread once per second
     (`gap_inquiry_periodic_start` / `gap_inquiry_stop`, both no-ops when the state already matches).
     BTstack can reject start/stop with 0x0c while a previous command is in flight, and Bluepad32 then
     keeps a wrong `bt_scanning_enabled` flag; without the reconcile inquiry could keep running forever.
   - Do not call `esp_wifi_set_ps(WIFI_PS_NONE)`: with BT enabled ESP-IDF rejects it (coexistence needs modem sleep).
   - The old `bt_scan_*` keys remain in ConfigManager/serial for compatibility but are unused.

2) Control arbitration - last-writer-wins
   - Both BT gamepad and WebSocket can send drive commands.
   - New API in TinkerThinkerBoard:
     - requestDriveFromBT/WS(int axisX, int axisY)
     - requestMotorDirectFromBT/WS(int motorIndex, int pwm)
     - requestMotorStopFromWS(int motorIndex)
   - Rules:
     - Non-neutral command (axes beyond threshold or non-zero PWM) takes ownership and applies immediately.
     - Neutral commands apply only if they come from the current owner.
     - Prevents one source’s neutral from cancelling the other’s active command.
   - Neutral threshold: 16 (see TinkerThinkerBoard::neutralThreshold).

3) Web UI additions
   - The six BT scan timing inputs were removed again (Nov 2026, see 1); config.html now has the
     controller search ("Controller suchen") with an "Erlauben" button in the whitelist section.

4) sdkconfig cleanup (Jan 2026)
   - env:esp32dev now points to sdkconfig.esp32dev aligned with the working skip_power_pin build.
   - Wi-Fi/BT SW coexistence forced on (CONFIG_ESP_COEX_SW_COEXIST_ENABLE plus *_SW_COEXIST).
   - Disabled unused stacks to free IRAM/flash: RainMaker, Insights, Matter/OpenThread, Ethernet/PPP; GDB stub and all coredumps off.
- Rationale: Only BT Classic via Bluepad32 and Wi-Fi AP/STA are used; IRAM was tight. Re-enable BLE/RainMaker if needed.

5) MODE button (Jan 2026, changed Nov 2026) **BREAKING**
   - The former H-bridge fault pin is now a MODE button on GPIO39 (active-low, pulled to GND on press).
   - GPIO39 is input-only and has no internal pull-up; use external pull-up in hardware.
   - Fault monitoring was removed; any code relying on motorFaults/checkFaults must be updated.
   - Startup factory reset (hold MODE for 10s) is checked before Wi-Fi initialization, so reset still works even if STA cannot connect.
   - Holding MODE (~0.7 s) starts/stops the controller search window (the radio modes Wi-Fi only / BT only were removed).
   - Status LED (LED0): ready = dim white, search = blinking blue, controller = green, disconnected = red.

6) Battery percentage curve (Jan 2026)
   - Voltage measurement reverted to the original analogRead method.
   - Percentage now uses a Li-ion discharge curve (non-linear) for more realistic %.

7) Setup page reliability + mobile UX (Feb 2026)
   - `data/setup.html` now includes viewport meta and connection state indicator.
   - `data/setup.js` now has WebSocket auto-reconnect with backoff and safe send helper.
   - Setup now persists motor drive pair selection by posting `motor_left_gui` / `motor_right_gui`.
   - `main/WebServerManager.cpp` exposes/parses `motor_left_gui` and `motor_right_gui` in `/getConfig` and `/config`.
   - `data/styles.css` adds mobile-responsive nav and full-width setup buttons for phone usability.

8) Control GUI modes (`gui_mode`), Basic Workshop UI & Legacy v1 Board Support
   - `gui_mode` in `/config.json` selects the primary control tab served at `/`:
     - `basic` (default): `data/basic.html` + `data/basic.js` for the Workshop Robot (2 track motors + 1 gripper motor, battery bar, cellular-style signal bars, single-LED color picker in header, labeled drive joystick, hold-to-run gripper open/close buttons).
     - `drive`: `data/index.html` + `data/script.js` (full drive & gripper control with servos, current charts, and multi-LED controls).
     - `arm`: `data/arm.html` (dedicated Robot Arm control page).
   - `data/setup.html` + `data/setup.js`: includes GUI mode selection, skips unconfigured motor steps (`none`), persists `gripper_motor_gui` and `gripper_inverted_gui`.
   - `legacy_board` (`bool` in `/config.json`): restores v1 hardware compatibility:
     - GPIO39 acts as active-low H-bridge fault input (`FAULT`, checked via `board.checkFaults()`) instead of the v2 `MODE` button, and disables the startup 10s factory-reset hold check on GPIO39.
     - Limits active servos to 3 (`Servo 1..3` on GPIO13, GPIO33, GPIO17) and hides `Servo 4..7` in the Web UI (`ConfigManager::getActiveServoCount()`).

9) Automatic LittleFS Gzip Compression (`tools/gzip_littlefs.py`)
   - Registered via `extra_scripts = pre:tools/gzip_littlefs.py` in `platformio.ini`.
   - Automatically compresses `.html`, `.js`, `.css`, `.ico`, `.svg` from `data/` into `.pio/data_gzip/*.gz` (~63% size reduction) while keeping `config.json` and `.png` uncompressed for runtime `ConfigManager` read/write access.
   - `ESPAsyncWebServer` transparently serves `.gz` files when present in LittleFS.
   - Fast standalone build & flash (<1s without ESP-IDF CMake re-evaluation):
     - `python tools/gzip_littlefs.py --build`
     - `python tools/gzip_littlefs.py --build --flash COM19`

10) Review fixes (Oct 2026)
   - Captive portal: connectivity probes pass automatically 15 s after a client connects, so phones stay on the robot AP. AP uses channel 6, max 4 clients, Wi-Fi power save off.
   - Static web files are served by an `onNotFound` LittleFS handler (`no-cache` + gzip-CRC ETag) instead of `serveStatic`, so browsers never keep stale UI files after an update.
   - WS drive watchdog: pages resend `{x,y}` while the joystick is deflected; the firmware neutralizes WS drive after 1 s without joystick data (via the arbiter, BT is not stopped).
   - All pages reconnect the WebSocket forever (max 5 s backoff).
   - `reApplyConfig()` creates the motor/servo/LED/monitor objects once and only applies new settings afterwards (no leak; other tasks keep valid pointers).
   - POST bodies of `/arm/config` and `/bt/whitelist` are assembled across chunks and errors are reported.
   - BT whitelist is enforced by `main/BtPairing.*` via a filter hook in `components/bluepad32_arduino`
     (`arduino_set_device_filter`, called on discovery and before a controller gets a seat), not by the
     Bluepad32 allowlist (no NVS writes, browser search also lists non-allowed controllers).
     Controller MACs are formatted in BTstack order [0]..[5] (they were reversed before, so old whitelist
     entries added via the UI never matched).
   - GitHub Pages: the CI `pages` job publishes `tools/webserial_installer.html` plus the freshly built binaries under `./release/` (GitHub release downloads are blocked by CORS).

11) Motor drive (Nov 2026)
   - H-bridges are AT8833 (DRV8833 clone). `MotorController::applyPwm()` uses slow decay by default
     (one input high, the other inverted PWM, off-phase = brake): torque at low PWM, near-linear speed.
     `motor_decay: "fast"` restores the old coast-PWM behavior.
   - PWM frequency per motor from `motor_frequency[]` (clamped 100-40000 Hz, default 20 kHz), applied at
     runtime via `ledcChangeFrequency` (channels 2n/2n+1 share one LEDC timer).
   - Kick-start: on start from standstill or direction change the output is `motor_kick_pwm` for
     `motor_kick_ms` (default 255 / 40 ms); `board.updateMotors()` in `loop()` ends the kick. Applies to
     `controlMotor` and `controlMotorRaw` (setup deadband test), not counted in `getMotorPWM()`.
   - All motor writes go through `writeOutput()` under a mutex (WS, BT loop and ramps write concurrently).
   - Current sensing averages over whole PWM periods (window >= 1 ms, 23/29 samples, coprime to the
     period count), period per sensor set from the motor frequency. Sensor 0 (GPIO34) = Motor B, sensor 1 (GPIO36) = Motor A.
   - Slow decay changes measured currents at partial PWM; re-check arm `stallLimit` values if homing misbehaves.
   - Stop mode `motor_stop_mode`: `brake` (default, both inputs high) or `coast` (both low).
   - Straight-line trim `motor_trim[4]` (50-100 %, default 100) scales only the range above the deadband
     in `controlMotor()` (not in `controlMotorRaw`), so the start threshold stays calibrated.
   - Stall protection (Motor A/B only): `board.monitorMotors()` runs every 100 ms in the WebClientTask (also
     without browser), measures both sensors (telemetry reuses `getLastHBridgeAmps()`), and calls
     `MotorController::tripStall()` when a driven motor draws >= `motor_stall_amps` (default 1.1 A, sensor
     saturates ~1.3 A) for `motor_stall_ms` (default 1000 ms). The motor stays blocked in that direction until
     the command returns to 0 or reverses. Telemetry field `motorBlocked[4]`. Arm stall detection (~0.3 s,
     ~1 A) reacts first.

12) Sumo template for the SchrottBot (`gui_mode: "sumo"`, Nov 2026)
   - Page `data/sumo.html` + `data/sumo.js` (reuses `basic.js` for drive joystick, battery, signal, LED and the
     WebSocket; `basic.js` forwards telemetry `sumo` to `window.onSumoTelemetry`). Served at `/` in sumo mode and at `/sumo`.
   - Three functions in percent 0..100: arms (both arm servos, the right one runs mirrored through its own angle
     range), lift (ramp up), push (ramp forward). `TinkerThinkerBoard::setSumoPos/nudgeSumo/getSumoPos`.
   - Servo mapping + angles in config key `sumo` (`{"arm_l":{"servo":4,"a0":0,"a1":180}, "arm_r", "lift", "push"}`,
     servo 1..7 as printed on the board, a0 = angle at 0 %, a1 = angle at 100 %). Defaults: Servo 4..7, right arm
     reversed. `GET/POST /sumo/config` (register `/sumo/config` before `/sumo`: a handler for `/sumo` also accepts
     `/sumo/...`). In sumo mode the servos boot at 0 % instead of 180°.
   - WebSocket: `{"sumo_arms": 0..100, "sumo_lift": .., "sumo_push": ..}`; telemetry `sumo: [arms, lift, push]`.
   - Bindings: `sumo_move {role: arms|lift|push, speed: %/s signed}` with edge `hold` moves while held and stays on
     release; evaluated in `InputBindingManager::tickSumo()` from the controller snapshot (controllers often send no
     new data while a button is held, `process()` only runs on new data). `sumo_set {role, pct}` on press.
   - Controls editor: button "Vorlage SchrottBot Sumo" (left stick drive, D-pad right/left arms open/close, D-pad up/down speed_adjust +/-0.1, R1/L1 ramp up/down,
     R2/L2 ramp forward/back, Cross lift 100 %, Square push 100 %, Triangle all to 0 %, Circle `speed_toggle {low, high}`
     0.5 <-> 1.0, Start `sumo_toggle {role}` arms, right stick X `sumo_move` with axis_pair input:
     proportional, `action.axis` x|y, deadband default 60). Misc buttons (`MISC_START/SELECT/SYSTEM`) are
     `miscButtons() << 16` in `process()` and the controller snapshot. It replaces the list; "Speichern" applies it.
   - Serial `set_config`/`get_config` also carry `sumo` (same object as `/sumo/config`).

Editing Guidelines
- Keep changes focused; do not introduce a second Arduino core.
- Respect the control arbiter API; route all new control surfaces through TinkerThinkerBoard request* methods.
- When touching Wi-Fi/BT behavior, adjust the search window logic in sketch.cpp (`setInquiry`, `prepareBluetoothStack`) and `BtPairing`; do not reintroduce Wi-Fi shutdowns or fast inquiry toggling.
- UI fields: add in data/config.html and wire to data/config.js; expose via WebServerManager.
- Always keep all source and documentation files in clean UTF-8 (without BOM) with intact German umlauts (`ä`, `ö`, `ü`, `ß`) and Unicode symbols (`°`, `↑`, `↓`, `►`, `▼`, `✓`). Never use PowerShell 5.1 `Get-Content`/`Set-Content` without explicit UTF-8 handling.

Common Tasks
- Adjust BT search: window length `searchWindowMs` in sketch.cpp, inquiry block/gap lengths in `prepareBluetoothStack()`.
- Add a new control source: implement request methods in TinkerThinkerBoard and call them from the source.
- Expand config: extend ConfigManager load/save; expose in WebServerManager; add UI inputs.

Pitfalls
- Duplicate Arduino symbols: do NOT create components/arduino. Use managed_components/espressif__arduino-esp32 only.
- Web UI 404: remember to upload the LittleFS image.
- Windows shells: prefer PowerShell path shown above; plain `pio` may not be on PATH.
- Windows PlatformIO upload hang (`UnicodeEncodeError`): `esptool.py` v5 outputs Unicode block characters (`█`) in its progress bar, which crashes PlatformIO's `cp1252` stdout pipe on Windows. Always set `$env:PYTHONIOENCODING = "utf-8"` before running `pio.exe run -t upload` / `uploadfs`, or invoke `esptool.py` / `python tools/gzip_littlefs.py --build --flash <PORT>` directly.

Contact/Context
- This repo is typically built in Windows with PlatformIO.
- Prior issues included double Arduino inclusion and BT starving Wi-Fi during association; both addressed as above.

CI/Workflow Notes (Important)
- Primary CI: .github/workflows/esp32-platformio-build-release.yml
  - Builds `env:esp32dev` via PlatformIO, creates LittleFS image, uploads artifacts.
  - On every successful push to `main`, creates a GitHub Release with tag `main-YYYYMMDD-HHMM-<shortsha>` and attaches bootloader/partitions/firmware/littlefs images.
  - Tag-based releases are still supported and attach the same artifacts.
- Removed CI: .github/workflows/esp-idf-latest.yaml
  - Pure ESP-IDF matrix build (esp32/s3/c3/c6/h2) removed to avoid duplication; PlatformIO CI is source of truth.
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
- Do not change FastLED to header-only; it has `.cpp` implementations for IDF5 (RMT v5, RGBW utils, etc.).
- Build is driven by `components/FastLED/CMakeLists.txt` (kept as-is from vendor).
- If CI reports missing `fl/ui_impl.h`, verify the vendor tree is complete (e.g., .gitignore didn’t exclude it) rather than adding CI-only include hacks.

Do/Don’t Summary
- Do: Keep Arduino-ESP32 as managed component; no duplicate `components/arduino`.
- Do: Use PlatformIO workflow; avoid separate ESP-IDF CI unless necessary.
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
  - Verifies `littlefs.bin` fits the FS partition; supports auto-scan of CH340/CP210x serial adapters.

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
- Inputs: Gamepad buttons (A/B/X/Y, L1/L2/R1/R2, sticks pressed), D-Pad directions, stick directions (up/down/left/right) and axes (X/Y, RX/RY) with thresholds.
- Actions: Motor drive (pair selection and mode), Servo set (fixed angle, band toggle, axis mapping), LED set color (single, range), GPIO set/toggle (HIGH/LOW), No-op.
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
  - D-Pad RIGHT/LEFT → servo nudge:
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
  - Maintains per-controller previous states for edge detection.
  - Evaluates button/dpad events and axis values each loop.
  - Applies actions through TinkerThinkerBoard APIs, respecting last-writer-wins for drive commands.
  - Enforces deadband, clamps, and rate limiting (e.g., 50 Hz for continuous axis actions, min interval for LED updates).
- WebServerManager:
  - Extend `/getConfig` and `/config` to expose/accept `control_bindings`.
  - Add `/testAction` endpoint to execute a single action payload for UI previews (optional, safe-guarded).
- sketch.cpp:
  - Replace hardcoded button/servo logic with calls into `InputBindingManager` using controller states from Bluepad32.
  - Keep existing Wi-Fi/BT scan duty cycle scheduler intact.

**UI Design (data/controls.html + data/controls.js)**
- New Controls page (or a tab in config.html) to manage bindings:
  - Binding list with add/remove/reorder; each binding shows Input and Action editors.
  - Input editor: select type (button/dpad/stick_dir/axis_pair), specific code(s)/axis, edge (press/release/hold), thresholds/deadband.
  - Action editor: choose action type (drive_pair/servo_set/servo_toggle_band/servo_nudge/led_set/gpio_set), show relevant params with validation.
  - Presets: button to load defaults (mirrors current behavior: right stick→GUI pair, left stick→other pair, D-Pad nudges, R2/L2 servo bands).
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
- Protect arbitration: axis-based drive uses existing requestDrive* methods; button actions use dedicated methods and should not steal ownership unless configured as active drive.

**Phased Implementation**
- Phase 1: Data model + backend
  - Add schema + ConfigManager support; implement InputBindingManager for button/dpad and axis_pair; wire into sketch.cpp.
- Phase 2: UI minimal
  - Basic CRUD UI for bindings; JSON round-trip; default preset.
- Phase 3: UI polish + previews
  - Validation, conditional forms, color picker, action tester.
- Phase 4: Extras
  - Per-controller profiles, import/export config, cloning bindings.

**Acceptance Criteria**
- User can map R/L sticks to motor pairs and/or servos; map buttons/D-Pad to servo angles, LED colors, and GPIO.
- Mappings persist in `/config.json` and apply at runtime without reflashing.
- Drive arbitration remains last-writer-wins; Wi-Fi/BT coexist scheduling unaffected.

**Potential Pitfalls**
- JSON size and ArduinoJson memory: increase document capacity and test `/config.json` size.
- Update rate and CPU: cap continuous actions to sensible rates; avoid starving Wi-Fi/BT.
- Backwards compatibility: provide defaults and migrate safely.

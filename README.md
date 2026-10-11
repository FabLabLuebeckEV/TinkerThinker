# TinkerThinker

English | [Deutsch](README.de.md)

ESP32-based robotics controller firmware with Bluetooth gamepad support, Wi-Fi web control, motor and servo control, WS2812 LEDs, and a LittleFS-hosted setup/configuration UI.

This README starts with the finished product from the end user's perspective. Developer and build notes are further down.

## Contents

- [Using a Flashed Board](#using-a-flashed-board)
- [Default Controls](#default-controls)
- [Factory Reset](#factory-reset)
- [Web Pages](#web-pages)
- [What This Project Includes](#what-this-project-includes)
- [Hardware Overview (default pins)](#hardware-overview-default-pins)
- [Project Layout](#project-layout)
- [Quick Start (PlatformIO)](#quick-start-platformio)
- [Bluetooth and Wi-Fi Coexistence](#bluetooth-and-wi-fi-coexistence)
- [Control Arbitration](#control-arbitration)
- [Releases and Flashing](#releases-and-flashing)
- [Configuration Reference](#configuration-reference)
- [Troubleshooting](#troubleshooting)
- [Credits and Support](#credits-and-support)
- [Further Reading](#further-reading)

## Using a Flashed Board

### Power on

1. Turn on the board using the physical power switch on the board edge next to the antenna housing.
2. The status LED should light dim white. Dim white means the board is on and in `Normal` radio mode.

From here you have two main control options:

- connect a Bluetooth controller
- connect with a phone or laptop over Wi-Fi and use the web interface

### Option 1: Bluetooth controller

1. Put your controller into pairing mode.
2. Wait for the board to accept a new Bluetooth connection.
3. Once a controller is connected, the status LED turns green.

Notes:

- In normal mode, Bluetooth scanning is duty-cycled to leave airtime for Wi-Fi.
- If pairing is unreliable, switch the board to Bluetooth scan mode with the MODE button. In that mode the LED is blue and Bluetooth scanning stays on continuously.
- Pairing keys are cleared on every boot, so re-pairing is usually straightforward.

### Option 2: Control over Wi-Fi

1. Search for the board's hotspot on your phone or laptop. Depending on the saved configuration, the hotspot is typically named `TinkerThinker` or `TinkerThinkerAP`.
2. Connect to that Wi-Fi network.
3. If your device warns that the network has no internet access, stay connected anyway.
4. Open [http://192.168.4.1](http://192.168.4.1) in your browser.
5. The control interface should appear and you can drive and configure the robot there.

Important mobile/network notes:

- Disable VPNs before connecting.
- Some phones automatically switch back to another Wi-Fi or to mobile data when they detect no internet. If that happens, reconnect to the robot and stay on that network.
- If the page does not load, make sure your browser did not silently rewrite `http://192.168.4.1` to `https://192.168.4.1`.
- Trying another browser often helps.
- Depending on the phone, turning mobile data off can help. On some devices the opposite helps, so it is worth trying both.

### Changing controller mappings

The web interface is not only for driving the board. It also lets you change the Bluetooth controller bindings and other runtime settings without reflashing the firmware.

## Default Controls

The default Bluetooth mapping is configured in [`main/ConfigManager.cpp`](main/ConfigManager.cpp) and mirrored in [`data/config.json`](data/config.json).

By default:

- Right stick (`RX` / `RY`) drives the configured GUI motor pair.
- Left stick (`X` / `Y`) drives the other motor pair.
- D-pad right nudges Servo 0 by `+10`.
- D-pad left nudges Servo 0 by `-10`.
- `R2` toggles Servo 0 between `0°` and `90°`.
- `L2` toggles Servo 0 between `90°` and `180°`.
- `R1` increases the global speed multiplier.
- `L1` decreases the global speed multiplier.

Motor assignment details:

- The configurable "GUI motor pair" is stored as `motor_left_gui` and `motor_right_gui`.
- In the shipped default config, that pair is motor `2` and motor `3`.
- That means the right stick drives motor channels `C/D` by default, while the left stick drives the remaining pair `A/B`.
- This can be changed later in `/config` or by using the setup wizard at `/setup`.

## Factory Reset

If you changed the configuration, forgot a password, or want to return to a known default state, reset the board like this:

1. Press and hold the MODE button on the side of the board.
2. While holding MODE, briefly press the reset button to reboot the board.
3. Keep holding MODE during startup for about 10 seconds. During the reset hold time, the status LED cycles through red, white, and orange.
4. When the reset completes, the configuration is cleared and the board restarts.

After a factory reset:

- the hotspot name goes back to `TinkerThinkerAP`
- saved Wi-Fi and controller configuration is reset to defaults

## Web Pages

### `/` (Control Tab - selectable via `gui_mode`)

The root route `/` serves the configured control interface (`gui_mode` in `/config` or `/setup`):

- **`basic` (Default - Workshop Robot, `/basic`)**:
  - Compact top status bar with visual battery bar, 4-bar wireless signal indicator (`wifiRssi`), and single-LED color picker (LED 0)
  - Touch joystick with directional labels (`Vorwärts`, `Links`, `Rechts`, `Rückwärts`)
  - Dedicated `Greifer auf` and `Greifer zu` touch buttons linked to the configured gripper motor (`motor_gripper_gui`)
- **`drive` (Full Drive Robot, `/drive`)**:
  - Live battery, motor current, and servo status
  - Touch joystick
  - Servo slider
  - Motor test buttons and gripper buttons
  - LED color and range control
- **`arm` (Robot Arm, `/arm`)**:
  - Dedicated control page for robot arm setups

### `/config`

Main configuration page for:

- Control GUI mode (`basic`, `drive`, `arm`)
- Legacy board hardware switch (`legacy_board` for v1 boards)
- Wi-Fi AP/STA settings
- Hotspot name and password
- Motor inversion, deadband, and PWM frequency
- GUI drive motor pair (`motor_left_gui`, `motor_right_gui`) and gripper motor (`motor_gripper_gui`)
- LED count
- Servo pulse widths
- Bluetooth scan timings
- Drive profile parameters

### `/controls`

Binding editor for controller inputs and actions:

- Buttons, D-pad directions, and stick axes
- Independent Bluetooth joystick direction settings (`bt_invert_x`, `bt_invert_y`, `bt_swap_axes`)
- Actions for motors, servos, LEDs, GPIO, and speed scaling
- Live test sliders for LEDs, motors, and active servos (3 on v1 boards, 7 on v2 boards)

### `/setup`

Guided first-time setup wizard for:

- Control GUI mode selection (`basic`, `drive`, `arm`) and hotspot name
- Motor role assignment (`Kettenmotor Links`, `Kettenmotor Rechts`, `Greifer Motor`, `Kein Motor`)
- Automatic step skipping: motors set to `Kein Motor` skip direction and deadband steps
- Direction correction and deadband tuning for active motors
- Automatic assignment of `motor_left_gui`, `motor_right_gui`, and `motor_gripper_gui`

## What This Project Includes

- Bluepad32 gamepad support over Bluetooth Classic
- 4 DC motor outputs
- Up to 7 servo outputs (7 on v2 boards, 3 on v1 legacy boards)
- WS2812 LED control
- Wi-Fi AP/STA support
- Async web UI served from LittleFS (automatically `.gz` compressed) with selectable control dashboards (`basic`, `drive`, `arm`)
- Battery voltage and current monitoring
- Configurable control bindings
- Runtime and compile-time support for v1 legacy boards (`legacy_board`)
- Last-writer-wins arbitration between web control and Bluetooth control

## Hardware Overview (v2 default pins vs. v1 legacy boards)

- Motors
  - `M0`: `pin1=16`, `pin2=25`
  - `M1`: `pin1=32`, `pin2=27`
  - `M2`: `pin1=4`, `pin2=12`
  - `M3`: `pin1=15`, `pin2=14`
- Servos
  - `S0`: `pin=13` (v1 & v2)
  - `S1`: `pin=33` (v1 & v2)
  - `S2`: `pin=17` (v1 & v2)
  - `S3`: `pin=23` (v2 only)
  - `S4`: `pin=18` (v2 only)
  - `S5`: `pin=19` (v2 only)
  - `S6`: `pin=5` (v2 only)
- WS2812 LED data: `pin=2`
- Battery ADC: `pin=35`
- Current sense: `pin=34`, `pin=36`
- GPIO 39:
  - **v2 boards (`legacy_board: false`, default)**: `MODE` button, active low (requires external pull-up)
  - **v1 boards (`legacy_board: true`)**: H-bridge motor driver fault input (`GPIO_FAULT_1`, `LOW` = OK, `HIGH` = motor fault)

### Legacy v1 Board Support

Older v1 boards wire GPIO 39 to the motor driver fault pin (which stays `LOW` during normal operation) instead of a `MODE` button, and provide 3 servo headers instead of 7.

- **Runtime switch**: Enable **Älteres Board (v1)** in `/config` (or set `"legacy_board": true` in `data/config.json`). When enabled, GPIO 39 is monitored as `GPIO_FAULT_1`, `MODE` button polling is disabled, and only 3 servos (`13, 33, 17`) are initialized.
- **Compile-time environment**: Build with `env:esp32dev_legacy` (`pio run -e esp32dev_legacy`) to bake `LEGACY_BOARD_DEFAULT=true` into the firmware defaults.
- **Boot safeguard**: Even if a v1 board boots with default v2 settings (`legacy_board: false`), the startup reset check detects if GPIO 39 remains permanently `LOW` without a button release and continues normal boot instead of entering a factory-reset loop.

## Project Layout

- [`main/`](main/)
  - [`sketch.cpp`](main/sketch.cpp): Arduino setup/loop, radio-mode handling, legacy board handling, Bluepad32 processing
  - [`main.c`](main/main.c): ESP-IDF entry point and Bluepad32 glue
  - [`TinkerThinkerBoard.*`](main/TinkerThinkerBoard.cpp): board facade and control arbitration
  - [`InputBindingManager.*`](main/InputBindingManager.cpp): controller input mapping
  - [`WebServerManager.*`](main/WebServerManager.cpp): web UI, WebSocket, config routes
  - [`ConfigManager.*`](main/ConfigManager.cpp): persistent configuration in LittleFS
- [`data/`](data/): web app source files (`basic.html`, `index.html`, `arm.html`, `config.html`, `setup.html`, `controls.html`), automatically packed as `.gz` into LittleFS by [`tools/gzip_littlefs.py`](tools/gzip_littlefs.py)
- [`PCB/`](PCB/): board files and exports
- [`CAD/`](CAD/): mechanical models and Sumo robot kit files
- [`tools/`](tools/): automated flashing scripts, LittleFS gzip hook, WebSerial installer, and CI helpers

## Quick Start (PlatformIO)

This repository is primarily built with PlatformIO.

1. Open the project in VS Code with the PlatformIO extension.
2. Select `env:esp32dev` (or `env:esp32dev_legacy` for v1 boards).
3. Build the firmware.
4. Upload the firmware.
5. Upload the LittleFS image so the web UI is available.
6. Open the serial monitor at `115200` baud if you want logs.

Windows PowerShell examples:

- Build firmware (v2 default): `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev`
- Build firmware (v1 legacy default): `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev_legacy`
- Upload firmware: `$env:PYTHONIOENCODING="utf-8"; & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t upload`
- Build LittleFS (`.gz`-optimized via PlatformIO): `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t buildfs`
- Upload LittleFS (`.gz`-optimized via PlatformIO): `$env:PYTHONIOENCODING="utf-8"; & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t uploadfs`
- Fast standalone LittleFS build & flash (<1s without CMake overhead): `python tools/gzip_littlefs.py --build --flash COM19`

### Optimized `.gz` LittleFS Build (Compressed Web UI)

To minimize flash usage and speed up page loads over Wi-Fi, all static web assets in [`data/`](data/) are automatically compressed with Gzip (level 9) before building `littlefs.bin`:

- **How it works**:
  - [`platformio.ini`](platformio.ini) registers `extra_scripts = pre:tools/gzip_littlefs.py`.
  - Before LittleFS packaging, [`tools/gzip_littlefs.py`](tools/gzip_littlefs.py) stages `data/` into `.pio/data_gzip/` and compresses `.html`, `.js`, `.css`, `.ico`, and `.svg` files into `.gz` (**~63% size reduction**, from ~208 KB down to ~77 KB).
  - `config.json` (required uncompressed for runtime read/write by `ConfigManager`) and `.png` files are copied uncompressed.
  - `ESPAsyncWebServer` transparently detects and serves the `.gz` files from LittleFS with `Content-Encoding: gzip`.
- **How to build and flash the optimized `.gz` filesystem**:
  1. **Fastest method (standalone script, <1 second, skips ESP-IDF CMake re-evaluation)**:
     ```powershell
     # Build .gz-optimized littlefs.bin only:
     python tools/gzip_littlefs.py --build

     # Build and flash directly to the board (e.g. COM19):
     python tools/gzip_littlefs.py --build --flash COM19
     ```
  2. **Via PlatformIO CLI (`buildfs` / `uploadfs`)**:
     ```powershell
     # Build .pio/build/esp32dev/littlefs.bin with automatic .gz compression:
     & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t buildfs

     # Build and upload .gz-optimized LittleFS via PlatformIO:
     $env:PYTHONIOENCODING="utf-8"
     & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t uploadfs
     ```
  3. **In VS Code (PlatformIO GUI)**:
     - Click **Project Tasks -> esp32dev -> Platform -> Build Filesystem Image** or **Upload Filesystem Image**. The `pre:tools/gzip_littlefs.py` hook runs automatically before `mklittlefs`.

Important:

- The web UI will not work until the LittleFS image has been uploaded.
- Keep all text files in `data/` encoded as clean UTF-8 (without BOM).
- This project uses Arduino as a managed ESP-IDF component. Do not add a second `components/arduino`.

## Bluetooth and Wi-Fi Coexistence

The ESP32 uses one shared 2.4 GHz radio for Wi-Fi and Bluetooth. This firmware therefore duty-cycles Bluetooth scanning so Wi-Fi still has airtime.

Current radio modes:

- `Normal`: dim white LED
- `Wi-Fi only`: orange LED
- `Bluetooth scan only`: blue LED
- controller connected: green LED

The MODE button cycles through the radio modes. In normal mode, Wi-Fi is also paused briefly after a controller connection to improve Bluetooth pairing stability.

## Control Arbitration

Both the Bluetooth controller and the web UI can issue drive commands.

The firmware uses a last-writer-wins model:

- the last non-neutral control source becomes the owner
- neutral input from another source does not cancel the active owner

This behavior is implemented in [`main/TinkerThinkerBoard.cpp`](main/TinkerThinkerBoard.cpp).

## Releases and Flashing

The main CI workflow builds:

- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`
- `littlefs.bin`

On successful pushes to `main`, GitHub Actions creates a release tagged like `main-YYYYMMDD-HHMM-<shortsha>`.

### Flashing with Auto-Flasher

You can flash release assets automatically using the flasher scripts in `tools/`:

- **Windows (one-click):** Run [`tools/start_flasher.bat`](tools/start_flasher.bat). It automatically checks for `uv` (recommended) or Python with `.venv`, installs requirements (`esptool`, `pyserial`), and launches the flasher.
- **Python / CLI:**
  ```bash
  python tools/auto_flasher.py
  ```
  Or using `uv`:
  ```bash
  uv run --with-requirements tools/requirements.txt python tools/auto_flasher.py
  ```

Flasher features:
- Automatically downloads and extracts the latest release assets from GitHub Actions.
- Calculates offsets from [`platformio.ini`](platformio.ini) and the partition CSV.
- Continuously watches serial ports: connect an ESP32 board to flash it automatically.
- Tracks flashed boards by MAC in `tools/blacklist.txt` per release tag. Boards already on the target release are skipped; if a new release is available, they will be updated.
- CLI flags:
  - `--once` or `-1`: Flash a single connected board and exit.
  - `--clear-blacklist` or `-c`: Clear blacklist to reflash boards.

### WebSerial Installer (Browser)

Alternatively, you can flash firmware and upload configurations directly through Google Chrome or Edge using the WebSerial installer at [`tools/webserial_installer.html`](tools/webserial_installer.html).

## Configuration Reference

Persistent configuration is stored in `/config.json` on LittleFS.

Main config groups:

- Control GUI mode (`gui_mode`: `"basic"`, `"drive"`, `"arm"`)
- Legacy board v1 switch (`legacy_board`)
- Wi-Fi mode, SSIDs, passwords
- Motor inversion and deadband
- GUI drive motor pair (`motor_left_gui`, `motor_right_gui`) and gripper motor (`motor_gripper_gui`)
- Motor frequency
- Servo pulse width limits
- LED count, brightness, and gamma
- Bluetooth scan timing
- Drive profile and motor curve
- `control_bindings`

The HTTP and WebSocket API is documented in [`API.md`](API.md).

## Troubleshooting

- The web page does not open:
  - make sure you are connected to the robot hotspot, not another Wi-Fi
  - disable VPNs
  - use `http://192.168.4.1`, not `https://192.168.4.1`
  - try another browser
  - on phones, try toggling mobile data
- The board shows a white LED but the controller does not connect:
  - put the controller back into pairing mode
  - switch to Bluetooth scan mode if needed so the LED turns blue
- **PS3 / DualShock 3 clone controllers** (common no-name clones with MAC prefix `A0:5A:5E` or `A0:5A:5F`) are supported with special handling:
  - the firmware skips the Bluetooth name request (clones don't answer it and cause a 15-second timeout)
  - GAP Security Level 2 and Wii-style PIN logic are bypassed for these devices
  - if your clone has a different MAC OUI and still doesn't connect, open an issue with the first 3 bytes of its MAC address
- The controller connects but the website becomes unreachable briefly:
  - this can happen for about 3 seconds because Wi-Fi is intentionally paused to improve Bluetooth pairing
- Motors move in the wrong direction:
  - correct them in `/setup` or `/config`
- The UI returns 404 or stays blank after flashing:
  - upload the LittleFS image

## Credits and Support

The board and software were created at FabLab Lübeck:

- Andre: hardware
- Marco: software

If you run into hardware or software problems, they are the right people to contact through the project or FabLab channels.

Project/community support:

- Board and main firmware: [TinkerThinker / board support Discord](https://discord.gg/kxpnWjJjyF)
- Bluepad32 / Bluetooth-related topics only: [Bluepad32 Discord](https://discord.gg/r5aMn6Cw5q)

## Further Reading

- [Bluepad32 for Arduino](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
- [Arduino as ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- [ESP-IDF VS Code extension](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/vscode-setup.html)

# TinkerThinkerBL

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
2. The status LED should light dim white.
3. Dim white means the board is on and in `Normal` radio mode.

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

1. Search for the board's hotspot on your phone or laptop.
2. Depending on the saved configuration, the hotspot is typically named `TinkerThinker` or `TinkerThinkerAP`.
3. After a factory reset, the hotspot name returns to `TinkerThinkerAP`.
4. Connect to that Wi-Fi network.
5. If your device warns that the network has no internet access, stay connected anyway.
6. Open `http://192.168.4.1` in your browser.
7. The control interface should appear and you can drive and configure the robot there.

Important mobile/network notes:

- Disable VPNs before connecting.
- Some phones automatically switch back to another Wi-Fi or to mobile data when they detect no internet. If that happens, reconnect to the robot and stay on that network.
- If the page does not load, make sure your browser did not silently rewrite `http://192.168.4.1` to `https://192.168.4.1`.
- Trying another browser often helps.
- Depending on the phone, turning mobile data off can help. On some devices the opposite helps, so it is worth trying both.

### Changing controller mappings

The web interface is not only for driving the board. It also lets you change the Bluetooth controller bindings and other runtime settings without reflashing the firmware.

## Default Controls

The default Bluetooth mapping is configured in [`main/ConfigManager.cpp`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/ConfigManager.cpp) and mirrored in [`data/config.json`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/data/config.json).

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
3. Keep holding MODE during startup for about 10 seconds.
4. During the reset hold time, the status LED cycles through red, white, and orange.
5. When the reset completes, the configuration is cleared and the board restarts.

After a factory reset:

- the hotspot name goes back to `TinkerThinkerAP`
- saved Wi-Fi and controller configuration is reset to defaults

## Web Pages

### `/`

Main control dashboard with:

- live battery and motor status
- touch joystick
- servo slider
- motor test buttons
- LED color control

### `/config`

Main configuration page for:

- Wi-Fi AP/STA settings
- hotspot name and password
- motor inversion, deadband, and PWM frequency
- LED count
- servo pulse widths
- Bluetooth scan timings
- drive profile parameters

### `/controls`

Binding editor for controller inputs and actions:

- buttons
- D-pad directions
- stick axes
- actions for motors, servos, LEDs, GPIO, and speed scaling

### `/setup`

Guided first-time setup page for:

- motor assignment
- direction correction
- deadband tuning
- GUI drive pair selection

## What This Project Includes

- Bluepad32 gamepad support over Bluetooth Classic
- 4 DC motor outputs
- 3 servo outputs
- WS2812 LED control
- Wi-Fi AP/STA support
- Async web UI served from LittleFS
- battery voltage and current monitoring
- configurable control bindings
- last-writer-wins arbitration between web control and Bluetooth control

## Hardware Overview (default pins)

- Motors
  - `M0`: `pin1=16`, `pin2=25`
  - `M1`: `pin1=32`, `pin2=27`
  - `M2`: `pin1=4`, `pin2=12`
  - `M3`: `pin1=15`, `pin2=14`
- Servos
  - `S0`: `pin=13`
  - `S1`: `pin=33`
  - `S2`: `pin=17`
- WS2812 LED data: `pin=2`
- Battery ADC: `pin=35`
- Current sense: `pin=34`, `pin=36`
- MODE button: `pin=39`, active low

Important:

- GPIO39 is input-only and has no internal pull-up.
- The hardware must provide an external pull-up for the MODE button.

## Project Layout

- [`main/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main)
  - [`sketch.cpp`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/sketch.cpp): Arduino setup/loop, radio-mode handling, Bluepad32 processing
  - [`main.c`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/main.c): ESP-IDF entry point and Bluepad32 glue
  - [`TinkerThinkerBoard.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/TinkerThinkerBoard.cpp): board facade and control arbitration
  - [`InputBindingManager.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/InputBindingManager.cpp): controller input mapping
  - [`WebServerManager.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/WebServerManager.cpp): web UI, WebSocket, config routes
  - [`ConfigManager.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/ConfigManager.cpp): persistent configuration in LittleFS
- [`data/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/data): web app files stored in LittleFS
- [`PCB/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/PCB): board files and exports
- [`CAD/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/CAD): mechanical models

## Quick Start (PlatformIO)

This repository is primarily built with PlatformIO.

1. Open the project in VS Code with the PlatformIO extension.
2. Select `env:esp32dev`.
3. Build the firmware.
4. Upload the firmware.
5. Upload the LittleFS image so the web UI is available.
6. Open the serial monitor at `115200` baud if you want logs.

Windows PowerShell examples:

- Build: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev`
- Upload: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t upload`
- Upload FS: `& "C:\\Users\\mgabr\\.platformio\\penv\\Scripts\\pio.exe" run -e esp32dev -t uploadfs`

Important:

- The web UI will not work until the LittleFS image from `data/` has been uploaded.
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

This behavior is implemented in [`main/TinkerThinkerBoard.cpp`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/TinkerThinkerBoard.cpp).

## Releases and Flashing

The main CI workflow builds:

- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`
- `littlefs.bin`

On successful pushes to `main`, GitHub Actions creates a release tagged like `main-YYYYMMDD-HHMM-<shortsha>`.

You can flash release assets automatically with:

```bash
python tools/auto_flasher.py
```

The script downloads the latest release, computes offsets from [`platformio.ini`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/platformio.ini) and the partition CSV, and flashes the correct images.

## Configuration Reference

Persistent configuration is stored in `/config.json` on LittleFS.

Main config groups:

- Wi-Fi mode, SSIDs, passwords
- motor inversion and deadband
- GUI motor pair selection
- motor frequency
- servo pulse width limits
- LED count
- Bluetooth scan timing
- drive profile and motor curve
- `control_bindings`

The HTTP and WebSocket API is documented in [`API.md`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/API.md).

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
- The controller connects but the website becomes unreachable briefly:
  - this can happen for about 3 seconds because Wi-Fi is intentionally paused to improve Bluetooth pairing
- Motors move in the wrong direction:
  - correct them in `/setup` or `/config`
- The UI returns 404 or stays blank after flashing:
  - upload the LittleFS image

## Credits and Support

The board and software were created at FabLab Lubeck:

- Andre: hardware
- Marco: software

If you run into hardware or software problems, they are the right people to contact through the project or FabLab channels.

Project/community support:

- Discord: [TinkerTink support server](https://discord.gg/r5aMn6Cw5q)

## Further Reading

- [Bluepad32 for Arduino](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
- [Arduino as ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- [ESP-IDF VS Code extension](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/vscode-setup.html)

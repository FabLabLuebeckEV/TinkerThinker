# 🤖 TinkerThinker - All-in-One ESP32 Robotics Controller

[🇩🇪 Deutsch](README.md) | **🇬🇧 English**

Welcome to **TinkerThinker**! This repository contains the complete firmware, web UI, PCB documentation, 3D-printable CAD files, and flashing tools for the ESP32-based robotics controller board created at **FabLab Lübeck**.

Whether you are building a **workshop tracked robot**, a **Sumo bot**, or a **4-axis robot arm**, you can start driving and configuring it right away - **no coding required** - using your smartphone, laptop, or a Bluetooth gamepad.

---

## 🧭 Quick Navigation

| For Beginners & Workshops | Features & Hardware | Flashing, Building & API |
| :--- | :--- | :--- |
| 🚀 [Get Started in 60 Seconds](#get-started-in-60-seconds) | 🎮 [The 3 Built-In Robot UIs](#the-3-built-in-robot-interfaces) | ⚡ [Flash Firmware (No Coding Needed)](#installing-or-updating-firmware) |
| 📱 [Wi-Fi & Smartphone Control](#option-a-control-via-smartphone-tablet-or-pc-wi-fi) | 🦾 [Robot Arm & Current Sensors](#3-4-axis-robot-arm-arm---arm) | 🛠️ [For Developers (PlatformIO)](#for-developers-building-with-platformio) |
| 🎮 [Bluetooth Gamepad Pairing](#option-b-control-via-bluetooth-gamepad) | 🧭 [Guided Setup Wizard (`/setup`)](#guided-setup-wizard-setup) | 📡 [HTTP & WebSocket API (`API.md`)](API.md) |
| 💡 [Status LED & Buttons](#status-led--onboard-buttons) | 🔌 [Pinout & Hardware Overview](#hardware-connections--pinout) | 🖨️ [3D Printing (`CAD/`)](CAD/README.md) & [PCB (`PCB/`)](PCB/README.md) |
| ❓ [Troubleshooting & FAQ](#troubleshooting--faq) | 🔄 [Factory Reset](#factory-reset) | 💬 [Support & Discord](#credits--support) |

---

## ✨ What Can TinkerThinker Do? (At a Glance)

- **🔌 4 DC Motor Outputs (`Motor A` to `Motor D`)**:
  - Ideal for tracked vehicles, 4WD robots, grippers, or 4-axis robot arms.
  - **`Motor A` and `Motor B` feature built-in current sensors** (measuring live motor current in Amps - used for automatic stall/endstop calibration on the robot arm!).
- **⚙️ Up to 7 Servo Outputs (`Servo 1` to `Servo 7`)**:
  - 7 servos on current **v2 boards** (or 3 servos on older **v1 legacy boards**, switchable with one click).
- **🌈 Addressable RGB LEDs (WS2812 / NeoPixel)**:
  - Onboard status LED + header for external LED strips (live color & range control in the browser).
- **🎮 Dual Wireless Control**:
  - **Wi-Fi Web UI**: Built-in Wi-Fi access point with responsive touch controls - **no app installation required**.
  - **Bluetooth Gamepads (Bluepad32)**: Supports PS3, PS4, PS5, Xbox Wireless, Nintendo Switch Pro, 8BitDo, and affordable **no-name PS3 clone controllers**.
- **🧠 Smart Built-In Extras**:
  - **Smartphone Connection Assistant (Captive Portal)** with automatic device detection (Apple iOS vs. Android vs. PC) and countdown timer to help users switch out of the OS mini-browser before it times out.
  - **Reachable via `http://tinker.local` (mDNS)** or `http://192.168.4.1`.
  - **Interactive Setup Wizard (`/setup`)** with numbered diagrams and wrong-port error detection.
  - **Visual Controller Editor (`/controls`)** with live gamepad & board graphics for custom button mapping.

---

## 🚀 Get Started in 60 Seconds

### 1. Power On
1. Flip the physical **power switch** on the board edge (next to the antenna housing) to **ON**.
2. The onboard **status LED** lights up **dim white** (= ready for both Wi-Fi and Bluetooth).

Now choose how you want to control your robot: **Smartphone/Browser (Option A)** or **Bluetooth Gamepad (Option B)** - or both at the same time!

---

### Option A: Control via Smartphone, Tablet, or PC (Wi-Fi)

1. **Connect to the Robot Wi-Fi**:
   - Open your device's Wi-Fi settings and connect to **`TinkerThinkerAP`** (or your custom robot name).
   - No password is required by default.
2. **Important on Smartphones - Switch to Your Full Browser**:
   - When connecting, phones automatically open a small **Captive Portal popup window**.
   - ⚠️ **Note**: On iOS and Android, this mini popup browser has a strict timeout (60-90 seconds) and closes itself when no internet is detected!
   - Our **Connection Assistant** automatically detects your device (Apple iPhone/iPad vs. Android) and guides you with a countdown to **keep the Wi-Fi connection without internet** and open the control page in **Safari or Chrome**.
3. **Open the Control Interface**:
   - Tap the button in the assistant or enter one of these addresses in your browser:
     - 👉 **[http://192.168.4.1](http://192.168.4.1)** *(works on all devices)*
     - 👉 **[http://tinker.local](http://tinker.local)** *(convenient mDNS address for iOS, macOS, Windows & PC)*

---

### Option B: Control via Bluetooth Gamepad

1. Turn on the board (LED is **dim white**).
2. Put your controller into **pairing mode**:
   - **PS4 / PS5 Controller**: Hold `SHARE` (or `CREATE`) + `PS button` simultaneously until the lightbar flashes rapidly.
   - **PS3 Controller (including clones)**: Turn on the controller unplugged by pressing the `PS button`.
   - **Xbox Wireless Controller**: Hold the small pairing button on the top edge for 3 seconds.
   - **Switch Pro / 8BitDo**: Hold the pairing button on the top edge.
3. Once connected, the onboard status LED turns **solid green** 🟢!

> **💡 Tip if pairing fails:**
> Press the **`MODE` button** twice until the status LED turns **blue** 🔵 (*Bluetooth Scan Only mode*). This dedicates 100% of the radio to continuous Bluetooth scanning.

---

## 💡 Status LED & Onboard Buttons

| LED Color | Radio Mode / State | Meaning |
| :--- | :--- | :--- |
| ⚪ **Dim White** | `Normal` (Default) | Both Wi-Fi hotspot **and** duty-cycled Bluetooth scanning are active. |
| 🟢 **Green** | `Controller Connected` | A Bluetooth gamepad is connected and ready to drive. |
| 🟠 **Orange** | `Wi-Fi Only` | Bluetooth scanning is paused; 100% radio airtime for Wi-Fi. |
| 🔵 **Blue** | `Bluetooth Scan Only` | Continuous Bluetooth scanning for fast controller pairing. |
| 🔴⚪🟠 **Cycling** | `Factory Reset` | `MODE` button is held for 10 seconds during startup. |

Press the **`MODE` button** briefly during operation to cycle between **White (Normal)** -> **Orange (Wi-Fi Only)** -> **Blue (Bluetooth Scan Only)**.

---

## 🎮 The 3 Built-In Robot Interfaces

Select your robot type in `/setup` or `/config` (`gui_mode`). The main route `/` automatically serves the matching dashboard:

### 1. 🚜 Workshop Robot (`basic` - `/basic`)
Clean, beginner-friendly interface for tracked and wheeled workshop robots:
- **Compact status bar**: Battery level (`%` + color bar), 4-bar Wi-Fi signal indicator, and a **1-click LED color picker** synced with the current board LED color.
- **Touch Joystick**: Labeled directional control (`Vorwärts`, `Rückwärts`, `Links`, `Rechts`).
- **Gripper Buttons**: Dedicated `Greifer auf` (Open) and `Greifer zu` (Close) touch buttons.

### 2. 🏎️ Advanced Drive Robot (`drive` - `/drive`)
Full telemetry dashboard for advanced builders and competitions:
- **Live Telemetry**: Battery voltage (`V`), percentage (`%`), and live **motor current (`A`)**.
- **Servo Sliders**: Direct angle sliders for all active servos (`Servo 1..7` on v2, `Servo 1..3` on v1).
- **Motor Test & Pair Swap**: Direct test buttons for `Motor A..D` plus 1-click drive pair swapping.
- **LED Strip Control**: Set color and active LED count.

### 3. 🦾 4-Axis Robot Arm (`arm` - `/arm`)
Dedicated interface with an **interactive 2D kinematic canvas** for the TinkerThinker robot arm:
- **Numbered Joints (`①` to `⑤`)**:
  - **`① Base (Turntable)`**: `360°` continuous rotation (**no current sensor**, on `Motor C` or `D`).
  - **`② Arm 1 (Lower Arm / Shoulder)`**: `180°` range (**⚡ with current sensor**, on `Motor A` or `B`).
  - **`③ Arm 2 (Middle Arm / Elbow)`**: `180°` range (**⚡ with current sensor**, on `Motor A` or `B`).
  - **`④ Arm 3 (Wrist / Upper Arm)`**: `180°` range from `+90°` (straight up) over the **left side** to `-90°` (straight down), avoiding the **yellow component on the right** (**no current sensor**, on `Motor C` or `D`).
  - **`⑤ Gripper (Servo)`**: Open/Close servo control with customizable angles.
- **⚡ Automatic Current-Sensor Homing (`Kalibrieren`)**:
  - Uses the current sensors on `Motor A` and `Motor B` to detect mechanical endstop stalls automatically, set the exact reference angle (`+90°` or `-90°`), and smoothly return to the `0°` home position.
- **Soft-Start / Soft-Stop Ramping & Dual Touch Joysticks**:
  - Smooth PWM ramping protects gears; manual controls continue sending power at estimated software limits so real physical movement is never blocked by estimation drift.

---

## 🛠️ Guided Setup Wizard (`/setup`)

Open **`http://192.168.4.1/setup`** for a step-by-step guided configuration:

1. **Choose Robot Type & Wi-Fi Name**: Select `Workshop-Roboter`, `Fahr-Roboter`, or `Roboter-Arm` and set your custom Wi-Fi SSID.
2. **1-Second Motor Identification**: Test each motor port (`1s` pulse) and click which part moved. Unused ports (`Kein Motor`) automatically skip direction and deadband steps.
3. **Numbered Robot Arm Wizard (`①` to `⑤`) with Wrong-Port Protection**:
   - Displays a persistent **numbered SVG diagram (`①` to `⑤`)** of the robot arm.
   - Starts with the **non-sensor ports (`Motor C` & `Motor D`)** for `① Base` and `④ Arm 3`, then tests the **current-sensor ports (`Motor A` & `Motor B`)** for `② Arm 1` and `③ Arm 2`.
   - **Wrong-Port Alert (`⚠️ Hey, Fehler!`)**: If a motor requiring a current sensor (`②` or `③`) is plugged into `Motor C/D` (or `①`/`④` is plugged into `Motor A/B`), the wizard alerts you immediately, explains which cables to swap, and restarts cleanly from the beginning.
   - Includes a live test step for **`⑤ Gripper Servo`** and saves the entire `arm_config` permanently to `/config.json` on the ESP32.

---

## 🔌 Hardware Connections & Pinout

| Board Port | Index | ESP32 GPIOs | Current Sensor? | Recommended Robot Arm Joint |
| :--- | :---: | :---: | :--- | :--- |
| **Motor A** | `M0` (`0`) | `GPIO 16` / `GPIO 25` | ✅ **Yes (`Sensor 2` on `GPIO 36`)** | **② Arm 1 (Lower Arm)** or **③ Arm 2 (Middle Arm)** |
| **Motor B** | `M1` (`1`) | `GPIO 32` / `GPIO 27` | ✅ **Yes (`Sensor 1` on `GPIO 34`)** | **③ Arm 2 (Middle Arm)** or **② Arm 1 (Lower Arm)** |
| **Motor C** | `M2` (`2`) | `GPIO 4` / `GPIO 12` | ❌ No | **① Base (Turntable)** or **④ Arm 3 (Wrist)** |
| **Motor D** | `M3` (`3`) | `GPIO 15` / `GPIO 14` | ❌ No | **④ Arm 3 (Wrist)** or **① Base (Turntable)** |

- **Servos**: `S0..S2` on `GPIO 13, 33, 17` (all boards); `S3..S6` on `GPIO 23, 18, 19, 5` (v2 boards only).
- **WS2812 LEDs**: `GPIO 2` · **Battery ADC**: `GPIO 35` · **GPIO 39**: `MODE` button (v2) / `GPIO_FAULT_1` (v1 legacy).

---

## ⚡ Installing or Updating Firmware

### 1. Browser WebSerial Installer (No Setup Required)
Open **[`tools/webserial_installer.html`](tools/webserial_installer.html)** in **Google Chrome** or **Microsoft Edge**, connect the board via USB, and click **Flash**.

### 2. One-Click Auto-Flasher (Windows & Workshops)
Run **[`tools/start_flasher.bat`](tools/start_flasher.bat)** (or `python tools/auto_flasher.py`). It automatically downloads the latest GitHub Actions release and flashes every connected ESP32 board. See **[`tools/README.md`](tools/README.md)** for details.

### 3. For Developers: Building with PlatformIO
```powershell
# Build firmware (v2 default):
pio run -e esp32dev

# Build Gzip-compressed LittleFS image:
pio run -e esp32dev -t buildfs

# Fast standalone LittleFS build & flash (<1s):
python tools/gzip_littlefs.py --build --flash COM19
```

---

## 💬 Credits & Support

Created at **FabLab Lübeck** by **Andre** (Hardware) and **Marco** (Software).
- 📡 **HTTP & WebSocket API Reference**: [`API.md`](API.md)
- 🧰 **Flashing Tools Documentation**: [`tools/README.md`](tools/README.md)
- 🖨️ **CAD & 3D Printing Files**: [`CAD/README.md`](CAD/README.md)
- 🔌 **PCB Documentation**: [`PCB/README.md`](PCB/README.md)
- 💬 **TinkerThinker Support Discord**: [discord.gg/kxpnWjJjyF](https://discord.gg/kxpnWjJjyF)

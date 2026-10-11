# TinkerThinker

[English](README.md) | Deutsch

ESP32-basierte Robotik-Controller-Firmware mit Bluetooth-Gamepad-Unterstützung, WLAN-Websteuerung, Motor- und Servosteuerung, WS2812-LEDs und einer über LittleFS (mit automatischer `.gz`-Komprimierung) ausgelieferten Setup-/Konfigurationsoberfläche.

Diese README beginnt aus Sicht von Endnutzern mit einer bereits geflashten Platine. Entwickler- und Build-Hinweise kommen weiter unten.

## Inhalt

- [Verwendung einer geflashten Platine](#verwendung-einer-geflashten-platine)
- [Standardsteuerung](#standardsteuerung)
- [Werksreset](#werksreset)
- [Webseiten](#webseiten)
- [Was das Projekt enthält](#was-das-projekt-enthält)
- [Hardware-Überblick (v2 Standard-Pins vs. alte v1-Platinen)](#hardware-überblick-v2-standard-pins-vs-alte-v1-platinen)
- [Projektstruktur](#projektstruktur)
- [Schnellstart (PlatformIO)](#schnellstart-platformio)
- [Bluetooth- und WLAN-Koexistenz](#bluetooth--und-wlan-koexistenz)
- [Steuerungs-Arbitration](#steuerungs-arbitration)
- [Releases und Flashen](#releases-und-flashen)
- [Konfigurationsreferenz](#konfigurationsreferenz)
- [Fehlersuche](#fehlersuche)
- [Credits und Support](#credits-und-support)
- [Weiterführende Links](#weiterführende-links)

## Verwendung einer geflashten Platine

### Einschalten

1. Schalte die Platine über den physischen Ein-/Ausschalter an der Kante neben der Antennenabdeckung ein.
2. Die Status-LED sollte danach schwach weiß leuchten. Schwach weiß bedeutet, dass die Platine eingeschaltet ist und sich im Funkmodus `Normal` befindet.

Ab hier hast du zwei Hauptmöglichkeiten:

- einen Bluetooth-Controller verbinden
- dich per WLAN mit Handy oder Laptop verbinden und die Weboberfläche nutzen

### Option 1: Bluetooth-Controller

1. Versetze den Controller in den Pairing-Modus.
2. Warte, bis die Platine eine neue Bluetooth-Verbindung annimmt.
3. Sobald ein Controller verbunden ist, wechselt die Status-LED auf grün.

Hinweise:

- Im Normalmodus wird das Bluetooth-Scannen getaktet, damit WLAN ebenfalls Sendezeit bekommt.
- Wenn das Pairing unzuverlässig ist, schalte mit dem MODE-Taster in den Bluetooth-Scan-Modus. In diesem Modus leuchtet die LED blau und das Scannen bleibt dauerhaft aktiv.
- Die Pairing-Keys werden bei jedem Boot gelöscht, deshalb ist erneutes Koppeln in der Regel unkompliziert.

### Option 2: Steuerung über WLAN

1. Suche auf deinem Handy oder Laptop nach dem Hotspot der Platine. Je nach gespeicherter Konfiguration heißt dieser typischerweise `TinkerThinker` oder `TinkerThinkerAP`.
2. Verbinde dich mit diesem WLAN.
3. Wenn dein Gerät meldet, dass das Netzwerk kein Internet hat, bleibe trotzdem verbunden.
4. Öffne [http://192.168.4.1](http://192.168.4.1) im Browser.
5. Danach sollte die Steuerungsoberfläche erscheinen und du kannst den Roboter steuern und konfigurieren.

Wichtige Hinweise für Handy und Netzwerk:

- Deaktiviere VPNs vor dem Verbinden.
- Manche Handys wechseln bei fehlendem Internet automatisch zurück in ein anderes WLAN oder auf mobile Daten. Falls das passiert, verbinde dich erneut mit dem Roboter-WLAN und bleibe darauf.
- Wenn die Seite nicht aufgeht, prüfe, ob dein Browser `http://192.168.4.1` stillschweigend zu `https://192.168.4.1` umgeschrieben hat.
- Ein anderer Browser hilft oft.
- Auf manchen Handys hilft es, mobile Daten auszuschalten. Bei anderen hilft das Gegenteil, deshalb lohnt sich beides auszuprobieren.

### Controller-Belegung ändern

Die Weboberfläche dient nicht nur zum Fahren. Dort kannst du auch die Bluetooth-Controller-Belegung und weitere Laufzeitoptionen ändern, ohne die Firmware neu zu flashen.

## Standardsteuerung

Die Standard-Belegung für Bluetooth ist in [`main/ConfigManager.cpp`](main/ConfigManager.cpp) definiert und in [`data/config.json`](data/config.json) gespiegelt.

Standardmäßig gilt:

- Der rechte Stick (`RX` / `RY`) steuert das konfigurierte GUI-Motorpaar.
- Der linke Stick (`X` / `Y`) steuert das andere Motorpaar.
- D-Pad rechts bewegt Servo 0 um `+10°`.
- D-Pad links bewegt Servo 0 um `-10°`.
- `R2` schaltet Servo 0 zwischen `0°` und `90°` um.
- `L2` schaltet Servo 0 zwischen `90°` und `180°` um.
- `R1` erhöht den globalen Geschwindigkeitsfaktor.
- `L1` verringert den globalen Geschwindigkeitsfaktor.

Details zur Motorzuordnung:

- Das konfigurierbare "GUI-Motorpaar" wird als `motor_left_gui` und `motor_right_gui` gespeichert.
- In der ausgelieferten Standardkonfiguration ist dieses Paar Motor `2` und Motor `3`.
- Dadurch steuert der rechte Stick standardmäßig die Motor-Kanäle `C/D`, während der linke Stick das verbleibende Paar `A/B` steuert.
- Das kann später in `/config` oder über den Setup-Assistenten unter `/setup` geändert werden.

## Werksreset

Wenn du die Konfiguration geändert hast, ein Passwort vergessen hast oder zu einem bekannten Ausgangszustand zurück möchtest, gehe so vor:

1. Halte den MODE-Taster an der Seite der Platine gedrückt.
2. Während du MODE gedrückt hältst, drücke den Reset-Taster kurz.
3. Halte MODE während des Starts etwa 10 Sekunden weiter gedrückt. Während dieser Zeit wechselt die Status-LED zwischen Rot, Weiß und Orange.
4. Nach Abschluss des Resets wird die Konfiguration gelöscht und die Platine startet neu.

Nach einem Werksreset:

- heißt das WLAN wieder `TinkerThinkerAP`
- sind gespeicherte WLAN- und Controller-Einstellungen wieder auf Standard gesetzt

## Webseiten

### `/` (Steuerungstab - umschaltbar über `gui_mode`)

Die Hauptroute `/` liefert die in `/config` oder `/setup` ausgewählte Steuerungsoberfläche (`gui_mode`):

- **`basic` (Standard - Workshop-Roboter, `/basic`)**:
  - Kompakte obere Statuszeile mit Akkubalken, 4-Balken-Funksignal-Anzeige (`wifiRssi`) und Einzel-LED-Farbwähler (LED 0)
  - Touch-Joystick zum Fahren mit Richtungsbeschriftung (`Vorwärts`, `Links`, `Rechts`, `Rückwärts`)
  - 2 Touch-Buttons `Greifer auf` und `Greifer zu`, verknüpft mit dem konfigurierten Greifer-Motor (`motor_gripper_gui`)
- **`drive` (Fahr-Roboter Komplettansicht, `/drive`)**:
  - Live-Anzeige für Batterie, Motorströme und Servos
  - Touch-Joystick
  - Servo-Slider
  - Motor-Testbuttons und Greifer-Steuerung
  - LED-Farb- und Bereichssteuerung
- **`arm` (Roboterarm, `/arm`)**:
  - Eigene Steuerseite für Roboterarm-Aufbauten

### `/config`

Konfigurationsseite für:

- Steuerungs-Oberfläche (`gui_mode`: `basic`, `drive`, `arm`)
- Hardware-Schalter für alte Platinen-Revision v1 (`legacy_board`)
- WLAN-AP/STA-Einstellungen
- Hotspot-Name und Passwort
- Motor-Invertierung, Deadband und PWM-Frequenz
- GUI-Fahrpaar (`motor_left_gui`, `motor_right_gui`) und Greifer-Motor (`motor_gripper_gui`)
- LED-Anzahl
- Servo-Pulsbreiten
- Bluetooth-Scan-Timings
- Fahrprofil-Parameter

### `/controls`

Editor für Controller-Eingaben und Aktionen:

- Buttons, D-Pad-Richtungen und Stick-Achsen
- Unabhängige Bluetooth-Joystick-Richtung (`bt_invert_x`, `bt_invert_y`, `bt_swap_axes`)
- Aktionen für Motoren, Servos, LEDs, GPIO und Geschwindigkeitsfaktor
- Live-Testregler für LEDs, Motoren und aktive Servos (3 bei v1-Boards, 7 bei v2-Boards)

### `/setup`

Geführte Ersteinrichtung für:

- Auswahl der Steuerungs-Oberfläche (`basic`, `drive`, `arm`) und des WLAN-Namens
- Motor-Rollenzuordnung (`Kettenmotor Links`, `Kettenmotor Rechts`, `Greifer Motor`, `Kein Motor`)
- Automatisches Überspringen: Motoren mit Auswahl `Kein Motor` überspringen Drehrichtungs- und Anlaufstrom-Schritte
- Korrektur der Drehrichtung und Deadband-Abgleich für aktive Motoren
- Automatische Speicherung von `motor_left_gui`, `motor_right_gui` und `motor_gripper_gui`

## Was das Projekt enthält

- Bluepad32-Gamepad-Unterstützung über Bluetooth Classic
- 4 DC-Motor-Ausgänge
- Bis zu 7 Servo-Ausgänge (7 auf v2-Platinen, 3 auf alten v1-Platinen)
- WS2812-LED-Steuerung
- WLAN-AP/STA-Unterstützung
- Async-Weboberfläche aus LittleFS (automatisch als `.gz` komprimiert) mit umschaltbaren Steuerungstabs (`basic`, `drive`, `arm`)
- Batterie-Spannungs- und Strommessung
- Konfigurierbare Eingabebelegungen
- Laufzeit- und Compile-Time-Unterstützung für alte v1-Platinen (`legacy_board`)
- Last-Writer-Wins-Arbitration zwischen Websteuerung und Bluetooth-Steuerung

## Hardware-Überblick (v2 Standard-Pins vs. alte v1-Platinen)

- Motoren
  - `M0`: `pin1=16`, `pin2=25`
  - `M1`: `pin1=32`, `pin2=27`
  - `M2`: `pin1=4`, `pin2=12`
  - `M3`: `pin1=15`, `pin2=14`
- Servos
  - `S0`: `pin=13` (v1 & v2)
  - `S1`: `pin=33` (v1 & v2)
  - `S2`: `pin=17` (v1 & v2)
  - `S3`: `pin=23` (nur v2)
  - `S4`: `pin=18` (nur v2)
  - `S5`: `pin=19` (nur v2)
  - `S6`: `pin=5` (nur v2)
- WS2812-LED-Datenleitung: `pin=2`
- Batterie-ADC: `pin=35`
- Strommessung: `pin=34`, `pin=36`
- GPIO 39:
  - **v2-Platinen (`legacy_board: false`, Standard)**: `MODE`-Taster, aktiv-low (erfordert externen Pull-up)
  - **v1-Platinen (`legacy_board: true`)**: Motortreiber-Fehler-Eingang (`GPIO_FAULT_1`, `LOW` = OK, `HIGH` = Motortreiber-Fehler)

### Unterstützung für alte v1-Platinen (`legacy_board`)

Ältere v1-Platinen führen auf GPIO 39 das Fault-Signal des Motortreibers (im Normalbetrieb dauerhaft `LOW`) statt eines `MODE`-Tasters und besitzen 3 statt 7 Servo-Anschlüsse.

- **Laufzeit-Schalter**: In `/config` die Option **Älteres Board (v1)** aktivieren (oder `"legacy_board": true` in `data/config.json` setzen). Damit wird GPIO 39 als `GPIO_FAULT_1` ausgewertet, die `MODE`-Tasterabfrage deaktiviert und die Servo-Anzahl auf 3 (`13, 33, 17`) begrenzt.
- **Compile-Time-Environment**: Über `env:esp32dev_legacy` (`pio run -e esp32dev_legacy`) wird `LEGACY_BOARD_DEFAULT=true` direkt als Standard in die Firmware kompiliert.
- **Boot-Schutzlogik**: Selbst wenn eine v1-Platine mit v2-Standardkonfiguration (`legacy_board: false`) startet, erkennt die Boot-Routine ein dauerhaft auf `LOW` liegendes GPIO 39 (kein Taster-Release) und setzt den normalen Bootvorgang fort, statt in einer Werksreset-Schleife hängen zu bleiben.

## Projektstruktur

- [`main/`](main/)
  - [`sketch.cpp`](main/sketch.cpp): Arduino-Setup/Loop, Funkmodi, Legacy-Board-Handling, Bluepad32-Verarbeitung
  - [`main.c`](main/main.c): ESP-IDF-Einstiegspunkt und Bluepad32-Glue
  - [`TinkerThinkerBoard.*`](main/TinkerThinkerBoard.cpp): Board-Fassade und Steuerungs-Arbitration
  - [`InputBindingManager.*`](main/InputBindingManager.cpp): Controller-Mapping
  - [`WebServerManager.*`](main/WebServerManager.cpp): Web-UI, WebSocket, Konfigurationsrouten
  - [`ConfigManager.*`](main/ConfigManager.cpp): persistente Konfiguration in LittleFS
- [`data/`](data/): Web-App-Quelldateien (`basic.html`, `index.html`, `arm.html`, `config.html`, `setup.html`, `controls.html`), die beim Dateisystem-Build durch [`tools/gzip_littlefs.py`](tools/gzip_littlefs.py) automatisch als `.gz` in LittleFS gepackt werden
- [`PCB/`](PCB/): Platinen-Dateien und Exporte
- [`CAD/`](CAD/): mechanische Modelle und Sumo-Bausatz-Dateien
- [`tools/`](tools/): automatische Flasher-Skripte, Gzip-Pre-Build-Hook, WebSerial-Installer und CI-Hilfsmittel

## Schnellstart (PlatformIO)

Dieses Repository wird primär mit PlatformIO gebaut.

1. Öffne das Projekt in VS Code mit der PlatformIO-Erweiterung.
2. Wähle `env:esp32dev` (oder `env:esp32dev_legacy` für alte v1-Platinen).
3. Baue die Firmware.
4. Lade die Firmware hoch.
5. Lade danach das LittleFS-Image hoch, damit die Weboberfläche verfügbar ist.
6. Öffne bei Bedarf den seriellen Monitor mit `115200` Baud.

Windows-PowerShell-Beispiele:

- Build Firmware (v2 Standard): `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev`
- Build Firmware (v1 Legacy Standard): `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev_legacy`
- Upload Firmware: `$env:PYTHONIOENCODING="utf-8"; & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t upload`
- Build LittleFS (`.gz`-optimiert via PlatformIO): `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t buildfs`
- Upload LittleFS (`.gz`-optimiert via PlatformIO): `$env:PYTHONIOENCODING="utf-8"; & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t uploadfs`
- Schnelles Build & Flash nur für LittleFS (<1s ohne CMake-Overhead): `python tools/gzip_littlefs.py --build --flash COM19`

### Optimierter `.gz` LittleFS-Build (Web-UI komprimiert bauen)

Damit die Weboberfläche im Flash-Speicher minimalen Platz belegt und schneller über WLAN lädt, werden die Dateien aus [`data/`](data/) vor dem Erzeugen von `littlefs.bin` automatisch mit Gzip (Stufe 9) komprimiert:

- **Funktionsweise**:
  - In [`platformio.ini`](platformio.ini) ist `extra_scripts = pre:tools/gzip_littlefs.py` hinterlegt.
  - Das Skript [`tools/gzip_littlefs.py`](tools/gzip_littlefs.py) kopiert vor dem LittleFS-Packvorgang alle Dateien aus `data/` nach `.pio/data_gzip/` und komprimiert `.html`, `.js`, `.css`, `.ico` und `.svg` automatisch zu `.gz` (**ca. 63 % Platzersparnis**, von ~208 KB auf ~77 KB).
  - `config.json` (für Lese-/Schreibzugriffe des `ConfigManager` zur Laufzeit) sowie bereits komprimierte `.png`-Dateien bleiben unkomprimiert erhalten.
  - `ESPAsyncWebServer` erkennt die `.gz`-Dateien im LittleFS transparent und liefert sie direkt mit `Content-Encoding: gzip` an den Browser aus.
- **So baust und flashst du das optimierte `.gz`-Dateisystem**:
  1. **Schnellster Weg (Standalone-Skript, <1 Sekunde, ohne ESP-IDF CMake-Durchlauf)**:
     ```powershell
     # Nur .gz-optimiertes littlefs.bin bauen:
     python tools/gzip_littlefs.py --build

     # Direkt bauen und auf das Board (z. B. COM19) flashen:
     python tools/gzip_littlefs.py --build --flash COM19
     ```
  2. **Über PlatformIO CLI (`buildfs` / `uploadfs`)**:
     ```powershell
     # Baut .pio/build/esp32dev/littlefs.bin automatisch inklusive .gz-Komprimierung:
     & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t buildfs

     # Baut und flasht das .gz-optimierte Dateisystem via PlatformIO:
     $env:PYTHONIOENCODING="utf-8"
     & "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t uploadfs
     ```
  3. **In VS Code (PlatformIO GUI)**:
     - Unter **Project Tasks -> esp32dev -> Platform -> Build Filesystem Image** bzw. **Upload Filesystem Image** anklicken. Der Pre-Build-Hook `tools/gzip_littlefs.py` läuft dabei automatisch vor `mklittlefs`.

Wichtig:

- Die Weboberfläche funktioniert erst, nachdem das LittleFS-Image hochgeladen wurde.
- Alle Textdateien in `data/` müssen in sauberem UTF-8 (ohne BOM) gespeichert sein.
- Dieses Projekt nutzt Arduino als gemanagte ESP-IDF-Komponente. Füge kein zweites `components/arduino` hinzu.

## Bluetooth- und WLAN-Koexistenz

Der ESP32 nutzt für WLAN und Bluetooth dasselbe 2,4-GHz-Funkmodul. Deshalb taktet diese Firmware die Bluetooth-Suche, damit WLAN weiterhin Sendezeit bekommt.

Aktuelle Funkmodi:

- `Normal`: LED schwach weiß
- `Wi-Fi only`: LED orange
- `Bluetooth scan only`: LED blau
- Controller verbunden: LED grün

Mit dem MODE-Taster kann zwischen den Funkmodi gewechselt werden. Im Normalmodus wird WLAN außerdem kurz pausiert, wenn ein Controller verbunden wird, um das Bluetooth-Pairing stabiler zu machen.

## Steuerungs-Arbitration

Sowohl der Bluetooth-Controller als auch die Weboberfläche können Fahrbefehle senden.

Die Firmware verwendet dafür ein Last-Writer-Wins-Modell:

- die letzte nicht-neutrale Eingabequelle wird Besitzer der Steuerung
- eine neutrale Eingabe einer anderen Quelle hebt den aktiven Besitzer nicht auf

Dieses Verhalten ist in [`main/TinkerThinkerBoard.cpp`](main/TinkerThinkerBoard.cpp) implementiert.

## Releases und Flashen

Der Haupt-CI-Workflow baut:

- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`
- `littlefs.bin`

Bei erfolgreichen Pushes auf `main` erstellt GitHub Actions ein Release mit einem Tag wie `main-YYYYMMDD-HHMM-<shortsha>`.

### Flashen mit dem Auto-Flasher

Release-Artefakte können automatisch mit den Skripten in `tools/` geflasht werden:

- **Windows (Ein-Klick-Start):** Starte [`tools/start_flasher.bat`](tools/start_flasher.bat). Das Skript prüft automatisch auf `uv` (empfohlen) oder Python mit `.venv`, installiert Abhängigkeiten (`esptool`, `pyserial`) und startet den Flasher.
- **Python / CLI:**
  ```bash
  python tools/auto_flasher.py
  ```
  Oder mit `uv`:
  ```bash
  uv run --with-requirements tools/requirements.txt python tools/auto_flasher.py
  ```

Funktionen des Auto-Flashers:
- Lädt automatisch das neueste Release von GitHub Actions herunter.
- Berechnet Offsets dynamisch aus [`platformio.ini`](platformio.ini) und der Partitions-CSV.
- Überwacht serielle Ports fortlaufend: Sobald eine ESP32-Platine angesteckt wird, wird sie geflasht.
- Merkt sich geflashte Platinen per MAC in `tools/blacklist.txt` bezogen auf den Release-Tag. Bereits aktuelle Platinen werden übersprungen; sobald ein neuerer Release-Tag verfügbar ist, werden sie aktualisiert.
- Befehlszeilen-Optionen:
  - `--once` oder `-1`: Nur eine einzelne Platine flashen und beenden.
  - `--clear-blacklist` oder `-c`: Blacklist leeren, um Platinen erneut zu flashen.

### WebSerial-Installer (Browser)

Alternativ können Firmware und Konfigurationen direkt im Browser (Google Chrome oder Edge) über den WebSerial-Installer unter [`tools/webserial_installer.html`](tools/webserial_installer.html) geflasht werden.

## Konfigurationsreferenz

Die persistente Konfiguration liegt in `/config.json` auf LittleFS.

Wichtige Konfigurationsgruppen:

- Steuerungs-Oberfläche (`gui_mode`: `"basic"`, `"drive"`, `"arm"`)
- Legacy-Board-v1-Schalter (`legacy_board`)
- WLAN-Modus, SSIDs, Passwörter
- Motor-Invertierung und Deadband
- Auswahl des GUI-Motorpaars (`motor_left_gui`, `motor_right_gui`) und Greifer-Motors (`motor_gripper_gui`)
- Motor-Frequenz
- Servo-Pulsbreiten-Grenzen
- LED-Anzahl, Helligkeit und Gamma
- Bluetooth-Scan-Timing
- Fahrprofil und Motorkurve
- `control_bindings`

Die HTTP- und WebSocket-API ist in [`API.md`](API.md) dokumentiert.

## Fehlersuche

- Die Webseite geht nicht auf:
  - stelle sicher, dass du mit dem Roboter-Hotspot verbunden bist und nicht mit einem anderen WLAN
  - deaktiviere VPNs
  - verwende `http://192.168.4.1` und nicht `https://192.168.4.1`
  - probiere einen anderen Browser
  - auf Handys kann das Umschalten der mobilen Daten helfen
- Die Platine zeigt weißes Licht, aber der Controller verbindet sich nicht:
  - versetze den Controller erneut in den Pairing-Modus
  - schalte falls nötig in den Bluetooth-Scan-Modus, damit die LED blau wird
- **PS3 / DualShock 3 Clone-Controller** (günstige No-Name-Clones mit MAC-Präfix `A0:5A:5E` oder `A0:5A:5F`) werden mit spezieller Behandlung unterstützt:
  - die Firmware überspringt den Bluetooth-Name-Request (Clones antworten nicht darauf und verursachen einen 15-Sekunden-Timeout)
  - GAP Security Level 2 und Wii-PIN-Logik werden für diese Geräte deaktiviert
  - falls dein Clone einen anderen MAC-OUI hat und sich trotzdem nicht verbindet, erstelle ein Issue mit den ersten 3 Bytes der MAC-Adresse
- Der Controller verbindet sich, aber die Webseite ist kurz nicht erreichbar:
  - das kann für etwa 3 Sekunden passieren, weil WLAN absichtlich kurz pausiert wird, um das Bluetooth-Pairing zu verbessern
- Die Motoren drehen falsch herum:
  - korrigiere das in `/setup` oder `/config`
- Die UI liefert 404 oder bleibt nach dem Flashen leer:
  - lade das LittleFS-Image hoch

## Credits und Support

Die Platine und Software wurden im FabLab Lübeck erstellt:

- Andre: Hardware
- Marco: Software

Bei Problemen mit Hardware oder Software sind die beiden die richtigen Ansprechpartner über das Projekt oder die FabLab-Kanäle.

Projekt- und Platinen-Support:

- TinkerThinker / Platine / Hauptsoftware: [Projekt-Discord](https://discord.gg/kxpnWjJjyF)
- Bluepad32 / ausschließlich Bluetooth-Themen: [Bluepad32-Discord](https://discord.gg/r5aMn6Cw5q)

## Weiterführende Links

- [Bluepad32 for Arduino](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
- [Arduino as ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
- [ESP-IDF VS Code extension](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/vscode-setup.html)

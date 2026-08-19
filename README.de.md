# TinkerThinkerBL

[English](README.md) | Deutsch

ESP32-basierte Robotik-Controller-Firmware mit Bluetooth-Gamepad-Unterstützung, WLAN-Websteuerung, Motor- und Servosteuerung, WS2812-LEDs und einer über LittleFS ausgelieferten Setup-/Konfigurationsoberfläche.

Diese README beginnt aus Sicht von Endnutzern mit einer bereits geflashten Platine. Entwickler- und Build-Hinweise kommen weiter unten.

## Inhalt

- [Verwendung einer geflashten Platine](#verwendung-einer-geflashten-platine)
- [Standardsteuerung](#standardsteuerung)
- [Werksreset](#werksreset)
- [Webseiten](#webseiten)
- [Was das Projekt enthält](#was-das-projekt-enthält)
- [Hardware-Überblick (Standard-Pins)](#hardware-ueberblick-standard-pins)
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
2. Die Status-LED sollte danach schwach weiß leuchten.
3. Schwach weiß bedeutet, dass die Platine eingeschaltet ist und sich im Funkmodus `Normal` befindet.

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

1. Suche auf deinem Handy oder Laptop nach dem Hotspot der Platine.
2. Je nach gespeicherter Konfiguration heißt dieser typischerweise `TinkerThinker` oder `TinkerThinkerAP`.
3. Nach einem Werksreset heißt das WLAN wieder `TinkerThinkerAP`.
4. Verbinde dich mit diesem WLAN.
5. Wenn dein Gerät meldet, dass das Netzwerk kein Internet hat, bleibe trotzdem verbunden.
6. Öffne `http://192.168.4.1` im Browser.
7. Danach sollte die Steuerungsoberfläche erscheinen und du kannst den Roboter steuern und konfigurieren.

Wichtige Hinweise für Handy und Netzwerk:

- Deaktiviere VPNs vor dem Verbinden.
- Manche Handys wechseln bei fehlendem Internet automatisch zurück in ein anderes WLAN oder auf mobile Daten. Falls das passiert, verbinde dich erneut mit dem Roboter-WLAN und bleibe darauf.
- Wenn die Seite nicht aufgeht, prüfe, ob dein Browser `http://192.168.4.1` stillschweigend zu `https://192.168.4.1` umgeschrieben hat.
- Ein anderer Browser hilft oft.
- Auf manchen Handys hilft es, mobile Daten auszuschalten. Bei anderen hilft das Gegenteil, deshalb lohnt sich beides auszuprobieren.

### Controller-Belegung ändern

Die Weboberfläche dient nicht nur zum Fahren. Dort kannst du auch die Bluetooth-Controller-Belegung und weitere Laufzeitoptionen ändern, ohne die Firmware neu zu flashen.

## Standardsteuerung

Die Standard-Belegung für Bluetooth ist in [`main/ConfigManager.cpp`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/ConfigManager.cpp) definiert und in [`data/config.json`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/data/config.json) gespiegelt.

Standardmäßig gilt:

- Der rechte Stick (`RX` / `RY`) steuert das konfigurierte GUI-Motorpaar.
- Der linke Stick (`X` / `Y`) steuert das andere Motorpaar.
- D-Pad rechts bewegt Servo 0 um `+10`.
- D-Pad links bewegt Servo 0 um `-10`.
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
3. Halte MODE während des Starts etwa 10 Sekunden weiter gedrückt.
4. Während dieser Zeit wechselt die Status-LED zwischen Rot, Weiß und Orange.
5. Nach Abschluss des Resets wird die Konfiguration gelöscht und die Platine startet neu.

Nach einem Werksreset:

- heißt das WLAN wieder `TinkerThinkerAP`
- sind gespeicherte WLAN- und Controller-Einstellungen wieder auf Standard gesetzt

## Webseiten

### `/`

Haupt-Steuerseite mit:

- Live-Anzeige für Batterie und Motoren
- Touch-Joystick
- Servo-Slider
- Motor-Testbuttons
- LED-Farbsteuerung

### `/config`

Konfigurationsseite für:

- WLAN-AP/STA-Einstellungen
- Hotspot-Name und Passwort
- Motor-Invertierung, Deadband und PWM-Frequenz
- LED-Anzahl
- Servo-Pulsbreiten
- Bluetooth-Scan-Timings
- Fahrprofil-Parameter

### `/controls`

Editor für Controller-Eingaben und Aktionen:

- Buttons
- D-Pad-Richtungen
- Stick-Achsen
- Aktionen für Motoren, Servos, LEDs, GPIO und Geschwindigkeitsfaktor

### `/setup`

Geführte Ersteinrichtung für:

- Motor-Zuordnung
- Korrektur der Drehrichtung
- Deadband-Abgleich
- Auswahl des GUI-Fahrpaars

## Was das Projekt enthält

- Bluepad32-Gamepad-Unterstützung über Bluetooth Classic
- 4 DC-Motor-Ausgänge
- 3 Servo-Ausgänge
- WS2812-LED-Steuerung
- WLAN-AP/STA-Unterstützung
- Async-Weboberfläche aus LittleFS
- Batterie-Spannungs- und Strommessung
- konfigurierbare Eingabebelegungen
- Last-Writer-Wins-Arbitration zwischen Websteuerung und Bluetooth-Steuerung

## Hardware-Überblick (Standard-Pins)

- Motoren
  - `M0`: `pin1=16`, `pin2=25`
  - `M1`: `pin1=32`, `pin2=27`
  - `M2`: `pin1=4`, `pin2=12`
  - `M3`: `pin1=15`, `pin2=14`
- Servos
  - `S0`: `pin=13`
  - `S1`: `pin=33`
  - `S2`: `pin=17`
- WS2812-LED-Datenleitung: `pin=2`
- Batterie-ADC: `pin=35`
- Strommessung: `pin=34`, `pin=36`
- MODE-Taster: `pin=39`, aktiv-low

Wichtig:

- GPIO39 ist nur als Eingang nutzbar und hat keinen internen Pull-up.
- Die Hardware muss deshalb einen externen Pull-up für den MODE-Taster bereitstellen.

## Projektstruktur

- [`main/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main)
  - [`sketch.cpp`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/sketch.cpp): Arduino-Setup/Loop, Funkmodi, Bluepad32-Verarbeitung
  - [`main.c`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/main.c): ESP-IDF-Einstiegspunkt und Bluepad32-Glue
  - [`TinkerThinkerBoard.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/TinkerThinkerBoard.cpp): Board-Fassade und Steuerungs-Arbitration
  - [`InputBindingManager.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/InputBindingManager.cpp): Controller-Mapping
  - [`WebServerManager.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/WebServerManager.cpp): Web-UI, WebSocket, Konfigurationsrouten
  - [`ConfigManager.*`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/ConfigManager.cpp): persistente Konfiguration in LittleFS
- [`data/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/data): Web-App-Dateien in LittleFS
- [`PCB/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/PCB): Platinen-Dateien und Exporte
- [`CAD/`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/CAD): mechanische Modelle

## Schnellstart (PlatformIO)

Dieses Repository wird primär mit PlatformIO gebaut.

1. Öffne das Projekt in VS Code mit der PlatformIO-Erweiterung.
2. Wähle `env:esp32dev`.
3. Baue die Firmware.
4. Lade die Firmware hoch.
5. Lade danach das LittleFS-Image hoch, damit die Weboberfläche verfügbar ist.
6. Öffne bei Bedarf den seriellen Monitor mit `115200` Baud.

Windows-PowerShell-Beispiele:

- Build: `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev`
- Upload: `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t upload`
- Upload FS: `& "C:\Users\mgabr\.platformio\penv\Scripts\pio.exe" run -e esp32dev -t uploadfs`

Wichtig:

- Die Weboberfläche funktioniert erst, nachdem das LittleFS-Image aus `data/` hochgeladen wurde.
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

Dieses Verhalten ist in [`main/TinkerThinkerBoard.cpp`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/main/TinkerThinkerBoard.cpp) implementiert.

## Releases und Flashen

Der Haupt-CI-Workflow baut:

- `bootloader.bin`
- `partitions.bin`
- `firmware.bin`
- `littlefs.bin`

Bei erfolgreichen Pushes auf `main` erstellt GitHub Actions ein Release mit einem Tag wie `main-YYYYMMDD-HHMM-<shortsha>`.

Release-Artefakte können automatisch geflasht werden mit:

```bash
python tools/auto_flasher.py
```

Das Skript lädt das neueste Release herunter, berechnet die Offsets aus [`platformio.ini`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/platformio.ini) und der Partitions-CSV und schreibt die richtigen Images.

## Konfigurationsreferenz

Die persistente Konfiguration liegt in `/config.json` auf LittleFS.

Wichtige Konfigurationsgruppen:

- WLAN-Modus, SSIDs, Passwörter
- Motor-Invertierung und Deadband
- Auswahl des GUI-Motorpaars
- Motor-Frequenz
- Servo-Pulsbreiten-Grenzen
- LED-Anzahl
- Bluetooth-Scan-Timing
- Fahrprofil und Motorkurve
- `control_bindings`

Die HTTP- und WebSocket-API ist in [`API.md`](/mnt/c/Users/mgabr/Desktop/GitProjekte/TinkerThinkerBL/API.md) dokumentiert.

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
- **PS3 / DualShock 3 Clone-Controller** (günstige No-Name-Clones mit MAC-Prefix `A0:5A:5F`) werden mit spezieller Behandlung unterstützt:
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

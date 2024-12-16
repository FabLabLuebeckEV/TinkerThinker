# TinkerThinkerBoard - README

Dieses Repository enthält den Code für das TinkerThinkerBoard, eine selbst entwickelte PCB-Lösung mit Ansteuerung für Motoren, Servos, LEDs, Batteriemessung und einem Webinterface zur Konfiguration und Steuerung. Das Board ist auf einem ESP32 basierend, nutzt **LittleFS** zur Konfiguration und **Websockets** sowie einen kleinen Webserver, um die Einstellungen und Steuerungen bequem über den Browser vornehmen zu können.

## Überblick

Das TinkerThinkerBoard vereint folgende Komponenten und Funktionen:

1. **Motorsteuerung:**  
   Vier DC-Motoren können über PWM gesteuert werden. Frequenzen, Invertierung, Deadbands und Swap (Seitentausch) sind konfigurierbar.

2. **Servosteuerung:**  
   Bis zu drei Servos können in ihrem Winkel (0-180°) angesteuert werden. Min- und Max-Pulsewidth sind konfigurierbar.

3. **LED-Steuerung (WS2812):**  
   Eine Kette von adressierbaren LEDs (Standard-WS2812) kann angesteuert werden. Die Anzahl ist konfigurierbar, die Farben können direkt aus dem Code heraus oder später über das Interface festgelegt werden.

4. **Batterie- und Systemüberwachung:**  
   Über einen ADC-Eingang wird die Batteriespannung gemessen und in Prozent umgerechnet. Das System überwacht Motorströme (via H-Bridges) und erkennt Motor-Faults.

5. **Web-Interface:**  
   Über einen integrierten Webserver können sowohl die Konfigurationen vorgenommen als auch direkte Motor-, Servo- und LED-Steuerungen ausgeführt werden. Das Interface ermöglicht:
   - Änderung von WLAN-Einstellungen (AP/STA-Modus)
   - Konfiguration der Motoren (Invert, Deadband, Frequenz, Swap)
   - Konfiguration der Servo-Min/Max-Pulse
   - Konfiguration der LED-Anzahl
   - Aktueller Status: Batteriestand, Motordaten, Servo-Winkel, LED-Farben
   - (Noch geplant: Setup-Assistent, verschiedene Steuerungsmodi wie "Panzersteuerung", "Omniweels" etc.)

6. **OTA-Option (noch optional):**  
   Es kann über die Einstellungen aktiviert werden, um Firmware-Updates Over-the-Air durchführen zu können.

## Softwareaufbau

- **`main.cpp`**: Beispielhaftes Setup mit `ConfigManager` und `TinkerThinkerBoard`-Initialisierung.
- **`TinkerThinkerBoard.*`**: Hauptklasse, in der alle Komponenten (MotorController, ServoController, LEDController, BatteryMonitor, SystemMonitor) zusammenlaufen. Stellt auch den WebServerManager bereit.
- **`ConfigManager.*`**: Lädt, speichert und verwaltet die Gerätekonfiguration aus einer JSON-Datei auf dem Flash (LittleFS).
- **`MotorController.*`**, **`ServoController.*`**, **`LEDController.*`**, **`BatteryMonitor.*`**, **`SystemMonitor.*`**: Klassen zur Hardwareansteuerung.
- **`WebServerManager.*`**: Startet den WLAN-Modus (AP oder STA), den Webserver und stellt Endpunkte und WebSocket-Handlings bereit.
- **`/data`-Verzeichnis** (mit `index.html`, `config.html`, `setup.html`, CSS und JS): Statische Dateien, die über den integrierten Webserver ausgeliefert werden.

## Erste Schritte

1. **Hardware aufbauen**:  
   - Motoren, Servos und WS2812-LED-Kette anschließen.
   - Batterie einstecken.  
   Bitte die jeweiligen Pins im Code prüfen (z. B. in `TinkerThinkerBoard.cpp`) und entsprechend verdrahten.

2. **Software kompilieren und flashen**:  
   Das Projekt kann in der Arduino-IDE oder PlatformIO kompiliert werden. Stellen Sie sicher, dass `LittleFS` korrekt eingebunden ist (ESP32 LittleFS Plugin).  
   Danach den Sketch auf den ESP32 flashen.  
   Anschließend über `ESP32 Data Upload` die Inhalte aus dem `data/`-Ordner auf den ESP32 hochladen.

3. **Erstaufruf**:  
   Im Auslieferungszustand ist der WLAN-Modus auf AP gesetzt.  
   - Verbinden Sie sich mit dem Hotspot, z. B. `TinkerThinkerAP` (Passwort siehe Default im `ConfigManager.cpp` oder `config.json`).
   - Rufen Sie im Browser `http://192.168.4.1` auf (Standard-AP-IP des ESP32).
   
   Nun sehen Sie die Startseite (Dashboard).

## Webinterface Benutzung

### Dashboard (`index.html`)

Hier können Sie den aktuellen Status ablesen:  
- Batteriespannung und -Prozent  
- Servo-Winkel  
- Motor-Informationen (PWM, Strom, Fault-Zustand)  
- LED-Farbe der ersten LED

**Steuerung:**
- Joystick (Touch/Mouse) zur Motorsteuerung (X/Y-Achsen)
- Servo-Slider, um den Servo-Winkel einzustellen
- Direkte Buttons für einen Beispiel-Motor (Vorwärts/Rückwärts/Stopp)

*Beispiel-Screenshot (hier wird später ein Bild eingefügt)*

### Konfiguration (`config.html`)

Hier können sämtliche Einstellungen angepasst werden:
- WLAN-Modus (AP oder STA), SSID, Passwort  
- Hotspot-Settings  
- Motor-Einstellungen (Invert, Deadband, Frequenz, Swap)  
- LED-Anzahl  
- OTA aktivieren/deaktivieren  
- Servoeinstellungen (Min/Max Pulsewidth)
  
Nach Änderungen auf "Speichern" klicken. Bei manchen Änderungen (insbesondere WLAN) startet das Board neu.

*Beispiel-Screenshot (hier wird später ein Bild eingefügt)*

### Setup-Assistent (`setup.html`)

Dieses Feature ist noch in Arbeit. Ziel ist es, den Nutzer Schritt für Schritt durch die Konfiguration zu führen. Hier sollen auch Motorentests, Strommessungen und verschiedene Steuerungsmodi konfiguriert werden.

*Beispiel-Screenshot (hier wird später ein Bild eingefügt)*

## Anpassungen

- Die Parameter für die Motor- und Servo-Pins sowie LED-Datenpin liegen im Code (z. B. `TinkerThinkerBoard.cpp`, `LEDController.cpp`).
- Änderungen an der Kalibrierung (z. B. Batteriespannung, Servo-Pulse-Bereiche) können im `ConfigManager` oder in den jeweiligen Klassen vorgenommen werden.

## Zukünftige Features / TODOs

- Motoren als Stepper konfigurierbar machen.
- Motoren so lange laufen lassen, bis ein bestimmter Strom erreicht ist (Lastabschätzung).
- Mehrere Weboberflächen / Modi für die Steuerung hinzufügen (Panzersteuerung, Omniwheels, etc.).
- Vollständiger Setup-Assistent mit Schritt-für-Schritt-Dialogen.
- OTA-Update-Funktion voll integrieren (UI für Firmware-Upload).
- Erweiterte Logging- und Diagnose-Seiten.

## Fehlersuche & Support

- Wenn das Webinterface nicht erreichbar ist, prüfen Sie, ob:
  - Der ESP32 korrekt im WLAN hängt oder der AP-Modus aktiv ist.
  - Die IP-Adresse im Browser richtig eingegeben wurde.
  - Die Dateien auf LittleFS korrekt hochgeladen wurden.
- Über die serielle Konsole (115200 Baud) können Debug-Ausgaben gelesen werden.

Bei weiteren Fragen oder Problemen bitte ein Issue auf GitHub erstellen oder direkt Kontakt aufnehmen.
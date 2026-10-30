# 🤖 TinkerThinker - Das All-in-One Robotik-Board

**🇩🇪 Deutsch** | [🇬🇧 English](README.en.md)

Willkommen bei **TinkerThinker**! Dieses Repository enthält die komplette Firmware, Weboberfläche, Platinen-Unterlagen (PCB), 3D-Druck-Modelle (CAD) und Flash-Werkzeuge für das ESP32-basierte Robotik-Steuerungsboard aus dem **FabLab Lübeck**.

Egal ob **Workshop-Kettenroboter**, **Sumo-Bot** oder **4-Achs-Roboter-Arm**: Du kannst sofort loslegen - **ganz ohne Programmierkenntnisse** direkt über dein Smartphone, Tablet, deinen Laptop oder ein Bluetooth-Gamepad!

---

## 🧭 Schnellnavigation

| Für Einsteiger & Workshops | Funktionen & Hardware | Flashen, Bauen & Technik |
| :--- | :--- | :--- |
| 🚀 [In 60 Sekunden starten](#in-60-sekunden-starten) | 🎮 [Die 3 Roboter-Oberflächen](#die-3-integrierten-roboter-oberflächen) | ⚡ [Firmware einfach flashen (ohne Code)](#firmware-installieren-oder-aktualisieren) |
| 📱 [WLAN & Smartphone-Verbindung](#option-a-steuerung-per-smartphone-tablet-oder-pc-wlan) | 🦾 [Roboter-Arm & Stromsensoren](#3-4-achs-roboter-arm-arm---arm) | 🛠️ [Für Entwickler (PlatformIO)](#für-entwickler-bauen-mit-platformio) |
| 🎮 [Bluetooth-Gamepad koppeln](#option-b-steuerung-per-bluetooth-gamepad) | 🧭 [Einrichtungs-Assistent (`/setup`)](#geführter-einrichtungs-assistent-setup) | 📡 [HTTP- & WebSocket-API (`API.md`)](API.md) |
| 💡 [Status-LED & Tasten erklärt](#status-led--tasten-auf-der-platine) | 🔌 [Anschlüsse & Pinbelegung](#hardware-anschlüsse--pinbelegung) | 🖨️ [3D-Druck (`CAD/`)](CAD/README.md) & [Platine (`PCB/`)](PCB/README.md) |
| ❓ [Hilfe bei Problemen (FAQ)](#häufige-fragen--problemlösungen-faq) | 🔄 [Werksreset durchführen](#werksreset-auf-werkseinstellungen-zurücksetzen) | 💬 [Support & Discord](#credits--support) |

---

## ✨ Was kann das TinkerThinker-Board? (Auf einen Blick)

- **🔌 4 DC-Motor-Ausgänge (`Motor A` bis `Motor D`)**:
  - Perfekt für Kettenfahrzeuge, Allrad-Roboter, Greifer oder 4-Achs-Roboterarme.
  - **`Motor A` und `Motor B` besitzen integrierte Stromsensoren** (messen live den Motorstrom in Ampere - ideal für automatische Endanschlag-Erkennung beim Roboter-Arm!).
- **⚙️ Bis zu 7 Servo-Anschlüsse (`Servo 1` bis `Servo 7`)**:
  - 7 Servos auf aktuellen **v2-Platinen** (bzw. 3 Servos auf älteren **v1-Platinen**, umschaltbar per Klick).
- **🌈 Adressierbare RGB-LEDs (WS2812 / NeoPixel)**:
  - Status-LED direkt auf dem Board + Anschluss für eigene LED-Streifen (Farbe & Effekte live im Browser wählbar).
- **🎮 Kabellose Steuerung auf zwei Wegen gleichzeitig**:
  - **WLAN-Weboberfläche**: Eigenes Roboter-WLAN mit moderner Touch-Steuerung im Browser - **keine App-Installation nötig**.
  - **Bluetooth-Gamepads (Bluepad32)**: Unterstützt PS3, PS4, PS5, Xbox Wireless, Nintendo Switch Pro, 8BitDo sowie günstige **No-Name PS3-Clone-Controller**.
- **🧠 Clevere Extras ab Werk**:
  - **Smartphone-Verbindungs-Assistent (Captive Portal)** mit Geräteerkennung (Apple iPhone/iPad, Android, PC) und Countdown, damit das Handy nicht nach 1 Minute die Verbindung trennt.
  - **Erreichbar über `http://tinker.local` (mDNS)** oder `http://192.168.4.1`.
  - **Interaktiver Einrichtungs-Assistent (`/setup`)** mit Bildern und Fehlermeldung bei falsch gesteckten Kabeln.
  - **Visueller Gamepad-Editor (`/controls`)** mit Live-Controller-Grafik zum freien Umbelegen aller Tasten und Joysticks.

---

## 🚀 In 60 Sekunden starten

### 1. Platine einschalten
1. Schieb den physischen **Ein-/Ausschalter** am Platinenrand (neben dem Antennengehäuse) auf **EIN**.
2. Die **Status-LED** auf dem Board leuchtet **schwach weiß** (= Board ist bereit für WLAN und Bluetooth).

Jetzt kannst du frei wählen: **Steuerung per Smartphone/Browser (Option A)** oder **per Bluetooth-Controller (Option B)** - oder beides gleichzeitig!

---

### Option A: Steuerung per Smartphone, Tablet oder PC (WLAN)

1. **Mit dem Roboter-WLAN verbinden**:
   - Öffne die WLAN-Einstellungen deines Geräts und wähle das Netzwerk **`TinkerThinkerAP`** (oder deinen eigenen vergebenen Roboter-Namen).
   - Standardmäßig ist kein Passwort nötig.
2. **Automatische Landingpage (Captive Portal) & Browser-Wechsel**:
   - Sobald du dich mit dem WLAN verbindest, öffnet sich automatisch unsere **helle, freundliche Verbindungs-Landingpage (`/portal.html`)**.
   - Sie erkennt automatisch, ob du ein **🍎 Apple iPhone/iPad**, ein **🤖 Android-Handy** oder einen **💻 PC/Laptop** nutzt, und zeigt dir **nur die für dein Gerät wichtigen 2 Schritte** an.
3. **Steuerungsseite im echten Browser öffnen (mDNS & IP)**:
   - 👉 **[http://tinker.local](http://tinker.local)** *(bequemer mDNS-Name - oder `http://<dein-robotername>.local`)*
   - 👉 **[http://192.168.4.1](http://192.168.4.1)** *(funktioniert garantiert auf jedem Gerät)*

#### 🌐 Warum gibt es die Landingpage mit Countdown statt direkter Steuerung im Popup?
- Wenn sich ein Smartphone mit einem WLAN ohne Internet verbindet, öffnet das Betriebssystem (iOS / Android) zunächst nur einen eingeschränkten **Captive-Portal-Minibrowser**.
- **Das Problem der Handys**: Dieser Minibrowser hat ein eingebautes System-Zeitlimit! Nach ca. **60 bis 90 Sekunden** sagt das Handy sonst *„Keine Internetverbindung“*, schließt das Fenster einfach von selbst und wirft dich aus der Steuerung.
- **Unsere Lösung**:
  - Die **Landingpage (`/portal.html`)** zeigt dir oben einen **sichtbaren 60-Sekunden-Countdown** und eine maßgeschneiderte Kurzanleitung für dein Modell:
    - **🍎 Apple (iPhone / iPad)**: Oben rechts auf **„Abbrechen“** -> **„Ohne Internet verwenden“** tippen -> in **Safari** öffnen.
    - **🤖 Android**: Bei der Systemmeldung auf **„Ja, verbunden bleiben“** / **„WLAN weiter versuchen“** tippen -> in **Chrome** öffnen.
  - Sobald du im richtigen Browser (Safari / Chrome) bist, läuft die Steuerung **stundenlang ohne Abbruch**!
- **Verbindung bleibt bestehen**: Etwa 15 Sekunden nach dem Verbinden beantwortet der Roboter die Internet-Prüfungen von iOS, Android und Windows mit „Internet vorhanden“. Dadurch wechselt das Handy nicht von selbst ins Mobilfunknetz und trennt das Roboter-WLAN nicht. Bricht die Verbindung trotzdem kurz ab, verbinden sich alle Steuerungsseiten automatisch neu.

> **💡 Tipp, falls die Seite am Handy nicht lädt:**
> - Schalte für die Dauer des Spielens kurz die **Mobilen Daten** aus (manche Handys suchen sonst im Mobilfunknetz nach dem Roboter).
> - Achte darauf, dass im Browser **`http://`** (ohne `s`) steht, nicht `https://`.
> - Deaktiviere aktive VPN-Apps.

---

### Option B: Steuerung per Bluetooth-Gamepad

1. Schalte das Board ein (LED leuchtet **schwach weiß**).
2. Versetze deinen Controller in den **Pairing-Modus** (Kopplungsmodus):
   - **PS4 / PS5 Controller**: `SHARE` (bzw. `CREATE`) + `PS-Taste` gleichzeitig gedrückt halten, bis die Lichtleiste schnell blitzt.
   - **PS3 Controller (auch Clones)**: Controller ohne USB-Kabel über die `PS-Taste` einschalten.
   - **Xbox Wireless Controller**: Kleine Pairing-Taste an der Stirnseite 3 Sekunden gedrückt halten.
   - **Switch Pro / 8BitDo**: Pairing-Taste oben am Controller gedrückt halten.
3. Nach wenigen Sekunden verbindet sich der Controller automatisch und die Status-LED auf dem Board leuchtet **dauerhaft grün** 🟢!

> **💡 Profi-Tipp bei schwierigem Pairing:**
> Halte auf dem Board die **MODE-Taste** zweimal jeweils ca. 1 Sekunde gedrückt (Weiß -> Orange -> Blau), bis die Status-LED **blau** 🔵 leuchtet (*Bluetooth-Scan-Modus*). Dann sucht das Board ununterbrochen mit voller Leistung nach Controllern.

---

## 💡 Status-LED & Tasten auf der Platine

Auf der Platine befinden sich zwei kleine Tasten (**`RESET`** und **`MODE`**) sowie die **Status-LED**, die dir jederzeit zeigt, was das Board gerade macht:

| LED-Farbe | Funkmodus / Zustand | Bedeutung |
| :--- | :--- | :--- |
| ⚪ **Schwach Weiß** | `Normal` (Standard) | WLAN-Hotspot **und** Bluetooth-Suche sind gleichzeitig aktiv. |
| 🟢 **Grün** | `Controller verbunden` | Ein Bluetooth-Gamepad ist aktiv gekoppelt und steuerbereit (Anzeige im Modus `Normal`). |
| 🟠 **Orange** | `Nur WLAN` | Bluetooth ist pausiert, 100 % Funkleistung für die Weboberfläche. |
| 🔵 **Blau** | `Nur Bluetooth-Scan` | Durchgehende Bluetooth-Suche zum schnellen Koppeln neuer Controller. |
| 🔴⚪🟠 **Wechselblinken** | `Werksreset läuft` | `MODE`-Taste wird beim Start für 10 Sekunden gedrückt gehalten. |

Halte die **`MODE`-Taste** im Betrieb ca. **1 Sekunde** gedrückt, um den Funkmodus zu wechseln. Die Reihenfolge ist **Weiß (Normal)** -> **Orange (Nur WLAN)** -> **Blau (Nur Bluetooth)** -> **Orange (Nur WLAN)** -> **Weiß (Normal)**. Auf v1-Platinen gibt es keine `MODE`-Taste, dort bleibt das Board immer im Modus `Normal`.

---

## 🎮 Die 3 integrierten Roboter-Oberflächen

Über das Setup (`/setup`) oder die Einstellungen (`/config`) kannst du auswählen, welchen Roboter-Typ du gebaut hast. Die Startseite `/` passt sich dann automatisch an:

### 1. 🚜 Workshop-Roboter (`basic` - `/basic`)
Die ideale, übersichtliche Oberfläche für Einsteiger, Kettenroboter und Fahr-Workshops:
- **Kompakte Statusleiste**: Zeigt Akkustand (`%` + Farbbalken), WLAN-Empfangsstärke (4 Balken) und einen **LED-Farbwähler**, der automatisch die aktuell auf dem Board leuchtende Farbe anzeigt.
- **Großer Touch-Joystick**: Intuitives Fahren (`Vorwärts`, `Rückwärts`, `Links`, `Rechts`).
- **Greifer-Steuerung**: Zwei große Touch-Tasten **`Greifer auf`** und **`Greifer zu`**.

### 2. 🏎️ Fahr-Roboter Erweitert (`drive` - `/drive`)
Das volle Cockpit für Fortgeschrittene, Tüftler und Wettbewerbs-Roboter:
- **Live-Telemetrie**: Echtzeit-Anzeige von Akkuspannung (`V`), Ladezustand (`%`) und **Motorstrom (`A`)**.
- **Servo-Steuerung**: Schieberegler für `Servo 1`. Regler für alle aktiven Servos (`Servo 1..7` auf v2, `Servo 1..3` auf v1) findest du im Steuerungs-Editor (`/controls`).
- **Einzelmotor-Tasten**: `Motor A` und `Motor B` fahren vor oder zurück, solange du die Taste gedrückt hältst.
- **Joystick-Richtung**: Achsen tauschen sowie X/Y invertieren, nur für die Website (unabhängig vom Bluetooth-Controller).
- **LED-Streifen-Steuerung**: Farbe und LED-Bereich der WS2812-LEDs live einstellen.

### 3. 🦾 4-Achs-Roboter-Arm (`arm` - `/arm`)
Speziell entwickelte Oberfläche mit **interaktiver 2D-Live-Simulation** für den TinkerThinker-Roboterarm:
- **Nummern-System (`①` bis `⑤`)**:
  - **`① Basis (Drehkranz)`**: `360°` Endlos-Drehung (**ohne Stromsensor**, an `Motor C` oder `D`).
  - **`② Arm 1 (Unterarm / Schulter)`**: `180°` Schwenkbereich (**⚡ mit Stromsensor**, an `Motor A` oder `B`).
  - **`③ Arm 2 (Mittelarm / Ellbogen)`**: `180°` Schwenkbereich (**⚡ mit Stromsensor**, an `Motor A` oder `B`).
  - **`④ Arm 3 (Handgelenk / Oberarm)`**: `180°` Schwenkbereich von `+90°` (senkrecht oben) über die **linke Seite** nach `-90°` (unten), sodass das **gelbe Bauteil rechts niemals überfahren** wird (**ohne Stromsensor**, an `Motor C` oder `D`).
  - **`⑤ Greifer (Servo)`**: Öffnen und Schließen per Servo (`Servo 1..3`) mit einstellbaren Winkeln.
- **⚡ Automatische Stromsensor-Kalibrierung (`Kalibrieren`-Buttons)**:
  - Da DC-Motoren keine internen Winkel-Encoder haben, misst das Board über die **Stromsensoren an `Motor A` und `Motor B`** den Stromanstieg am mechanischen Endanschlag.
  - Ein Klick auf **`⚡ Alle kalibrieren`** fährt die Gelenke bis zum Anschlag, erkennt den Strom-Peak automatisch, stoppt sofort, setzt den exakten Winkel (`+90°` bzw. `-90°`) und fährt sanft in die **neutrale Startposition (`0°`)**!
- **Manuelle Steuerung ohne künstliche Blockade**:
  - Weil die grafische Winkelanzeige zwischen den Anschlägen eine zeitbasierte Schätzung ist, sendet die manuelle Steuerung (Joystick / `◀` `▶` Buttons) auch am geschätzten Limit weiter Strom an den Motor, bis der echte physische Anschlag erreicht ist.
- **Sanftes Anfahren & Abbremsen (Soft-Start / Soft-Stop)**:
  - Schont Zahnräder und Mechanik durch eine sanfte Beschleunigungsrampe.
- **Dual-Touch-Joysticks**: Zwei virtuelle Kreuzknüppel für gleichzeitiges, flüssiges Steuern aller 4 Achsen wie bei einem echten Bagger/Kran.

---

## 🧭 Alle Webseiten im Überblick

Wenn du mit dem Roboter-WLAN verbunden bist, erreichst du über die obere Menüleiste folgende Seiten:

| URL / Pfad | Name | Was kannst du hier machen? |
| :--- | :--- | :--- |
| **`/`** | **Steuerung** | Öffnet automatisch deine gewählte Roboter-Oberfläche (`/basic`, `/drive` oder `/arm`). |
| **`/setup`** | **Setup-Assistent** | Geführte Schritt-für-Schritt-Ersteinrichtung für Fahrmotoren oder den Roboter-Arm (inkl. Grafik ①-⑤). |
| **`/controls`** | **Steuerungs-Editor** | Visuelles Live-Abbild von Bluetooth-Gamepad & Board zum Testen und freien Umbelegen aller Tasten/Sticks. |
| **`/config`** | **Konfiguration** | Alle Experten-Einstellungen (WLAN-Name/Passwort, v1/v2-Board, Motor-PWM, Servos, Projekt- & GitHub-Infos). |
| **`/portal.html`** | **Verbindungs-Hilfe** | Freundlicher Smartphone-Assistent mit Apple/Android-Erkennung zum Wechsel in den richtigen Browser. |

---

## 🛠️ Geführter Einrichtungs-Assistent (`/setup`)

Du weißt nicht genau, welches Motorkabel an welchem Anschluss (`A`, `B`, `C`, `D`) steckt oder warum ein Motor rückwärts läuft? Öffne einfach **`http://192.168.4.1/setup`**:

1. **Roboter-Typ & WLAN-Name wählen**: Wähle `Workshop-Roboter`, `Fahr-Roboter` oder `Roboter-Arm` und gib deinem Roboter einen eigenen WLAN-Namen (z. B. `Roboter-Max`).
2. **Motoren einzeln antesten (`1s`)**:
   - Du klickst auf **„Motor testen (1s)“**, beobachtest am echten Roboter, welches Teil sich bewegt hat, und klickst den passenden Button an.
   - Nicht belegte Anschlüsse (*„Kein Motor“*) überspringen automatisch alle weiteren Fragen.
3. **Drehrichtung & Anlauf-Schwelle (Deadband)**:
   - Der Assistent fragt dich in klaren Worten nach der beobachteten Richtung (z. B. *„Vorwärts“* vs. *„Rückwärts“*) und kehrt falsch gepolte Motoren automatisch um.
4. **Besonderheit beim Roboter-Arm (`①` bis `⑤` mit Kabel-Fehlerprüfung)**:
   - Oben siehst du dauerhaft eine **nummerierte Grafik des Roboter-Arms (`①` bis `⑤`)**.
   - Der Assistent startet gezielt mit **`Motor C` und `Motor D` (OHNE Stromsensor)** für **`① Basis`** und **`④ Arm 3 (Handgelenk)`** und testet danach **`Motor A` und `Motor B` (⚡ MIT Stromsensor)** für **`② Arm 1`** und **`③ Arm 2`**.
   - **Automatische Fehler-Erkennung (*„⚠️ Hey, Fehler!“*)**: Hast du versehentlich einen Armteil mit Stromsensor-Bedarf (`②` oder `③`) an `Motor C/D` angeschlossen (oder `①`/`④` an `Motor A/B`), stoppt der Assistent sofort, erklärt dir genau, welche zwei Stecker du am Board tauschen musst, und lässt dich sauber von vorne starten.
   - Zum Schluss kannst du in Schritt `⑤` direkt den **Greifer-Servo** (`Greifer Auf` / `Greifer Zu`) live testen.
   - Alle Einstellungen werden dauerhaft auf dem ESP32 gespeichert - jedes verbundene Handy hat sofort die fertige Konfiguration!

---

## 🎮 Visueller Steuerungs-Editor (`/controls`)

Unter **`http://192.168.4.1/controls`** findest du das visuelle Diagnose- und Mapping-Studio:

- **Live-Controller-Grafik & Board-Vorschau**:
  - Sobald du Tasten oder Joysticks an deinem gekoppelten Bluetooth-Gamepad bewegst, leuchten sie in Echtzeit auf dem Bildschirm auf.
- **Klick-Zuordnung (Tasten frei belegen)**:
  - Klicke einfach im Bild auf eine Controller-Taste (z. B. `R2`, `Steuerkreuz`, `Linker Stick`) und weise ihr eine Aktion zu (Motor fahren, Servo auf Winkel stellen, Servo schrittweise bewegen, LED-Farbe ändern oder Geschwindigkeits-Stufe schalten).
- **Joystick-Achsen anpassen**:
  - Vorwärts/Rückwärts oder Links/Rechts vertauscht? Mit den Schaltern `X invertieren`, `Y invertieren` und `Achsen tauschen` korrigierst du das mit einem Klick.
- **Integrierter Hardware-Live-Tester**:
  - Teste alle Servos, Motoren und LEDs direkt per Schieberegler über die Weboberfläche.

### Standard-Belegung für Bluetooth-Gamepads (ab Werk)

| Controller-Eingabe | Standard-Funktion |
| :--- | :--- |
| **Rechter Stick (`RX` / `RY`)** | Fährt das konfigurierte Haupt-Motorpaar (Standard: `Motor C & D`) |
| **Linker Stick (`X` / `Y`)** | Steuert das zweite Motorpaar (Standard: `Motor A & B`) |
| **Steuerkreuz Rechts / Links** | Fährt `Motor A` vorwärts / rückwärts, solange gedrückt |
| **Steuerkreuz Hoch / Runter** | Fährt `Motor B` vorwärts / rückwärts, solange gedrückt |
| **Trigger `R2` / `L2`** | Schaltet `Servo 1` zwischen `0°`/`90°` bzw. `90°`/`180°` um |
| **Schultertasten `R1` / `L1`** | Erhöht (`R1`) bzw. verringert (`L1`) die globale Fahrgeschwindigkeit |

---

## 🔌 Hardware-Anschlüsse & Pinbelegung

### Motoren & Stromsensoren

| Anschluss am Board | Interner Index | ESP32 GPIO-Pins | Stromsensor integriert? | Empfohlene Verwendung beim Roboter-Arm |
| :--- | :---: | :---: | :--- | :--- |
| **Motor A** | `M0` (`0`) | `GPIO 16` / `GPIO 25` | ✅ **Ja (`Sensor 2` an `GPIO 36`)** | **② Arm 1 (Unterarm)** oder **③ Arm 2 (Mittelarm)** |
| **Motor B** | `M1` (`1`) | `GPIO 32` / `GPIO 27` | ✅ **Ja (`Sensor 1` an `GPIO 34`)** | **③ Arm 2 (Mittelarm)** oder **② Arm 1 (Unterarm)** |
| **Motor C** | `M2` (`2`) | `GPIO 4` / `GPIO 12` | ❌ Nein | **① Basis (Drehkranz)** oder **④ Arm 3 (Handgelenk)** |
| **Motor D** | `M3` (`3`) | `GPIO 15` / `GPIO 14` | ❌ Nein | **④ Arm 3 (Handgelenk)** oder **① Basis (Drehkranz)** |

### Servos, LEDs & Sensorik (Platinen-Revision v2 vs. v1)

| Funktion | ESP32 GPIO | Hinweis (v2 Standard vs. ältere v1-Platine) |
| :--- | :---: | :--- |
| **Servo 1 bis 3 (`S0..S2`)** | `13`, `33`, `17` | Auf **allen** Platinen (v1 & v2) vorhanden (`Servo 1` = Greifer-Standard) |
| **Servo 4 bis 7 (`S3..S6`)** | `23`, `18`, `19`, `5` | Nur auf neueren **v2-Platinen** bestückt |
| **WS2812 RGB-LEDs** | `GPIO 2` | Onboard-Status-LED (`LED 0`) + externer LED-Streifen |
| **Akku-Spannungsmessung** | `GPIO 35` | Misst live die Versorgungsspannung (ADC) |
| **MODE-Taster / Fault-Pin** | `GPIO 39` | **v2-Board**: `MODE`-Taster · **v1-Board**: Motortreiber-Fault-Eingang |

> **🔄 Ältere v1-Platine im Einsatz?**
> Falls du eine ältere Platinen-Version (v1 mit nur 3 Servo-Steckplätzen) nutzt, kannst du in **`/config`** ganz oben einfach den Haken bei **„Älteres Board (v1 - 3 Servos, Fault-Pin auf GPIO 39)“** setzen. **Setze den Haken direkt nach dem ersten Flashen:** Auf einem v1-Board liegt `GPIO 39` dauerhaft auf LOW. Solange der Haken fehlt, wertet die Firmware das beim Einschalten wie eine gedrückte `MODE`-Taste: Die LED blinkt 10 Sekunden Rot-Weiß-Orange und alle Einstellungen werden auf Werkseinstellungen zurückgesetzt. Danach startet das Board normal weiter (keine Neustart-Schleife).

---

## 🔄 Werksreset (Auf Werkseinstellungen zurücksetzen)

Du hast das WLAN-Passwort vergessen oder möchtest alle Einstellungen komplett zurücksetzen?

1. Halte die **`MODE`-Taste** an der Seite der Platine gedrückt.
2. Drücke kurz auf die **`RESET`-Taste** (oder schalte das Board aus und wieder ein), **während du `MODE` weiter gedrückt hältst**.
3. Halte `MODE` für **ca. 10 Sekunden** gedrückt: Währenddessen blinkt die Status-LED abwechselnd **Rot - Weiß - Orange**.
4. Sobald der Reset abgeschlossen ist, startet das Board neu. Das WLAN heißt jetzt wieder **`TinkerThinkerAP`** (ohne Passwort).

---

## ⚡ Firmware installieren oder aktualisieren

Du möchtest ein neues Board bespielen oder ein Update aufspielen? Dafür gibt es **3 einfache Wege** - die ersten beiden funktionieren **komplett ohne Programmierumgebung**:

### Weg 1: Direkt im Browser mit dem WebSerial-Installer (Empfohlen für Einsteiger)
Du brauchst **weder Python noch VS Code**:
1. Öffne den **[Web-Flasher (GitHub Page)](https://fablabluebeckev.github.io/TinkerThinker/)** in **Google Chrome** oder **Microsoft Edge** (am Desktop). Er lädt automatisch das neueste Release. Alternativ funktioniert auch die Datei [`tools/webserial_installer.html`](tools/webserial_installer.html) lokal.
2. Verbinde dein TinkerThinker-Board per USB-Kabel mit dem Computer.
3. Klicke auf **„Latest Release laden“**, dann auf **„Latest Release flashen“** und wähle den USB-Port des Boards aus - der Installer spielt Bootloader, Partitionstabelle, Firmware und die komprimierte Weboberfläche (`littlefs.bin`) vollautomatisch auf!
   - *Bonus*: Über den Installer kannst du auch fertige Roboter-Konfigurationen sichern und auf weitere Boards klonen.

### Weg 2: Ein-Klick Auto-Flasher (Ideal für Workshops & Serien-Flashen)
Wenn du mehrere Boards hintereinander flashen möchtest:
- **Unter Windows**: Doppelklicke einfach auf **[`tools/start_flasher.bat`](tools/start_flasher.bat)**.
- **Per Terminal (macOS / Linux / Windows)**:
  ```bash
  python tools/auto_flasher.py
  ```
- **Was passiert automatisch?**
  - Das Skript lädt automatisch das **allerneueste Release** direkt von GitHub herunter.
  - Sobald du ein Board per USB ansteckst, wird es erkannt und komplett geflasht.
  - Bereits aktuell geflashte Boards werden anhand ihrer MAC-Adresse (`tools/blacklist.txt`) erkannt und übersprungen, bis ein neues Release erscheint.
  - Alle Details findest du in **[`tools/README.md`](tools/README.md)**.

---

## 🛠️ Für Entwickler: Bauen mit PlatformIO

Wenn du selbst am C++-Code (`main/`) oder an der Weboberfläche (`data/`) arbeitest:

### Projektstruktur
- **[`main/`](main/)**: ESP32-Firmware (ESP-IDF + Arduino-Komponente + Bluepad32)
  - [`sketch.cpp`](main/sketch.cpp): Hauptschleife, Funkmodi (`Normal`, `Wi-Fi only`, `BT scan`), mDNS & Reset-Logik
  - [`TinkerThinkerBoard.cpp`](main/TinkerThinkerBoard.cpp): Ansteuerung von Motoren, Servos, LEDs, Strom-/Akkusensoren & Last-Writer-Wins-Arbitration
  - [`WebServerManager.cpp`](main/WebServerManager.cpp): AsyncWebServer, WebSocket (`/ws`), Captive-Portal-Erkennung & REST-Endpunkte
  - [`ConfigManager.cpp`](main/ConfigManager.cpp): Speicherung aller Einstellungen (inkl. `arm_config` & `control_bindings`) in `/config.json` auf LittleFS
  - [`InputBindingManager.cpp`](main/InputBindingManager.cpp): Auswertung und Zuordnung der Bluetooth-Gamepad-Eingaben
- **[`data/`](data/)**: Weboberfläche (`basic.html`, `index.html`, `arm.html`, `setup.html`, `controls.html`, `config.html`, `portal.html`) - wird beim Bauen automatisch per Gzip komprimiert!
- **[`tools/`](tools/)**: Auto-Flasher, WebSerial-Installer und Gzip-Build-Skript ([`tools/README.md`](tools/README.md))
- **[`PCB/`](PCB/)**: Schaltpläne (V4.4), Platinen-Ansichten, 3D-Modell & Sicherheitshinweise ([`PCB/README.md`](PCB/README.md))
- **[`CAD/`](CAD/)**: 3D-Druck-Modelle für Chassis, Platinenhalter und Sumo-Roboter-Bausatz ([`CAD/README.md`](CAD/README.md))

### Wichtige Befehle (PlatformIO CLI)

```powershell
# 1. Firmware kompilieren (Standard v2-Board):
pio run -e esp32dev

# 2. Komprimiertes LittleFS-Dateisystem (.gz) bauen:
pio run -e esp32dev -t buildfs

# 3. Ultraschneller LittleFS-Build & Flash (< 1 Sekunde, ohne CMake-Wartezeit):
python tools/gzip_littlefs.py --build --flash COM19
```

> **📦 Automatische `.gz`-Komprimierung der Weboberfläche:**
> Vor jedem Dateisystem-Build komprimiert [`tools/gzip_littlefs.py`](tools/gzip_littlefs.py) alle `.html`, `.js`, `.css`, `.ico` und `.svg` Dateien aus `data/` nach `.pio/data_gzip/` (spart **~69 % Flash-Speicher** und beschleunigt das Laden im Browser drastisch). `config.json` bleibt unkomprimiert, damit das Board Einstellungen zur Laufzeit lesen und speichern kann.

---

## ❓ Häufige Fragen & Problemlösungen (FAQ)

<details>
<summary><strong>📱 Mein Handy schließt das Steuerungsfenster nach 1-2 Minuten von selbst!</strong></summary>

Das liegt am eingebauten **Captive-Portal-Minibrowser** von iOS und Android: Sobald das Handy merkt, dass der Roboter-Hotspot kein Internet hat, bricht das kleine Popup-Fenster nach einem Timeout ab.
**Lösung:** Folge einfach den 2 Schritten auf unserer Startseite (`/portal.html`): Wähle bei der Systemfrage *„Ohne Internet verbunden bleiben“* und öffne **`http://192.168.4.1`** oder **`http://tinker.local`** in deinem **echten Browser (Safari / Chrome)**. Dort bleibt die Verbindung dauerhaft stabil! Etwa 15 Sekunden nach dem Verbinden meldet der Roboter dem Handy außerdem „Internet vorhanden“, damit es das WLAN nicht von selbst verlässt.
</details>

<details>
<summary><strong>🌐 Die Seite <code>192.168.4.1</code> lädt im Browser nicht.</strong></summary>

1. Prüfe, ob dein Handy noch mit dem Roboter-WLAN verbunden ist.
2. Schalte am Handy testweise die **Mobilen Daten** aus und deaktiviere aktive **VPN-Apps**.
3. Stelle sicher, dass der Browser **`http://192.168.4.1`** (ohne `s`!) aufruft und nicht `https://`.
</details>

<details>
<summary><strong>🎮 Mein Bluetooth-Controller (oder PS3-Clone) verbindet sich nicht.</strong></summary>

1. Halte die **`MODE`-Taste** am Board zweimal jeweils ca. 1 Sekunde gedrückt, bis die Status-LED **blau** 🔵 leuchtet (dauerhafter Bluetooth-Scan-Modus).
2. Versetze den Controller erneut in den Pairing-Modus.
3. **Günstige PS3-/DualShock-3-Clone-Controller** (MAC-Präfix `A0:5A:5E` / `A0:5A:5F`) werden von unserer Firmware speziell unterstützt. Falls dein Clone eine andere MAC-Adresse hat, erstelle gern ein GitHub-Issue mit den ersten 3 Bytes der MAC-Adresse.
</details>

<details>
<summary><strong>🔄 Ein Motor dreht falsch herum oder die falschen Räder bewegen sich.</strong></summary>

Starte einfach den geführten Assistenten unter **`http://192.168.4.1/setup`** oder kehre die Drehrichtung einzelner Motoren per Klick unter **`http://192.168.4.1/config`** um. Du musst dafür keine Kabel umlöten!
</details>

---

## 💬 Credits & Support

Entwickelt mit ❤️ im **FabLab Lübeck**:
- **Andre**: Hardware & Platinen-Design (PCB)
- **Marco**: Firmware, Weboberfläche & Flash-Tools

### Weiterführende Dokumente & Community
- 📡 **API-Dokumentation (HTTP & WebSocket)**: [`API.md`](API.md)
- 🧰 **Flash-Tools & WebSerial-Installer**: [`tools/README.md`](tools/README.md)
- 🖨️ **CAD- & 3D-Druck-Dateien**: [`CAD/README.md`](CAD/README.md)
- 🔌 **Schaltplan & PCB-Unterlagen**: [`PCB/README.md`](PCB/README.md)
- 💬 **TinkerThinker / FabLab Support-Discord**: [discord.gg/kxpnWjJjyF](https://discord.gg/kxpnWjJjyF)
- 🎮 **Bluepad32 Discord (nur für allgemeine Bluetooth-Gamepad-Fragen)**: [discord.gg/r5aMn6Cw5q](https://discord.gg/r5aMn6Cw5q)

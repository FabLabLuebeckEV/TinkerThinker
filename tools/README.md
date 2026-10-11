# 🧰 TinkerThinker Flash- & Build-Tools

[⬅️ Zurück zur Hauptseite (`README.md`)](../README.md)

Dieses Verzeichnis enthält alle Werkzeuge zum schnellen Flashen von Platinen (im Browser oder vollautomatisch für Workshops), zur Gzip-Komprimierung der LittleFS-Weboberfläche und für die GitHub-Actions-CI.

## 📂 Übersicht der Tools

| Datei | Beschreibung |
| :--- | :--- |
| **`webserial_installer.html`** | 🌐 **Browser-Installer (Chrome / Edge)**: Flashen & Konfigurieren per USB ganz ohne Python oder VS Code. |
| **`start_flasher.bat`** | 🪟 **Ein-Klick-Starter für Windows**: Richtet Python/`uv` automatisch ein und startet den Auto-Flasher. |
| **`auto_flasher.py`** | ⚡ **Automatischer Serien-Flasher**: Lädt das neueste GitHub-Release und flasht jedes angesteckte Board automatisch. |
| **`auto_flasher_print.py`** | 🏷️ **Workshop-Flasher mit Etikettendruck**: Flasht das Board und druckt automatisch ein Label mit Namen/MAC/QR-Code. |
| **`gzip_littlefs.py`** | 📦 **LittleFS Gzip-Packer**: Komprimiert `data/` automatisch um ~69 % vor dem Dateisystem-Build (oder flasht in <1s). |
| **`webserial_release_bundle.js`** | 💾 **Offline-Bundle**: Enthält die aktuellen Binaries direkt für den Offline-Einsatz des WebSerial-Installers. |
| **`build_webserial_release_bundle.*`** | 🔄 Aktualisiert das Offline-Bundle aus dem neuesten GitHub-Release oder lokalen Build. |
| **`requirements.txt`** | 📋 Python-Abhängigkeiten (`esptool`, `pyserial`, `requests`, `tqdm`). |

---

## 1. 🌐 WebSerial-Installer im Browser (`webserial_installer.html`)

Der einfachste Weg für Einsteiger ohne jede Software-Installation:
1. Öffne **`webserial_installer.html`** in **Google Chrome**, **Microsoft Edge** oder **Opera**.
2. Schließe das TinkerThinker-Board per USB-Kabel an.
3. Klicke auf **„Verbinden & Flashen“**:
   - Der Installer spielt Bootloader, Partitionen, Firmware und die Weboberfläche (`littlefs.bin`) komplett in einem Rutsch auf.
   - Dank des mitgelieferten Offline-Bundles (`webserial_release_bundle.js`) funktioniert das auch ohne Internetverbindung im Workshop-Raum!
   - Zusätzlich kannst du über den Installer die Konfiguration eines fertig eingerichteten Roboters auslesen, als Datei speichern und auf beliebig viele weitere Boards klonen.

---

## 2. ⚡ Auto-Flasher für Workshops (`start_flasher.bat` / `auto_flasher.py`)

Der Auto-Flasher lädt automatisch das neueste Release (`bootloader.bin`, `partitions.bin`, `firmware.bin`, `littlefs.bin`) von GitHub Actions herunter und überwacht alle USB-Ports. Sobald du ein ESP32-Board ansteckst, wird es automatisch geflasht.

### Schnellstart unter Windows
Doppelklick auf **`start_flasher.bat`**.
- Nutzt automatisch `uv` (falls installiert) oder legt selbstständig ein lokales `.venv` mit allen Paketen aus `requirements.txt` an.

### Aufruf im Terminal (Windows / macOS / Linux)
```bash
# Mit uv (empfohlen):
uv run --with-requirements tools/requirements.txt python tools/auto_flasher.py

# Oder direkt mit Python:
python tools/auto_flasher.py
```

### Nützliche Parameter
- `--once` oder `-1`: Flasht genau ein angeschlossenes Board und beendet sich danach.
- `--clear-blacklist` oder `-c`: Löscht `tools/blacklist.txt`, um bereits geflashte Boards nochmals mit derselben Version zu bespielen.

### Wie funktioniert die MAC-Blacklist (`blacklist.txt`)?
Jedes erfolgreich geflashte Board wird mit seiner eindeutigen MAC-Adresse und dem aktuellen Release-Tag in `tools/blacklist.txt` vermerkt (z. B. `A0:B7:65:...=main-20261011-1528-d61be20`).
- Steckst du dasselbe Board versehentlich ein zweites Mal an, wird es übersprungen.
- Sobald ein **neues Release** auf GitHub erscheint, erkennt das Skript den neuen Tag und aktualisiert auch bekannte Boards automatisch!

---

## 3. 📦 Ultraschnelles LittleFS-Bauen & Flashen (`gzip_littlefs.py`)

Beim Bauen über PlatformIO (`pio run -e esp32dev -t buildfs`) wird `gzip_littlefs.py` automatisch als Pre-Build-Hook ausgeführt und komprimiert `.html`, `.js`, `.css`, `.ico` und `.svg` aus `data/` mit Gzip (Stufe 9) nach `.pio/data_gzip/`.

Wenn du nur Änderungen an HTML/JS/CSS in `data/` gemacht hast, kannst du das Dateisystem **ohne den langsamen ESP-IDF CMake-Durchlauf in unter 1 Sekunde** bauen und direkt flashen:

```bash
# Nur .gz-optimiertes littlefs.bin bauen:
python tools/gzip_littlefs.py --build

# Bauen und direkt auf COM19 flashen:
python tools/gzip_littlefs.py --build --flash COM19
```

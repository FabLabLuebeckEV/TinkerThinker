# TinkerThinker Tools

Dieses Verzeichnis enthält Hilfsskripte zum automatisierten Flashen, für den Workshop-Rollout, die LittleFS-Gzip-Komprimierung, den WebSerial-Installer und die CI.

## Übersicht

| Datei | Beschreibung |
|---|---|
| `start_flasher.bat` | Ein-Klick-Starter für Windows (nutzt `uv` oder Python `.venv`) |
| `auto_flasher.py` | Automatischer Flasher mit serieller Port-Überwachung und Blacklist |
| `auto_flasher_print.py` | Flasher mit automatischem Labeldruck für den Workshop |
| `gzip_littlefs.py` | Pre-Build-Hook & CLI-Tool zur `.gz`-Komprimierung der LittleFS-Webdateien |
| `webserial_installer.html` | Browser-basierter WebSerial-Installer (Chrome / Edge) |
| `webserial_release_bundle.js` | Gebündelte Release-Binaries für den Offline-WebSerial-Installer |
| `build_webserial_release_bundle.*` | Generierungsskripte für das WebSerial-Release-Bundle |
| `ci_patch_scons.py` | SCons-Patch für die GitHub-Actions-CI |
| `requirements.txt` | Python-Abhängigkeiten (`esptool`, `pyserial`, `requests`, `tqdm`) |

---

## 1. Auto-Flasher (`auto_flasher.py` / `start_flasher.bat`)

Der Auto-Flasher lädt die aktuellen Release-Binaries (`bootloader.bin`, `partitions.bin`, `firmware.bin`, `littlefs.bin`) von GitHub Actions herunter und wartet auf das Anstecken von ESP32-Boards. Sobald ein Board erkannt wird, wird es automatisch geflasht.

### Schnellstart unter Windows

Doppelklick auf `start_flasher.bat`.

Das Skript:
1. Sucht nach `uv`. Wenn vorhanden, wird `uv run --with-requirements requirements.txt` ausgeführt.
2. Falls kein `uv` vorhanden ist, wird ein lokales virtuelles Environment (`.venv`) angelegt und `pip install -r requirements.txt` ausgeführt.
3. Startet anschließend `auto_flasher.py`.

### Manueller Aufruf / CLI-Optionen

```bash
# Mit uv (empfohlen):
uv run --with-requirements tools/requirements.txt python tools/auto_flasher.py

# Mit Python:
python tools/auto_flasher.py
```

Optionale Parameter:
- `--once` oder `-1`: Flasht genau ein angeschlossenes Board und beendet sich danach.
- `--clear-blacklist` oder `-c`: Löscht die `blacklist.txt`, sodass bereits geflashte Boards erneut geflasht werden.

### Funktionsweise der Blacklist
Bereits geflashte Boards werden anhand ihrer MAC-Adresse in `blacklist.txt` zusammen mit dem Release-Tag gespeichert (z. B. `A0:B7:65:...=main-20261005-1552-2a12e052`). Ein Board wird nur übersprungen, wenn es bereits mit dem aktuellen Release-Tag geflasht wurde. Erscheint ein neueres Release auf GitHub, werden auch zuvor geflashte Boards automatisch aktualisiert.

---

## 2. LittleFS Gzip-Komprimierung (`gzip_littlefs.py`)

Komprimiert beim Bauen des LittleFS-Images (`pio run -t buildfs` / `uploadfs`) automatisch alle `.html`, `.js`, `.css`, `.ico` und `.svg` Dateien aus `data/` als `.gz` nach `.pio/data_gzip/`, während `config.json` und `.png`-Dateien unkomprimiert kopiert werden.

Für schnelles lokales Bauen/Flashen des Dateisystems (in unter 1 Sekunde ohne ESP-IDF-CMake-Durchlauf):

```bash
python tools/gzip_littlefs.py --build
python tools/gzip_littlefs.py --flash COM19
```

---

## 3. WebSerial-Installer (`webserial_installer.html`)

Ermöglicht das Flashen und Konfigurieren direkt im Browser ohne Python-Installation über die WebSerial-API (Google Chrome, Microsoft Edge, Opera).

- Öffne `webserial_installer.html` im Browser.
- Verbinde das Board über USB.
- Firmware und Dateisystem können per Mausklick installiert werden.
- Roboterkonfigurationen können exportiert, geklont und eingespielt werden.

---

## 4. Auto-Flasher mit Labeldruck (`auto_flasher_print.py`)

Für die Serienfertigung oder Workshops: Flasht das ESP32-Board und sendet nach erfolgreichem Flash-Vorgang automatisch einen Druckauftrag für ein Typenschild / Label (z. B. mit MAC-Adresse, Board-Name und QR-Code) an einen Netzwerk-Etikettendrucker.

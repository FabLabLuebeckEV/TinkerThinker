# TinkerThinker Tools

Dieses Verzeichnis enthaelt Hilfsskripte zum automatisierten Flashen, fuer den Workshop-Rollout, den WebSerial-Installer und die CI.

## Uebersicht

| Datei | Beschreibung |
|---|---|
| `start_flasher.bat` | Ein-Klick-Starter fuer Windows (nutzt `uv` oder Python `.venv`) |
| `auto_flasher.py` | Automatischer Flasher mit serieller Port-Ueberwachung und Blacklist |
| `auto_flasher_print.py` | Flasher mit automatischem Labeldruck fuer den Workshop |
| `webserial_installer.html` | Browser-basierter WebSerial-Installer (Chrome / Edge) |
| `webserial_release_bundle.js` | Gebuendelte Release-Binaries fuer den Offline-WebSerial-Installer |
| `build_webserial_release_bundle.*` | Generierungsskripte fuer das WebSerial-Release-Bundle |
| `ci_patch_scons.py` | SCons-Patch fuer die GitHub-Actions-CI |
| `requirements.txt` | Python-Abhaengigkeiten (`esptool`, `pyserial`, `requests`, `tqdm`) |

---

## 1. Auto-Flasher (`auto_flasher.py` / `start_flasher.bat`)

Der Auto-Flasher laedt die aktuellen Release-Binaries (`bootloader.bin`, `partitions.bin`, `firmware.bin`, `littlefs.bin`) von GitHub Actions herunter und wartet auf das Anstecken von ESP32-Boards. Sobald ein Board erkannt wird, wird es automatisch geflasht.

### Schnellstart unter Windows

Doppelklick auf `start_flasher.bat`.

Das Skript:
1. Sucht nach `uv`. Wenn vorhanden, wird `uv run --with-requirements requirements.txt` ausgefuehrt.
2. Falls kein `uv` vorhanden ist, wird ein lokales virtuelles Environment (`.venv`) angelegt und `pip install -r requirements.txt` ausgefuehrt.
3. Startet anschliessend `auto_flasher.py`.

### Manueller Aufruf / CLI-Optionen

```bash
# Mit uv (empfohlen):
uv run --with-requirements tools/requirements.txt python tools/auto_flasher.py

# Mit Python:
python tools/auto_flasher.py
```

Optionale Parameter:
- `--once` oder `-1`: Flasht genau ein angeschlossenes Board und beendet sich danach.
- `--clear-blacklist` oder `-c`: Loescht die `blacklist.txt`, sodass bereits geflashte Boards erneut geflasht werden.

### Funktionsweise der Blacklist
Bereits geflashte Boards werden anhand ihrer MAC-Adresse in `blacklist.txt` zusammen mit dem Release-Tag gespeichert (z. B. `A0:B7:65:...=main-20261005-1552-2a12e052`). Ein Board wird nur uebersprungen, wenn es bereits mit dem aktuellen Release-Tag geflasht wurde. Erscheint ein neueres Release auf GitHub, werden auch zuvor geflashte Boards automatisch aktualisiert.

---

## 2. WebSerial-Installer (`webserial_installer.html`)

Ermoeglicht das Flashen und Konfigurieren direkt im Browser ohne Python-Installation ueber die WebSerial-API (Google Chrome, Microsoft Edge, Opera).

- Oeffne `webserial_installer.html` im Browser.
- Verbinde das Board ueber USB.
- Firmware und Dateisystem koennen per Mausklick installiert werden.
- Roboterkonfigurationen koennen exportiert, geklont und eingespielt werden.

---

## 3. Auto-Flasher mit Labeldruck (`auto_flasher_print.py`)

Fuer die Serienfertigung oder Workshops: Flasht das ESP32-Board und sendet nach erfolgreichem Flash-Vorgang automatisch einen Druckauftrag fuer ein Typenschild / Label (z. B. mit MAC-Adresse, Board-Name und QR-Code) an einen Netzwerk-Etikettendrucker.

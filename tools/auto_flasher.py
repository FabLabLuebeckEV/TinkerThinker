# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "esptool>=5.0.0",
#     "pyserial>=3.5",
#     "requests>=2.28.0",
#     "tqdm>=4.65.0",
# ]
# ///
#!/usr/bin/env python3
import sys
import os
import time
import csv
import subprocess
from typing import Optional, Tuple, Dict, Set

# Ensure working directory is tools/ directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
os.chdir(SCRIPT_DIR)

try:
    import serial.tools.list_ports  # type: ignore
except Exception:
    serial = None

try:
    from tqdm import tqdm  # type: ignore
except Exception:
    tqdm = None

try:
    import requests
except Exception:
    requests = None


USER = 'FabLabLuebeckEV'
REPO = 'TinkerThinker'
GITHUB_API_URL = f'https://api.github.com/repos/{USER}/{REPO}/releases/latest'


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def read_partitions_csv_path() -> Optional[str]:
    """Return partitions CSV path from SCRIPT_DIR or CWD if present; otherwise None."""
    preferred = os.path.join(SCRIPT_DIR, 'partitions_dual3mb_1m5spiffs.csv')
    if os.path.isfile(preferred):
        return preferred
    try:
        for name in os.listdir(SCRIPT_DIR):
            lname = name.lower()
            if lname.endswith('.csv') and 'partitions' in lname:
                return os.path.join(SCRIPT_DIR, name)
    except Exception:
        pass
    return None


def parse_partitions_csv(csv_path: str) -> Dict[str, Dict[str, int]]:
    """Parse ESP32 CSV partition table. Return dict with keys for factory/ota_0/fs.
    Values contain {'offset': int, 'size': int} in bytes. Also includes 'fs_name'.
    """
    result: Dict[str, Dict[str, int]] = {}
    fs_name = None
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].strip().startswith('#'):
                continue
            try:
                name = row[0].strip()
                typ = row[1].strip().lower()
                subtype = row[2].strip().lower()
                offset_str = row[3].strip()
                size_str = row[4].strip()
            except Exception:
                continue

            def parse_hex_or_size(s: str) -> int:
                s = s.strip().lower()
                if s.startswith('0x'):
                    return int(s, 16)
                if s.endswith('m'):
                    return int(s[:-1]) * 1024 * 1024
                if s.endswith('k'):
                    return int(s[:-1]) * 1024
                return int(s)

            try:
                offset = parse_hex_or_size(offset_str)
                size = parse_hex_or_size(size_str)
            except Exception:
                continue

            if typ == 'app' and (name.lower() == 'factory' or subtype == 'ota_0'):
                result['app'] = {'offset': offset, 'size': size}
            if typ == 'data' and (subtype == 'spiffs' or subtype == 'littlefs'):
                result['fs'] = {'offset': offset, 'size': size}
                fs_name = subtype

    if fs_name and 'fs' in result:
        result['fs']['name'] = fs_name  # type: ignore
    return result


def download_release_assets() -> str:
    """Download latest release assets from GitHub into script directory. Returns release tag."""
    print("Checking for latest release on GitHub...")
    if requests is None:
        eprint("requests package not available. Skipping download.")
        return 'offline'

    try:
        r = requests.get(GITHUB_API_URL, timeout=15)
        r.raise_for_status()
        data = r.json()
        tag_name = data.get('tag_name', 'latest')
        release_title = data.get('name', tag_name)
        print(f"Latest release found: {tag_name} ({release_title})")

        assets = data.get('assets', [])
        if not assets:
            print("No assets found in latest release.")
            return tag_name

        for asset in assets:
            url = asset.get('browser_download_url')
            name = asset.get('name')
            size = int(asset.get('size', 0))
            if not url or not name:
                continue

            dest_path = os.path.join(SCRIPT_DIR, name)
            if os.path.isfile(dest_path) and os.path.getsize(dest_path) == size:
                print(f"  {name} is up to date.")
                continue

            print(f"  Downloading {name} ({size / 1024:.1f} KB)...")
            with requests.get(url, stream=True, timeout=60) as resp:
                resp.raise_for_status()
                bar = tqdm(total=size, unit='iB', unit_scale=True, unit_divisor=1024, desc=name) if tqdm else None
                with open(dest_path, 'wb') as f:
                    for chunk in resp.iter_content(chunk_size=64 * 1024):
                        if chunk:
                            f.write(chunk)
                            if bar:
                                bar.update(len(chunk))
                if bar:
                    bar.close()

        with open(os.path.join(SCRIPT_DIR, 'latest_tag.txt'), 'w', encoding='utf-8') as f:
            f.write(tag_name)
        print("Release assets downloaded successfully.\n")
        return tag_name

    except Exception as e:
        eprint(f"[WARNING] Could not check/download GitHub release: {e}")
        tag_file = os.path.join(SCRIPT_DIR, 'latest_tag.txt')
        if os.path.isfile(tag_file):
            try:
                with open(tag_file, 'r', encoding='utf-8') as f:
                    cached = f.read().strip()
                print(f"Using cached release version: {cached}\n")
                return cached
            except Exception:
                pass
        return 'local'


def find_image(preferred: str, contains_any: Tuple[str, ...]) -> Optional[str]:
    """Find image file in SCRIPT_DIR by preferred name or fuzzy contains match."""
    pref_path = os.path.join(SCRIPT_DIR, preferred)
    if os.path.isfile(pref_path):
        return pref_path
    try:
        candidates = []
        for name in os.listdir(SCRIPT_DIR):
            if not name.lower().endswith('.bin'):
                continue
            lname = name.lower()
            if any(tok in lname for tok in contains_any):
                candidates.append(os.path.join(SCRIPT_DIR, name))
        if not candidates:
            return None
        candidates.sort(key=lambda s: (len(os.path.basename(s)), s))
        return candidates[0]
    except Exception:
        return None


def load_blacklist() -> Dict[str, str]:
    """Return dict of {mac_lower: release_tag} from blacklist.txt."""
    bl: Dict[str, str] = {}
    bl_path = os.path.join(SCRIPT_DIR, 'blacklist.txt')
    if not os.path.isfile(bl_path):
        return bl
    try:
        with open(bl_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split(maxsplit=1)
                mac = parts[0].strip().lower()
                tag = parts[1].strip() if len(parts) > 1 else ''
                bl[mac] = tag
    except Exception as e:
        eprint(f"Notice: reading blacklist: {e}")
    return bl


def save_blacklist(bl: Dict[str, str]):
    """Persist blacklist to blacklist.txt."""
    bl_path = os.path.join(SCRIPT_DIR, 'blacklist.txt')
    try:
        with open(bl_path, 'w', encoding='utf-8') as f:
            for m, t in bl.items():
                if t:
                    f.write(f"{m} {t}\n")
                else:
                    f.write(f"{m}\n")
    except Exception as e:
        eprint(f"Notice: writing blacklist: {e}")


def is_blacklisted(mac: str, release_tag: str, bl: Dict[str, str]) -> bool:
    """Check if MAC is already blacklisted for the target release_tag."""
    mac_lower = mac.lower()
    if mac_lower not in bl:
        return False
    prev_tag = bl[mac_lower]
    # If previously flashed with the current release tag, skip
    if prev_tag and release_tag and prev_tag == release_tag:
        return True
    # If neither has a tag, treat as matching
    if not prev_tag and not release_tag:
        return True
    # If the tag is different, board should be updated!
    return False


def get_mac(port: str, retries: int = 2) -> str:
    """Read ESP32 MAC address with retry."""
    py = sys.executable
    for attempt in range(retries):
        try:
            res = subprocess.run(
                [py, '-m', 'esptool', '--chip', 'esp32', '--port', port, '--baud', '115200', 'read_mac'],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=12
            )
            for line in res.stdout.splitlines():
                if 'MAC:' in line:
                    return line.split()[-1].strip().lower()
        except Exception as e:
            eprint(f"Attempt {attempt + 1}: reading MAC on {port}: {e}")
        time.sleep(0.5)
    return 'UNKNOWN'


def program(port: str, release_tag: str, blacklist: Dict[str, str]) -> bool:
    """Flash a connected ESP32 board."""
    mac = get_mac(port)
    if mac == 'UNKNOWN':
        eprint(f"Could not read MAC on {port}. Board might still be booting or requires BOOT button.")
        return False

    print(f"Board detected on {port} with MAC: {mac}")
    if is_blacklisted(mac, release_tag, blacklist):
        print(f"Board {mac} is already flashed with '{release_tag}'. Skipping.")
        return False

    csv_path = read_partitions_csv_path()
    flash_size = '8MB'
    parts = parse_partitions_csv(csv_path) if csv_path else {}
    if not csv_path:
        print('No partitions CSV found; using default 8MB offsets.')

    bootloader_off = 0x1000
    parttable_off = 0x8000
    app_off = parts.get('app', {}).get('offset', 0x20000)
    fs_off = parts.get('fs', {}).get('offset', 0x620000)
    fs_size = parts.get('fs', {}).get('size', 0x180000)

    bootloader_img = find_image('bootloader.bin', ('bootloader',))
    partitions_img = find_image('partitions.bin', ('partitions', 'partition'))
    firmware_img = find_image('firmware.bin', ('firmware', 'app'))
    littlefs_img = find_image('littlefs.bin', ('littlefs', 'spiffs'))

    if not bootloader_img or not partitions_img or not firmware_img:
        eprint('Missing required binary images (bootloader / partitions / firmware).')
        return False
    if not littlefs_img:
        eprint('Missing littlefs image.')
        return False

    fs_bytes = os.path.getsize(littlefs_img)
    if fs_size and fs_bytes > fs_size:
        eprint(f'{littlefs_img} ({fs_bytes} bytes) exceeds partition size ({fs_size} bytes). Aborting.')
        return False

    py = sys.executable

    print(f"Erasing flash on {port}...")
    subprocess.run([py, '-m', 'esptool', '--chip', 'esp32', '--port', port, '--baud', '921600', 'erase_flash'], timeout=45, check=False)

    cmd = [
        py, '-m', 'esptool',
        '--chip', 'esp32',
        '--port', port,
        '--baud', '921600',
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash',
        '--flash_mode', 'dio',
        '--flash_size', flash_size,
        '--flash_freq', '40m',
        hex(bootloader_off), bootloader_img,
        hex(parttable_off), partitions_img,
        hex(app_off), firmware_img,
        hex(fs_off), littlefs_img
    ]
    print(f"Flashing {port} ({flash_size}, dio, 40m)...")
    res = subprocess.run(cmd)
    if res.returncode != 0:
        eprint(f"Flashing {port} failed at 921600 baud. Retrying at 460800 baud...")
        cmd[6] = '460800'
        res = subprocess.run(cmd)
        if res.returncode != 0:
            eprint(f"Flashing {port} failed.")
            return False

    # Mark as flashed
    blacklist[mac] = release_tag
    save_blacklist(blacklist)
    print("\n" + "=" * 60)
    print(f" [SUCCESS] Board ({mac}) on {port} flashed with '{release_tag}'!")
    print(" Please unplug this board and plug in the next board.")
    print("=" * 60 + "\n")
    return True


def scan_and_program_loop(release_tag: str, blacklist: Dict[str, str], once: bool = False):
    """Continuously monitor serial ports and flash detected boards."""
    if serial is None:
        eprint('pyserial not available. Install with: pip install pyserial')
        sys.exit(1)

    print("=" * 60)
    print(f" Auto-Flasher active. Target firmware: {release_tag}")
    print(" Connect an ESP32 board to flash automatically.")
    print(" (Press Ctrl+C to exit)")
    print("=" * 60 + "\n")

    handled_ports: Set[str] = set()

    while True:
        try:
            current_ports = {p.device: p for p in serial.tools.list_ports.comports()}

            # Detect disconnected devices
            for dev in list(handled_ports):
                if dev not in current_ports:
                    print(f"[{dev}] Board disconnected. Ready for next board...")
                    handled_ports.remove(dev)

            # Detect candidates
            for dev, p in current_ports.items():
                if dev in handled_ports:
                    continue

                info = f"{p.description or ''} {p.manufacturer or ''} {p.hwid or ''}".lower()
                is_candidate = any(tok in info for tok in [
                    'ch34', 'ch91', 'cp21', 'ftdi', 'usb serial', 'espressif', 'silicon labs', 'wch.cn', 'usb-to-uart'
                ])

                if is_candidate:
                    print(f"\nESP32 candidate detected: {dev} ({p.description})")
                    time.sleep(0.8)
                    success = program(dev, release_tag, blacklist)
                    handled_ports.add(dev)
                    if once and success:
                        print("Single flash mode completed.")
                        return

            time.sleep(1)
        except KeyboardInterrupt:
            print("\nAuto-flasher stopped by user.")
            break
        except Exception as e:
            eprint(f"Loop error: {e}")
            time.sleep(1)


def main():
    if '--clear-blacklist' in sys.argv or '-c' in sys.argv:
        bl_file = os.path.join(SCRIPT_DIR, 'blacklist.txt')
        if os.path.isfile(bl_file):
            os.remove(bl_file)
        print("Blacklist cleared.\n")

    blacklist = load_blacklist()
    release_tag = download_release_assets()

    once_mode = '--once' in sys.argv or '-1' in sys.argv
    scan_and_program_loop(release_tag, blacklist, once=once_mode)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
import sys
import os
import time
import csv
import requests
import subprocess
from typing import Optional, Tuple, Dict

try:
    import serial.tools.list_ports  # type: ignore
except Exception:
    serial = None  # optional

# Optional progress bar
try:
    from tqdm import tqdm  # type: ignore
except Exception:
    tqdm = None


USER = 'FabLabLuebeckEV'
REPO = 'TinkerThinker'
GITHUB_API_URL = f'https://api.github.com/repos/{USER}/{REPO}/releases/latest'


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def read_platformio_partitions() -> Tuple[str, str]:
    """Return (partitions_csv_path, flash_size_str) based on platformio.ini in repo root."""
    ini_path = os.path.join(os.getcwd(), 'platformio.ini')
    partitions = None
    flash_size = '8MB'
    if not os.path.isfile(ini_path):
        return ('partitions_dual3mb_1m5spiffs.csv', flash_size)
    try:
        with open(ini_path, 'r', encoding='utf-8') as f:
            for line in f:
                s = line.strip()
                if s.startswith('board_build.partitions'):
                    # board_build.partitions = <file>
                    parts = s.split('=', 1)
                    if len(parts) == 2:
                        partitions = parts[1].strip()
                elif s.startswith('board_build.flash_size'):
                    parts = s.split('=', 1)
                    if len(parts) == 2:
                        flash_size = parts[1].strip()
    except Exception:
        pass
    if not partitions:
        partitions = 'partitions_dual3mb_1m5spiffs.csv'
    return (partitions, flash_size)


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
            # Columns: Name, Type, SubType, Offset, Size, Flags
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
                # support like 1m, 512k
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

    if fs_name:
        result['fs']['name'] = fs_name  # type: ignore
    return result


def download_release_assets() -> None:
    """Download latest release assets from GitHub into current directory."""
    try:
        r = requests.get(GITHUB_API_URL, timeout=30)
        r.raise_for_status()
        data = r.json()
        if 'assets' not in data:
            print('No assets found in latest release')
            return
        for asset in data['assets']:
            url = asset.get('browser_download_url')
            name = asset.get('name')
            size = int(asset.get('size', 0))
            if not url or not name:
                continue
            print(f'Downloading {name} ...')
            with requests.get(url, stream=True, timeout=60) as resp:
                resp.raise_for_status()
                if tqdm:
                    bar = tqdm(total=size, unit='iB', unit_scale=True, unit_divisor=1024, desc=name)
                else:
                    bar = None
                with open(name, 'wb') as f:
                    for chunk in resp.iter_content(chunk_size=64 * 1024):
                        if not chunk:
                            continue
                        f.write(chunk)
                        if bar:
                            bar.update(len(chunk))
                if bar:
                    bar.close()
        print('Download completed.')
    except Exception as e:
        eprint(f'Error downloading release assets: {e}')


def ensure_esptool() -> str:
    """Return python -m esptool invocation path using current interpreter."""
    return sys.executable


def get_mac(port: str) -> str:
    py = ensure_esptool()
    try:
        res = subprocess.run([py, '-m', 'esptool', '--chip', 'esp32', '--port', port, 'read_mac'],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
        for line in res.stdout.splitlines():
            if 'MAC:' in line:
                return line.split()[-1].strip()
    except Exception as e:
        eprint(f'Failed to read MAC: {e}')
    return 'UNKNOWN'


def program(port: str) -> bool:
    partitions_csv, flash_size = read_platformio_partitions()
    csv_path = os.path.join(os.getcwd(), partitions_csv)
    parts = parse_partitions_csv(csv_path)

    bootloader_off = 0x1000
    parttable_off = 0x8000
    app_off = parts.get('app', {}).get('offset', 0x20000)
    fs_off = parts.get('fs', {}).get('offset')
    fs_size = parts.get('fs', {}).get('size', 0)

    if not fs_off:
        eprint('Filesystem partition not found in CSV. Abort.')
        return False

    # Basic sanity
    if not os.path.isfile('bootloader.bin') or not os.path.isfile('partitions.bin') or not os.path.isfile('firmware.bin'):
        eprint('Missing required images (bootloader.bin / partitions.bin / firmware.bin). Run download first.')
        return False
    if not os.path.isfile('littlefs.bin'):
        eprint('Missing littlefs.bin. If your build doesn\'t include FS, remove it from flashing or build it.')
        return False

    fs_bytes = os.path.getsize('littlefs.bin')
    if fs_size and fs_bytes > fs_size:
        eprint(f'littlefs.bin ({fs_bytes} bytes) is larger than partition size ({fs_size} bytes). Abort.')
        return False

    mac = get_mac(port)
    print(f'Using MAC: {mac}')
    try:
        with open('blacklist.txt', 'r') as bl:
            if mac in bl.read():
                print('MAC found in blacklist.txt. Aborting.')
                return False
    except Exception:
        pass

    py = ensure_esptool()

    # Optional: Erase flash
    subprocess.run([py, '-m', 'esptool', '--chip', 'esp32', '--port', port, '--baud', '921600', 'erase_flash'], check=False)

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
        hex(bootloader_off), 'bootloader.bin',
        hex(parttable_off), 'partitions.bin',
        hex(app_off), 'firmware.bin',
        hex(fs_off), 'littlefs.bin'
    ]
    print('Flashing with command:\n  ' + ' '.join(cmd))
    res = subprocess.run(cmd)
    if res.returncode != 0:
        eprint('Flashing failed.')
        return False

    try:
        with open('blacklist.txt', 'a') as bl:
            bl.write(mac + '\n')
    except Exception:
        pass
    print('Flashing completed.')
    return True


def scan_and_program_loop():
    if serial is None:
        eprint('pyserial not installed. Install with: pip install pyserial')
        sys.exit(1)
    print('Watching serial ports...')
    while True:
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            desc = (p.description or '').lower()
            if ('ch340' in desc) or ('cp210x' in desc) or ('cp210' in desc) or ('usb serial' in desc):
                print(f'ESP32 candidate: {p.device} - {p.description}')
                try:
                    if program(p.device):
                        print('Flash success')
                except Exception as e:
                    eprint(f'Error flashing {p.device}: {e}')
        time.sleep(1)


def main():
    # Ensure blacklist exists
    try:
        open('blacklist.txt', 'x').close()
    except Exception:
        pass
    download_release_assets()
    scan_and_program_loop()


if __name__ == '__main__':
    main()


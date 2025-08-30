import serial.tools.list_ports
import time
import esptool
import sys
import requests
from tqdm import tqdm
import subprocess

def get_mac_from_esptool(com_port):
    """ Liest die MAC-Adresse des ESP32 aus """
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool",  '--chip', 'esp32', "--port", com_port, "read_mac"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )

        print("Esptool output:\n", result.stdout)  # Debugging: Zeigt die komplette Ausgabe

        for line in result.stdout.split("\n"):
            if "MAC:" in line:
                mac = line.split()[-1].strip()
                print(f"Gefundene MAC: {mac}")  # Debugging: Gibt die gefundene MAC aus
                return mac

    except Exception as e:
        print(f"Fehler beim Abrufen der MAC: {e}")

    print("WARNUNG: Keine MAC-Adresse gefunden, gebe 'UNKNOWN' zurück!")
    return "UNKNOWN"  # Gibt einen Dummy-Wert zurück, um NoneType-Fehler zu verhindern


    print("WARNUNG: Keine MAC-Adresse gefunden, gebe 'UNKNOWN' zurück!")
    return "UNKNOWN"  # Gibt einen Dummy-Wert zurück, um NoneType-Fehler zu verhindern


# GitHub-Repo-Details
USER = 'FabLabLuebeckEV'
REPO = 'TinkerThinker'
GITHUB_API_URL = f'https://api.github.com/repos/{USER}/{REPO}/releases/latest'


def download_images():
    """ Lädt die neuesten Firmware-Dateien aus dem GitHub-Release herunter. """
    url = f'https://api.github.com/repos/{USER}/{REPO}/releases/latest'

    try:
        response = requests.get(url)
        response.raise_for_status()
        data = response.json()

        if 'assets' in data:
            for asset in data['assets']:
                download_url = asset['browser_download_url']
                file_name = asset['name']

                print(f"Downloading {file_name}...")
                response = requests.get(download_url, stream=True)

                with open(file_name, 'wb') as file, tqdm(
                    desc=file_name,
                    total=int(asset['size']),
                    unit='iB',
                    unit_scale=True,
                    unit_divisor=1024,
                ) as bar:
                    for chunk in response.iter_content(chunk_size=1024):
                        size = file.write(chunk)
                        bar.update(size)

            print("Download completed.")
        else:
            print("No assets found in the latest release.")
    except requests.RequestException as e:
        print(f"Error during request to GitHub API: {e}")



def program_esp32(com_port):
    mac = get_mac_from_esptool(com_port)
    print(f"Verwendete MAC: {mac}")

    if mac in open("blacklist.txt").read():
        print("MAC in blacklist.txt gefunden. Flashen wird abgebrochen.")
        return False

    # Erase Flash before writing
    esptool.main(['--chip', 'esp32', '--port', com_port, '--baud', '921600', 'erase_flash'])
    print(f"Flash von {com_port} gelöscht...")

    # Flash Bootloader, Partition Table, Firmware, and LittleFS
    esptool.main([
        '--chip', 'esp32',
        '--port', com_port,
        '--baud', '921600',
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash',
        '--flash_mode', 'dio',
        '--flash_size', '8MB',
        '--flash_freq', '40m',
        '0x1000', 'bootloader.bin',  # Bootloader
        '0x8000', 'partitions.bin',  # Partition Table
        '0x10000', 'firmware.bin',  # Main Firmware
        '0x290000', 'littlefs.bin'  # LittleFS File System
    ])

    print("Firmware wurde erfolgreich hochgeladen.")

    with open("blacklist.txt", "a") as f:
        f.write(mac + "\n")
        print(f"MAC {mac} wurde in blacklist.txt geschrieben.")

    return True


def scan_ports():
    print("Scanne Ports...")
    ports = serial.tools.list_ports.comports()
    for port in ports:
        print(f"Port: {port.device} - {port.description}")
        if 'USB-SERIAL CH340' in port.description or "Silicon Labs CP210x USB to UART Bridge" in port.description or "USB Serial Port" in port.description:
            print(f"ESP32 erkannt auf: {port.device}")
            try:
                if program_esp32(port.device):
                    print("Flashen erfolgreich!")
            except Exception as e:
                print(f"Fehler beim Flashen: {e}")


def watch_ports():
    print("Beobachte Ports...")
    while True:
        scan_ports()
        time.sleep(1)


#erstelle klasse, die std out beschreibt und alles in eine varriable speichert
class StdoutRedirector():
    def __init__(self):
        self.text = ""
    def write(self, string):
        self.text += string + "\n"
        sys.__stdout__.write(string)

    def flush(self):
        sys.__stdout__.flush()

    def getMac(self):
        mac = ""
        #MAC: a8:48:fa:c1:4f:0b
        tmp =  self.text.split("\n")
        for line in tmp:
            if "MAC:" in line:
                mac = line.split(" ")[1]
        return mac
    def isatty(self):
        return True


def main():
    #chekc if blacklist exist and if not create
    try:
        open("blacklist.txt", "x")
    except:
        pass
    download_images()
    watch_ports()


if __name__ == "__main__":
    main()

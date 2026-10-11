#!/usr/bin/env python3
"""
Pre-build script for PlatformIO LittleFS image generation (and standalone CLI).
Compresses web assets (.html, .js, .css, .ico, .svg) from data/ into .pio/data_gzip/*.gz
so they are stored gzip-compressed in LittleFS and served with Content-Encoding: gzip.
Keeps config.json and binary images (.png) uncompressed because firmware accesses
/config.json directly on LittleFS.

Standalone fast usage (skips ESP-IDF CMake overhead, <1s build):
  python tools/gzip_littlefs.py --build
  python tools/gzip_littlefs.py --flash COM19
"""
import argparse
import gzip
import os
import shutil
import subprocess
import sys

# Force UTF-8 on Windows consoles/pipes so PlatformIO's click.secho never crashes
# with UnicodeEncodeError (cp1252) when esptool v5 prints Unicode progress bars.
os.environ["PYTHONIOENCODING"] = "utf-8"
os.environ["PYTHONUTF8"] = "1"
for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        try:
            _stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass

GZIP_EXTENSIONS = {".html", ".htm", ".js", ".css", ".ico", ".svg"}
FS_OFFSET = "0x620000"
FS_SIZE = 0x180000  # 1572864 bytes


def prepare_gzip_data_dir(project_dir: str) -> str:
    src_dir = os.path.join(project_dir, "data")
    dst_dir = os.path.join(project_dir, ".pio", "data_gzip")

    if os.path.isdir(dst_dir):
        shutil.rmtree(dst_dir)
    os.makedirs(dst_dir, exist_ok=True)

    total_raw = 0
    total_out = 0

    for root, _, files in os.walk(src_dir):
        rel_root = os.path.relpath(root, src_dir)
        target_root = dst_dir if rel_root == "." else os.path.join(dst_dir, rel_root)
        os.makedirs(target_root, exist_ok=True)

        for fname in sorted(files):
            if fname.endswith(".gz"):
                continue
            src_path = os.path.join(root, fname)
            ext = os.path.splitext(fname)[1].lower()
            raw_bytes = open(src_path, "rb").read()
            raw_len = len(raw_bytes)
            total_raw += raw_len

            if ext in GZIP_EXTENSIONS:
                gz_bytes = gzip.compress(raw_bytes, compresslevel=9, mtime=0)
                dst_path = os.path.join(target_root, fname + ".gz")
                with open(dst_path, "wb") as f:
                    f.write(gz_bytes)
                total_out += len(gz_bytes)
            else:
                dst_path = os.path.join(target_root, fname)
                with open(dst_path, "wb") as f:
                    f.write(raw_bytes)
                total_out += raw_len

    ratio = (1.0 - (total_out / total_raw)) * 100.0 if total_raw > 0 else 0.0
    print(
        f"[gzip_littlefs] Prepared {dst_dir}: "
        f"{total_raw} B -> {total_out} B ({ratio:.1f}% saved)"
    )
    return dst_dir


def find_mklittlefs() -> str:
    pio_pkgs = os.path.expanduser("~/.platformio/packages")
    if os.path.isdir(pio_pkgs):
        for entry in sorted(os.listdir(pio_pkgs)):
            if entry.startswith("tool-mklittlefs"):
                for exe in ("mklittlefs.exe", "mklittlefs"):
                    candidate = os.path.join(pio_pkgs, entry, exe)
                    if os.path.isfile(candidate):
                        return candidate
    raise FileNotFoundError("mklittlefs not found in ~/.platformio/packages")


def build_littlefs_bin(project_dir: str, env_name: str = "esp32dev") -> str:
    gz_dir = prepare_gzip_data_dir(project_dir)
    out_dir = os.path.join(project_dir, ".pio", "build", env_name)
    os.makedirs(out_dir, exist_ok=True)
    out_bin = os.path.join(out_dir, "littlefs.bin")
    mklfs = find_mklittlefs()
    cmd = [mklfs, "-c", gz_dir, "-s", str(FS_SIZE), "-p", "256", "-b", "4096", out_bin]
    subprocess.run(cmd, check=True)
    print(f"[gzip_littlefs] Built {out_bin} ({os.path.getsize(out_bin)} bytes)")
    return out_bin


# PlatformIO SCons hook
try:
    Import("env")  # type: ignore[name-defined]
    from SCons.Script import COMMAND_LINE_TARGETS  # type: ignore[import-not-found]
    if any(t in COMMAND_LINE_TARGETS for t in ("buildfs", "uploadfs", "uploadfsota")):
        proj_dir = env.subst("$PROJECT_DIR")  # type: ignore[name-defined]
        gz_dir = prepare_gzip_data_dir(proj_dir)
        env.Replace(PROJECT_DATA_DIR=gz_dir)  # type: ignore[name-defined]
except NameError:
    if __name__ == "__main__":
        parser = argparse.ArgumentParser(description="Gzip web assets and optionally build/flash LittleFS")
        parser.add_argument("--build", action="store_true", help="Build littlefs.bin directly via mklittlefs (<1s)")
        parser.add_argument("--flash", metavar="PORT", help="Build and flash littlefs.bin to serial PORT")
        args = parser.parse_args()
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if args.build or args.flash:
            bin_path = build_littlefs_bin(repo_root)
            if args.flash:
                esptool = os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py")
                pio_py = os.path.expanduser("~/.platformio/penv/Scripts/python.exe")
                if not os.path.isfile(pio_py):
                    pio_py = os.path.expanduser("~/.platformio/penv/bin/python")
                py_exe = pio_py if os.path.isfile(pio_py) else sys.executable
                sub_env = os.environ.copy()
                sub_env["PYTHONIOENCODING"] = "utf-8"
                subprocess.run(
                    [
                        py_exe,
                        esptool,
                        "--chip", "esp32",
                        "--port", args.flash,
                        "--baud", "921600",
                        "--before", "default-reset",
                        "--after", "hard-reset",
                        "write-flash", "-z",
                        "--flash-mode", "dio",
                        "--flash-freq", "80m",
                        "--flash-size", "8MB",
                        FS_OFFSET, bin_path,
                    ],
                    env=sub_env,
                    check=True,
                )
        else:
            prepare_gzip_data_dir(repo_root)

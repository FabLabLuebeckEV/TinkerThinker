"""Spiegelt die letzten Releases für den Web-Flasher (GitHub Page) nach <ziel>/releases/.

GitHub-Release-Downloads sind im Browser per CORS gesperrt. Die Pipeline legt deshalb die
Binaries der letzten Releases direkt neben die Seite und schreibt eine index.json, aus der der
Web-Flasher seine Release-Auswahl baut.

Aufruf:  python tools/mirror_releases.py <ziel> [anzahl]
Benötigt die gh-CLI (in GitHub Actions mit GH_TOKEN, lokal mit gh auth login).
"""
import json
import os
import shutil
import subprocess
import sys

FILES = ["bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin"]


def gh(*args):
    result = subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError("gh " + " ".join(args) + " fehlgeschlagen: " + result.stderr.strip())
    return result.stdout


def main():
    if len(sys.argv) < 2:
        raise SystemExit("Aufruf: python tools/mirror_releases.py <ziel> [anzahl]")
    target = sys.argv[1]
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 15
    repo = os.environ.get("GITHUB_REPOSITORY")
    repo_args = ["--repo", repo] if repo else []

    releases = json.loads(gh("release", "list", *repo_args, "--limit", str(limit),
                             "--json", "tagName,name,publishedAt,isLatest,isDraft,isPrerelease"))
    out_dir = os.path.join(target, "releases")
    os.makedirs(out_dir, exist_ok=True)

    index = []
    for rel in releases:
        if rel.get("isDraft"):
            continue
        tag = rel["tagName"]
        dest = os.path.join(out_dir, tag)
        os.makedirs(dest, exist_ok=True)
        args = ["release", "download", tag, *repo_args, "-D", dest, "--clobber"]
        for name in FILES:
            args += ["-p", name]
        try:
            gh(*args)
        except RuntimeError as err:
            # Ein unvollständiges altes Release soll nicht das ganze Deployment verhindern,
            # wird aber sichtbar im Log gemeldet und nicht angeboten.
            print("Release " + tag + " übersprungen: " + str(err))
            shutil.rmtree(dest, ignore_errors=True)
            continue
        missing = [name for name in FILES if not os.path.isfile(os.path.join(dest, name))]
        if missing:
            print("Release " + tag + " übersprungen, es fehlen: " + ", ".join(missing))
            shutil.rmtree(dest, ignore_errors=True)
            continue
        index.append({
            "tag": tag,
            "name": rel.get("name") or tag,
            "published_at": rel.get("publishedAt"),
            "latest": bool(rel.get("isLatest")),
            "prerelease": bool(rel.get("isPrerelease")),
        })
        print("Gespiegelt: " + tag)

    if not index:
        raise SystemExit("Keine vollständigen Releases gefunden")
    with open(os.path.join(out_dir, "index.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, ensure_ascii=False, indent=1)
    print(str(len(index)) + " Releases gespiegelt")


if __name__ == "__main__":
    main()

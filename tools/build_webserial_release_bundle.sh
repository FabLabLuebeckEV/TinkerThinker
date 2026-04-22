#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

REPO_API="https://api.github.com/repos/FabLabLuebeckEV/TinkerThinker/releases/latest"
OUT_JS="$SCRIPT_DIR/webserial_release_bundle.js"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Fehlt: $1" >&2
    exit 1
  fi
}

require_cmd curl
require_cmd jq
require_cmd base64

echo "Lade Release-Metadaten..."
curl -fsSL "$REPO_API" -o "$TMP_DIR/release.json"

TAG_NAME="$(jq -r '.tag_name // "unknown"' "$TMP_DIR/release.json")"
PUBLISHED_AT="$(jq -r '.published_at // ""' "$TMP_DIR/release.json")"

declare -A OFFSETS=(
  ["bootloader.bin"]="0x1000"
  ["partitions.bin"]="0x8000"
  ["firmware.bin"]="0x20000"
  ["littlefs.bin"]="0x620000"
)

ASSET_JSON=()

for NAME in bootloader.bin partitions.bin firmware.bin littlefs.bin; do
  URL="$(jq -r --arg name "$NAME" '.assets[] | select(.name == $name) | .browser_download_url' "$TMP_DIR/release.json" | head -n1)"
  SIZE="$(jq -r --arg name "$NAME" '.assets[] | select(.name == $name) | .size' "$TMP_DIR/release.json" | head -n1)"

  if [[ -z "$URL" || "$URL" == "null" ]]; then
    echo "Asset fehlt im Release: $NAME" >&2
    exit 1
  fi

  echo "Lade $NAME..."
  curl -fL "$URL" -o "$TMP_DIR/$NAME"
  B64="$(base64 -w 0 "$TMP_DIR/$NAME")"
  OFFSET="${OFFSETS[$NAME]}"
  ASSET_JSON+=("  \"${NAME}\": {\"offset\": \"${OFFSET}\", \"size\": ${SIZE:-0}, \"dataBase64\": \"${B64}\"}")
done

{
  echo "window.TINKERTHINKER_RELEASE_BUNDLE = {"
  echo "  tag_name: $(jq -Rn --arg v "$TAG_NAME" '$v'),"
  echo "  published_at: $(jq -Rn --arg v "$PUBLISHED_AT" '$v'),"
  echo "  source: \"local_bundle\","
  echo "  assets: {"
  for i in "${!ASSET_JSON[@]}"; do
    if [[ "$i" -gt 0 ]]; then
      echo ","
    fi
    printf "%s" "${ASSET_JSON[$i]}"
  done
  echo
  echo "  }"
  echo "};"
} > "$OUT_JS"

echo "Bundle erstellt: $OUT_JS"
echo "Tag: $TAG_NAME"
echo "Die Datei webserial_installer.html kann jetzt direkt lokal geoeffnet werden."

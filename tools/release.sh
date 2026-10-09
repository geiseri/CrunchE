#!/bin/sh
# Local firmware release: build -> merge -> publish gh-pages -> tag.
# Usage: sh tools/release.sh <version> [--dry-run]   e.g. sh tools/release.sh 1.4.0
# Publishes the merged image + manifest to the gh-pages branch (GitHub Pages),
# which serves the browser flasher at tools/flasher/. No GitHub Releases needed
# (release assets have no CORS headers; Pages sends Access-Control-Allow-Origin: *).
set -e
cd "$(dirname "$0")/.."

V=${1:?usage: release.sh <version, e.g. 1.4.0>}
V=${V#v}
DRY=""
[ "${2:-}" = "--dry-run" ] && DRY=1

ENV=esp32-s3-super-mini
B=".pio/build/$ENV"
SITE="https://geiseri.github.io/CrunchE/flash/"
PIO="$HOME/.platformio/penv/bin/pio"
PY="$HOME/.platformio/penv/bin/python"
ESPTOOL="$PY -m esptool"
[ -x "$PIO" ] || PIO=pio
command -v "$PIO" >/dev/null 2>&1 || { echo "platformio not found"; exit 1; }
"$PY" -c "import esptool" 2>/dev/null || ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"

# ---- 1. build ------------------------------------------------------------
"$PIO" run -e "$ENV"
[ -f "$B/firmware.bin" ] || { echo "no $B/firmware.bin"; exit 1; }

# ---- 2. locate the framework's boot_app0 ----------------------------------
FW=""
for d in "$HOME"/.platformio/packages/framework-arduinoespressif32*; do
  [ -f "$d/tools/partitions/boot_app0.bin" ] && FW=$d && break
done
[ -n "$FW" ] || { echo "missing boot_app0.bin (PlatformIO Arduino core not installed?)"; exit 1; }
APP0="$FW/tools/partitions/boot_app0.bin"

# ---- 3. merge into one flashable image ------------------------------------
# Offsets are this project's verified flash map (see .pio .../idedata.json
# extra.flash_images + the max_app_4MB partition csv):
#   0x0 bootloader, 0x8000 partitions, 0xe000 boot_app0, app 0x10000.
# esptool-js cannot patch the flash header the way esptool does, so bake it
# now. memory_type is qio_qspi (not opi_*), so DIO per ESP Web Tools docs.
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
$ESPTOOL --chip esp32s3 merge_bin -o "$STAGE/merged.bin" \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0     "$B/bootloader.bin" \
  0x8000  "$B/partitions.bin" \
  0xe000  "$APP0" \
  0x10000 "$B/firmware.bin"

"$PY" - "$V" "$STAGE" <<'PY'
import json, sys
v, stage = sys.argv[1], sys.argv[2]
m = json.load(open("tools/flasher/manifest.template.json"))
m["version"] = v                       # part paths stay relative: merged.bin
json.dump(m, open(f"{stage}/manifest.json", "w"), indent=2)
print(f"merged.bin: {__import__('os').path.getsize(stage + '/merged.bin'):,} bytes")
PY

if [ -n "$DRY" ]; then
  echo "[dry-run] stopping before publish. Manifest:"
  cat "$STAGE/manifest.json"
  exit 0
fi

# ---- 4. publish to the gh-pages worktree ----------------------------------
if ! git rev-parse --verify -q gh-pages >/dev/null \
   && ! git rev-parse --verify -q origin/gh-pages >/dev/null; then
  echo "gh-pages branch missing. One-time setup (see plan Phase 4):"
  echo "  git switch --orphan gh-pages && git rm -rf --quiet ."
  echo "  mkdir -p flash firmware && touch .nojekyll"
  echo "  git add -A && git commit -m 'Pages: firmware artifact store'"
  echo "  git push origin gh-pages && git switch -"
  echo "Then enable Pages: Settings -> Pages -> Deploy from a branch -> gh-pages / (root)"
  exit 1
fi
WT=.gh-pages
[ -d "$WT" ] || git worktree add "$WT" gh-pages 2>/dev/null || \
  git worktree add "$WT" origin/gh-pages
git -C "$WT" checkout --force gh-pages 2>/dev/null || true

for D in "firmware/$V" firmware/latest; do
  mkdir -p "$WT/$D"
  cp "$STAGE/merged.bin" "$STAGE/manifest.json" "$WT/$D/"
done
mkdir -p "$WT/flash"
cp tools/flasher/index.html "$WT/flash/index.html"

git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then
  echo "gh-pages: no changes to publish"
else
  git -C "$WT" commit -m "firmware $V"
  git -C "$WT" push origin gh-pages
fi

# ---- 5. tag -----------------------------------------------------------------
git tag -a "v$V" -m "CrunchE $V" 2>/dev/null || echo "tag v$V already exists, skipping"
git push origin "v$V"

echo
echo "Published. Updater: $SITE"
echo "Users get $V after Pages deploys (~1 min; Pages caches ~10 min)."

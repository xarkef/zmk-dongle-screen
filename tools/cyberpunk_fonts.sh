#!/usr/bin/env bash
# Regenerate the cyberpunk theme's LVGL fonts. Run from the repo root.
# Needs node (npx) and python3 with fontTools (for Orbitron's weight axis).
set -euo pipefail
out=boards/shields/dongle_screen/src/fonts
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

python3 - "$tmp" <<'PY'
import sys
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
for name, wght in (("Bold", 700), ("Black", 900)):
    instantiateVariableFont(TTFont("tools/fonts/Orbitron.ttf"), {"wght": wght}).save(f"{sys.argv[1]}/Orbitron-{name}.ttf")
PY

conv() { # font range size name
    (cd "$(dirname "$1")" && npx -y lv_font_conv@1.5.2 --no-compress --no-prefilter --bpp 4 --format lvgl \
        --lv-include lvgl.h --force-fast-kern-format --font "$(basename "$1")" -r "$2" --size "$3" -o "$tmp/$4.c")
    mv "$tmp/$4.c" "$out/$4.c"
}
conv "$tmp/Orbitron-Bold.ttf" 0x20,0x30-0x39 22 cp_orbitron_22
conv "$tmp/Orbitron-Black.ttf" 0x20,0x2D,0x30-0x39,0x41-0x5A,0x5F 34 cp_orbitron_34
for size in 10 12 14; do
    conv tools/fonts/ShareTechMono.ttf 0x20-0x7E "$size" "cp_mono_$size"
done

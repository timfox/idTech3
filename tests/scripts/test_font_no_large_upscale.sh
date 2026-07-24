#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
SCR="$ROOT/runtime/client/shell/cl_scrn.c"

grep -q 'smallest raster that is not smaller' "$SCR"
grep -q 'SCR_TtfPointSizeForRefLineHeight' "$SCR"
grep -q 'FONT_GLYPH_UPSCALED' "$SCR"
grep -q 'r_fontMaxRasterUpscale' "$SCR"
echo "PASS: HUD tier selection prevents avoidable raster upscaling"

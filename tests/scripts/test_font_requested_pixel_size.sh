#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
SCR="$ROOT/runtime/client/shell/cl_scrn.c"

grep -q 'R_FontRequestedPixelHeight' "$SCR"
grep -q 'FONT_SIZE_PIXELS' "$SCR"
grep -q 'logicalSize \* uiScale' "$SCR"
echo "PASS: authoritative pixel/point font-size conversion"

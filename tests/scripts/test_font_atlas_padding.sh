#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
FONT="$ROOT/renderers/common/tr_font.c"

grep -q 'const int atlasPadding = 2;' "$FONT"
grep -q 'scaled_width + atlasPadding' "$FONT"
grep -q 'maxHeight + atlasPadding' "$FONT"
echo "PASS: font atlas uses two-pixel transparent padding"

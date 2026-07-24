#!/usr/bin/env bash
# Static check: BSP30 loader must sample the lighting lump (not white-only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/renderers/vulkan/tr_bsp30.c"
test -f "$SRC"
grep -q 'GS_SampleFaceLuxel' "$SRC"
grep -q 'BSP30_LUMP_LIGHTING' "$SRC"
grep -q 's_bsp30LitFaces' "$SRC"
# Must not be the old white-only bridge comment as the sole lighting path.
if grep -q 'initial bridge uses vertex-white lighting' "$SRC"; then
  echo "FAIL: stale white-vertex bridge comment still present" >&2
  exit 1
fi
echo "PASS: BSP30 lighting lump sampling present"

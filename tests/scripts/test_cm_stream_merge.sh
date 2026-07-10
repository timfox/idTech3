#!/usr/bin/env bash
# cm_stream_merge collision overlay smoke checks (sector BSP fixtures + merge API).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

GEN="$ROOT/scripts/tools/gen_sector_bsp.py"
FIXTURE_DIR="$ROOT/tests/data/openworld/maps"
FIXTURE="$FIXTURE_DIR/sector_0_0.bsp"

echo "[test_cm_stream_merge] checking sources..."
CM_MERGE="$(idtech3_require_file engine/core/cm_stream_merge.c src/qcommon/cm_stream_merge.c)"
idtech3_require_file engine/core/cm_stream_merge.h src/qcommon/cm_stream_merge.h >/dev/null
idtech3_require_file engine/core/cm_stream.c src/qcommon/cm_stream.c >/dev/null
test -f scripts/tools/gen_sector_bsp.py || { echo "missing gen_sector_bsp.py"; exit 1; }
WP="$(idtech3_require_file modules/world/world_proc.cpp src/world/world_proc.cpp)"
CL_OW="$(idtech3_require_file runtime/client/world/cl_openworld.cpp src/client/world/cl_openworld.cpp)"

echo "[test_cm_stream_merge] grep API symbols..."
rg -q 'CM_Stream_MergeSector' "$CM_MERGE"
rg -q 'CM_Stream_TraceMerged' "$CM_MERGE"
rg -q 'CM_Stream_PointContentsMerged' "$CM_MERGE"
rg -q 'cm_stream_merge.c' CMakeLists.txt

echo "[test_cm_stream_merge] generate sector BSP fixture..."
python3 "$GEN" "$FIXTURE" --cell-x 0 --cell-y 0 --visual
test -f "$FIXTURE"

echo "[test_cm_stream_merge] validate BSP header..."
python3 - "$FIXTURE" <<'PY'
import struct
import sys

path = sys.argv[1]
LUMP_SURFACES = 13
with open(path, "rb") as f:
    ident, version = struct.unpack("<II", f.read(8))
assert ident == (ord("P") << 24) + (ord("S") << 16) + (ord("B") << 8) + ord("I"), hex(ident)
assert version == 46, version
# LUMP_BRUSHES = 8, LUMP_BRUSHSIDES = 9
with open(path, "rb") as f:
    f.seek(8 + 8 * 8)
    brush_ofs, brush_len = struct.unpack("<II", f.read(8))
    f.seek(8 + 9 * 8)
    side_ofs, side_len = struct.unpack("<II", f.read(8))
    f.seek(8 + LUMP_SURFACES * 8)
    surf_ofs, surf_len = struct.unpack("<II", f.read(8))
assert brush_len == 12, brush_len  # one dbrush_t
assert side_len == 48, side_len    # six dbrushside_t
assert surf_len >= 104, surf_len   # one dsurface_t with --visual
print(f"ok: brushes@{brush_ofs} len={brush_len}, sides@{side_ofs} len={side_len}, surfaces@{surf_ofs} len={surf_len}")
PY

echo "[test_cm_stream_merge] demo + proc scatter wiring..."
test -f examples/demo_game/mod/maps/sector_0_0.bsp
rg -q 'WorldProc_FormatScatterRegionPath' "$WP"
rg -q 'r_procScatterRegion' "$WP"
rg -q 'WorldProc_FormatScatterRegionPath' "$CL_OW"

echo "[test_cm_stream_merge] ok"

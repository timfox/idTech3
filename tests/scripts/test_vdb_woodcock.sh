#!/usr/bin/env bash
# Wiring check: VDB Woodcock / mode-3 volumetric fog path stays documented and linked.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

FRAG="$(idtech3_file renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.frag src/renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.frag)"
COMP="$(idtech3_file renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.comp src/renderers/vulkan/shaders/glsl/volumetric/volumetric_fog.comp)"
VDB="$(idtech3_file renderers/vulkan/vk_vdb.c src/renderers/vulkan/vk_vdb.c)"
INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
DEVICE="$(idtech3_file renderers/vulkan/vk_init_device.c src/renderers/vulkan/vk_init_device.c)"
DOC="$ROOT/docs/VDB_WOODCOCK_VOLUMETRICS.md"
DEMO_CFG="$ROOT/examples/demo_game/mod/demo_vdb_woodcock.cfg"
DEMO_NVDB="$ROOT/examples/demo_game/bootstrap_media/vdb/fog_2cubed.nvdb"
FIXTURE_NVDB="$ROOT/tests/data/fog_2cubed.nvdb"
SIM_DOC="$ROOT/docs/SIM_RENDER_PROFILE.md"

grep -q 'integrateVdbWoodcockFog' "$FRAG"
grep -q 'binding = 10' "$FRAG"
grep -q 'binding = 11' "$FRAG"
grep -q 'vdbFogMajorant' "$FRAG"
grep -q 'volumetricFogIntegration.*3\|integration == 3\|== 3' "$FRAG"

grep -q 'binding = 17' "$COMP"
grep -q 'binding = 18' "$COMP"
grep -q 'vdbFogMajorant\|vdbFogDensity' "$COMP"

grep -q 'r_vdbMajorantBrick' "$VDB"
grep -q 'vdb_rebuild_majorant' "$VDB"
grep -q 'r_vdbFog' "$VDB"

grep -q 'volumetric_integration' "$INIT"
grep -q '0|1|2|3' "$INIT"

grep -q 'binding = 17' "$DEVICE"
grep -q 'binding = 18' "$DEVICE"

test -f "$DOC"
grep -q 'demo_vdb_woodcock.cfg' "$DOC"
grep -q 'r_volumetricFogIntegration 3' "$DOC"

test -f "$SIM_DOC"
grep -q 'Woodcock\|OpenVDB' "$SIM_DOC"
grep -q '| `3`' "$SIM_DOC"

test -f "$DEMO_CFG"
grep -q 'volumetric_integration 3' "$DEMO_CFG"
grep -q 'vdb_load vdb/fog_2cubed.nvdb' "$DEMO_CFG"
grep -q 'r_vdbMajorantBrick' "$DEMO_CFG"

test -f "$DEMO_NVDB"
test -f "$FIXTURE_NVDB"
cmp -s "$DEMO_NVDB" "$FIXTURE_NVDB"

grep -q 'fog_2cubed.nvdb' "$ROOT/examples/demo_game/CMakeLists.txt"
grep -q 'demo_vdb_woodcock.cfg' "$ROOT/examples/demo_game/CMakeLists.txt"
grep -q 'DEMO_BOOTSTRAP_VDB\|vdb/fog_2cubed.nvdb' "$ROOT/examples/demo_game/CMakeLists.txt"

# Optional: run unit_nanovdb_decode if present in a common build dir
for _build in "$ROOT/build-vk-Release" "$ROOT/build" "$ROOT/build-vk-Debug"; do
	if [[ -x "$_build/unit_nanovdb_decode" ]]; then
		"$_build/unit_nanovdb_decode"
		break
	fi
done

echo "test_vdb_woodcock.sh: ok"

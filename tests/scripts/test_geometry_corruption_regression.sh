#!/usr/bin/env bash
# Static + unit gates for exploding-triangle / BSP30 triangulation fixes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

TRI="$ROOT/renderers/common/tr_bsp30_triangulate.c"
VBO="$ROOT/renderers/vulkan/vk_vbo.c"
BSP="$ROOT/renderers/vulkan/tr_bsp.c"

grep -q 'Bsp30_FanFromHub' "$TRI" || fail 'hub-fan fallback missing'
grep -q 'R_Bsp30_TriangleCentroidInside' "$TRI" || fail 'centroid validation missing'
grep -q 'offset == ~0U' "$VBO" || fail 'soft IBO must reject ~0U upload'
grep -q 'soft_buffer_offset != ~0U' "$VBO" || fail 'soft IBO draw must reject ~0U bind'
grep -q 'Bad index in face surface' "$BSP" || fail 'Q3 face index range check missing'
grep -q 'geometry_corruption_status' "$ROOT/renderers/vulkan/vk_geometry_corruption.c" || fail 'geometry_corruption_status command'
grep -q 'r_geometryCorruptionDebug' "$ROOT/renderers/vulkan/vk_geometry_corruption.c" || fail 'r_geometryCorruptionDebug'
grep -q 'vk_geometry_corruption_allow_draw' "$ROOT/renderers/vulkan/vk_draw_state.c" || fail 'draw isolation hook'
[[ -f "$ROOT/docs/EXPLODING_GEOMETRY.md" ]] || fail 'docs/EXPLODING_GEOMETRY.md'

# Unit test if build tree present
if [[ -x "$ROOT/build-vk-Release/unit_bsp30_triangulate" ]] ||
   cmake --build "$ROOT/build-vk-Release" -j --target unit_bsp30_triangulate >/dev/null 2>&1; then
	ctest --test-dir "$ROOT/build-vk-Release" -R unit_bsp30_triangulate --output-on-failure || \
		fail 'unit_bsp30_triangulate'
else
	echo "WARN: skip ctest (build tree missing)"
fi

# Offline map audit when map + gcc available
MAP="$ROOT/../Surf/maps/surf_aztec.bsp"
[[ -f "$MAP" ]] || MAP="$ROOT/release/surf/maps/surf_aztec.bsp"
if [[ -f "$MAP" ]] && command -v gcc >/dev/null; then
	gcc -O2 -o /tmp/bsp30_map_tri \
		"$ROOT/tests/unit/test_bsp30_map_triangulate.c" \
		"$TRI" -I"$ROOT/renderers/common" -I"$ROOT/engine/core" -lm
	out="$(/tmp/bsp30_map_tri "$MAP")"
	echo "$out"
	echo "$out" | grep -q 'ear  ok=' || fail 'map audit did not run'
	# After hub search, exterior ear rate should be very low (allow a few pathological)
	bad="$(echo "$out" | sed -n 's/.*ear  ok=[0-9]* bad=\([0-9]*\).*/\1/p')"
	if [[ -n "$bad" && "$bad" -gt 20 ]]; then
		fail "surf_aztec still has $bad exterior triangulations (want <=20)"
	fi
fi

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All geometry_corruption_regression checks passed."

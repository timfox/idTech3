#!/usr/bin/env bash
# Regression: BSP30 / no-lightgrid maps must keep finite PBR LightDir and a
# loadable vulkan.so without incomplete WIP "contract" modules half-wired in.
#
# Symptom when broken: all-black 3D on surf_aztec while UI presents; OA Q3BSP OK.
# Magenta r_clear remains → SceneHDR never receives opaque color writes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "PASS: $*"; }

LIGHT="$ROOT/renderers/vulkan/tr_light.c"
FRAG="$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
BE="$ROOT/renderers/vulkan/tr_backend.c"
BF="$ROOT/renderers/vulkan/vk_black_frame.c"

[[ -f "$LIGHT" ]] || fail "missing tr_light.c"
[[ -f "$FRAG" ]] || fail "missing gen_frag.tmpl"

grep -q 'world->lightGridData == NULL' "$LIGHT" || \
	fail 'R_LightDirForPoint must handle missing light grid'
grep -q 'VectorCopy( normal, lightDir )' "$LIGHT" || \
	fail 'missing light grid must still write a finite lightDir (normal stand-in)'
pass 'R_LightDirForPoint writes finite dir without light grid'

grep -q 'sqrLightDist > 1e-12' "$FRAG" || \
	fail 'gen_frag must guard LightDir normalize against zero length'
grep -qi 'missing lightgrid\|LightDir at 0' "$FRAG" || \
	fail 'gen_frag should document BSP30 / zero LightDir NaN guard'
pass 'gen_frag safe-normalizes LightDir'

# Non-OIT world path must record SceneHDR writers / draw counts (diagnostics).
grep -q 'vk_black_frame_note_writer( "ForwardOpaque" )' "$BE" || \
	fail 'ForwardOpaque writer note missing'
awk '
  /else$/ { elseblk=1 }
  elseblk && /RB_RenderDrawSurfList\( cmd->drawSurfs/ { drew=1 }
  drew && /vk_black_frame_note_writer\( "ForwardOpaque" \)/ { ok=1; exit }
  END { exit (ok ? 0 : 1) }
' "$BE" || fail 'non-split / r_oit 0 path must note ForwardOpaque writer'
pass 'non-OIT path notes ForwardOpaque SceneHDR writer'

# Incomplete WIP modules must not be required includes of black_frame.
if grep -q 'vk_frame_contract.h' "$BF"; then
	fail 'vk_black_frame must not depend on incomplete vk_frame_contract.h'
else
	pass 'vk_black_frame free of incomplete frame_contract include'
fi

for f in vk_frame_contract.c vk_hdr_pipeline.c vk_depth_contract.c \
	vk_shadow_contract.c vk_renderer_perf.c vk_shading_compare.c \
	vk_indirect_light.c vk_reflection_hierarchy.c; do
	if [[ -f "$ROOT/renderers/vulkan/$f" ]]; then
		# Allowed only if self-contained; stubs that declare empty symbols are OK
		# if they do not pull missing APIs. Flag half-wired modules that call
		# undeclared helpers.
		if grep -qE 'vk_gpu_scene_generation|vk_black_frame_draw_count|GpuSceneObject' \
			"$ROOT/renderers/vulkan/$f"; then
			fail "incomplete WIP module present: $f (references undeclared APIs)"
		fi
	fi
done
pass 'no half-wired contract modules referencing missing GPU-scene APIs'

[[ -f "$ROOT/docs/BLACK_FRAME_REGRESSION.md" ]] || fail 'missing BLACK_FRAME_REGRESSION.md'
pass 'black-frame regression doc present'

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All BSP30 SceneHDR / LightDir checks passed."
exit 0

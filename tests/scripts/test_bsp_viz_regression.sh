#!/usr/bin/env bash
# Static gates: BSP visualization consumes production visibility + reversed-Z depth.
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
shade="${repo_root}/renderers/vulkan/tr_shade.c"
pipelines="${repo_root}/renderers/vulkan/vk_pipelines_persistent.c"
world="${repo_root}/renderers/vulkan/tr_world.c"
viz_c="${repo_root}/renderers/vulkan/vk_bsp_viz.c"
viz_h="${repo_root}/renderers/vulkan/vk_bsp_viz.h"
bsp="${repo_root}/renderers/vulkan/tr_bsp.c"
init="${repo_root}/renderers/vulkan/tr_init.c"

fail() {
	echo "test_bsp_viz_regression: $*" >&2
	exit 1
}

[[ -f "${viz_c}" ]] || fail "vk_bsp_viz.c missing"
[[ -f "${viz_h}" ]] || fail "vk_bsp_viz.h missing"

grep -Fq 'bspVisibilityFrame_t' "${viz_h}" || fail "bspVisibilityFrame_t missing"
grep -Fq 'vk_bsp_viz_effective_showtris' "${viz_c}" || fail "effective showtris helper missing"
grep -Fq 'r_bspVizThroughWalls' "${viz_c}" || fail "r_bspVizThroughWalls missing"
grep -Fq 'bsp_viz_status' "${viz_c}" || fail "bsp_viz_status command missing"
grep -Fq 'vk_bsp_viz_register' "${init}" || fail "bsp viz not registered in R_Init"
grep -Fq 'vk_bsp_viz_on_map_change' "${bsp}" || fail "map change does not invalidate viz"

# Visible-only wireframe must use the current viewport depth.  Reversed-Z
# DEPTH_RANGE_ZERO is 1.0 (nearest) and is reserved for explicit mode 2.
grep -Fq 'depthRange = ( showtrisMode == 2 ) ? DEPTH_RANGE_ZERO : DEPTH_RANGE_NORMAL;' "${shade}" ||
	fail "visible-only overlay does not retain geometry depth"
grep -Fq 'vk_bsp_viz_effective_showtris' "${shade}" ||
	fail "DrawTris/RB_EndSurface does not consult r_bspViz policy"
grep -Fq 'vk_draw_geometry( depthRange, qfalse );' "${shade}" ||
	fail "wireframe overlay can write depth"

# Normals default to depth-tested; through-walls only for mode >= 2.
grep -Fq 'r_shownormals->integer >= 2 || vk_bsp_viz_want_through_walls()' "${shade}" ||
	fail "DrawNormals still always through-walls"

# An omitted GLS_DEPTHTEST_DISABLE means vk_create_pipeline selects the
# renderer-wide reversed-Z GREATER_OR_EQUAL comparison.
state_line="$(grep -F 'state_bits = GLS_POLYMODE_LINE;' "${pipelines}" || true)"
[[ -n "${state_line}" ]] ||
	fail "DrawTris pipeline is not depth-tested without depth writes"

# Guard the architectural invariant: debug wireframe is emitted by RB_EndSurface
# from accepted draw batches, while the world list retains PVS/area, frustum,
# surface-cull, and view-generation dedup stages — and feeds bsp viz counters.
grep -Fq 'R_MarkLeaves ();' "${world}" ||
	fail "world draw list no longer applies authoritative PVS/area visibility"
grep -Fq 'vk_bsp_viz_note_mark_leaves' "${world}" ||
	fail "world MarkLeaves does not publish view leaf/cluster to bsp viz"
grep -Fq 'vk_bsp_viz_begin_frame' "${world}" ||
	fail "world surfaces do not reset bsp viz frame identity"
grep -Fq 'vk_bsp_viz_note_surface_duplicate' "${world}" ||
	fail "world draw list no longer reports surface dedup to bsp viz"
grep -Fq 'vk_bsp_viz_note_surface_accepted' "${world}" ||
	fail "world draw list no longer reports accepted surfaces to bsp viz"
grep -Fq 'vk_bsp_viz_note_leaf_frustum_reject' "${world}" ||
	fail "frustum rejects are not reported to bsp viz"
grep -Fq 'R_RecursiveWorldNode( tr.world->nodes, 15,' "${world}" ||
	fail "world draw list no longer applies authoritative frustum traversal"
grep -Fq 'if ( surf->viewCount == tr.viewCount )' "${world}" ||
	fail "world draw list no longer deduplicates surfaces per view"
grep -Fq 'if ( R_CullSurface( surf->data, surf->shader ) )' "${world}" ||
	fail "world draw list no longer applies production surface culling"

# Mode 4 / through-walls must be opt-in (default 0).
grep -Fq 'r_bspVizThroughWalls", "0"' "${viz_c}" ||
	fail "r_bspVizThroughWalls default is not 0"
grep -Fq 'r_bspViz", "0"' "${viz_c}" ||
	fail "r_bspViz default is not 0"

echo "test_bsp_viz_regression: PASS"

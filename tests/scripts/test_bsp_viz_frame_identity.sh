#!/usr/bin/env bash
# Frame identity / map-change invalidation for BSP viz.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
viz_c="${repo_root}/renderers/vulkan/vk_bsp_viz.c"
world="${repo_root}/renderers/vulkan/tr_world.c"
bsp="${repo_root}/renderers/vulkan/tr_bsp.c"

fail() { echo "test_bsp_viz_frame_identity: $*" >&2; exit 1; }

grep -Fq 'vk_bsp_viz_on_map_change' "${viz_c}" || fail "on_map_change missing"
grep -Fq 's_mapGeneration++' "${viz_c}" || fail "map generation not bumped"
grep -Fq 'vk_bsp_viz_begin_frame' "${world}" || fail "per-world-pass frame reset missing"
grep -Fq 'vk_bsp_viz_on_map_change' "${bsp}" || fail "RE_LoadWorldMap does not invalidate viz"

echo "test_bsp_viz_frame_identity: PASS"

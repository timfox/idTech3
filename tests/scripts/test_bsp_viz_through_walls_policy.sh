#!/usr/bin/env bash
# Through-walls BSP viz is opt-in only.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
viz_c="${repo_root}/renderers/vulkan/vk_bsp_viz.c"
shade="${repo_root}/renderers/vulkan/tr_shade.c"

fail() { echo "test_bsp_viz_through_walls_policy: $*" >&2; exit 1; }

grep -Fq 'r_bspVizThroughWalls", "0"' "${viz_c}" || fail "throughWalls default not 0"
grep -Fq 'r_bspViz && r_bspViz->integer >= 4' "${viz_c}" || fail "mode 4 not gated as through-walls"
grep -Fq 'showtrisMode == 2' "${shade}" || fail "through-walls depth not isolated to mode 2"

echo "test_bsp_viz_through_walls_policy: PASS"

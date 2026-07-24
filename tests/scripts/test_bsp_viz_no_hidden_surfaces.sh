#!/usr/bin/env bash
# Visible-only overlay must not independently walk/draw the full BSP.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
viz_c="${repo_root}/renderers/vulkan/vk_bsp_viz.c"
shade="${repo_root}/renderers/vulkan/tr_shade.c"
world="${repo_root}/renderers/vulkan/tr_world.c"

fail() { echo "test_bsp_viz_no_hidden_surfaces: $*" >&2; exit 1; }

# Overlay draws tess submitted by production passes only.
grep -Fq 'drawListSource=production_R_AddWorldSurfaces' "${viz_c}" ||
	fail "status does not claim production draw-list source"
grep -Fq 'vk_bsp_viz_effective_showtris' "${shade}" ||
	fail "overlay not driven by effective showtris (production batches)"
# No independent full-tree surface walk in the viz module (status string OK).
if grep -E 'R_RecursiveWorldNode\(|R_AddWorldSurface\(|firstmarksurface' "${viz_c}" >/dev/null; then
	fail "vk_bsp_viz.c must not independently traverse BSP surfaces"
fi
# Production path still owns visibility.
grep -Fq 'R_MarkLeaves' "${world}" || fail "MarkLeaves missing"
grep -Fq 'R_CullSurface' "${world}" || fail "CullSurface missing"

echo "test_bsp_viz_no_hidden_surfaces: PASS"

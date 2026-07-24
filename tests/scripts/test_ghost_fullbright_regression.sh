#!/usr/bin/env bash
# Regression gates for ghost/fullbright split diagnosis.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

"$ROOT/tests/scripts/test_bsp30_lightmap_vertex.sh"

test -f "$ROOT/config/ghost_fullbright_ref.cfg"
grep -q 'r_exposure_auto 0' "$ROOT/config/ghost_fullbright_ref.cfg"
grep -q 'r_historyQuarantine 8' "$ROOT/config/ghost_fullbright_ref.cfg"

grep -q 'vk_ghost_lighting_register' "$ROOT/renderers/vulkan/tr_init.c"
grep -q 'r_skyboxHDR_autoExposure' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
grep -q 'seta r_temporalSSR 0\|set r_temporalSSR 0' "$ROOT/config/surf.cfg" \
  || grep -q 'r_temporalSSR 0' "$ROOT/config/ghost_fullbright_ref.cfg"

echo "PASS: ghost/fullbright regression gates"

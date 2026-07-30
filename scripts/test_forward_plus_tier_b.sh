#!/usr/bin/env bash
# Tier B Forward+ mixed dlights wiring + checklist automation (no GPU required for wiring).
# Full GPU A/B still needs GAME_BASE + map; see docs/samples/renderer_regression/scenes/08_tier_b_mixed_dlights.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

FP_COMP="$ROOT/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp"
GEN="$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
TR_SHADE="$ROOT/renderers/vulkan/tr_shade.c"
TR_INIT="$ROOT/renderers/vulkan/tr_init.c"
SCENE="$ROOT/docs/samples/renderer_regression/scenes/08_tier_b_mixed_dlights.md"
MAPS="$ROOT/scripts/renderer_regression_maps.sh"

test -f "$FP_COMP" || fail "missing tile cull"
test -f "$SCENE" || fail "missing Tier B scene doc"
test -f "$MAPS" || fail "missing renderer_regression_maps.sh"

rg -q 'spot_frustum_tile_overlap|spotFrustumTileCull' "$FP_COMP" || fail "spot frustum cull missing"
rg -q 'lightVolumeDepthCull' "$FP_COMP" || fail "light volume depth cull missing"
rg -q 'forwardPlusHiZProbePad|hiZ' "$FP_COMP" || fail "Forward+ HiZ probe pad missing"
rg -q 'r_forwardPlusHiZ' "$TR_INIT" || fail "r_forwardPlusHiZ cvar missing"
rg -q 'r_forwardPlusHiZPyramid' "$TR_INIT" || fail "r_forwardPlusHiZPyramid cvar missing"
rg -q 'r_forwardPlusEnergyRenorm.*, "0"' "$TR_INIT" || rg -q 'r_forwardPlusEnergyRenorm", "0"' "$TR_INIT" || fail "EnergyRenorm default should be 0"
rg -q 'r_forwardPlusShade' "$TR_SHADE" || fail "Forward+ shade gate in tr_shade"
rg -q 'ProjectDlightTexture' "$TR_SHADE" || fail "projector path still present"
# Single-path: projector skipped when Forward+ shade owns
rg -q 'r_forwardPlusShade->value > 0.0f' "$TR_SHADE" || fail "single-path projector skip missing"
rg -q 'pbrForwardPlus.w > 1e-6' "$GEN" || fail "renorm only when EnergyRenorm > 0"
rg -q 'spotFrustumTileCull|lightVolumeDepthCull' "$SCENE" || fail "Tier B scene doc outdated"

echo "OK: Forward+ Tier B wiring (spot/volume/HiZ/single-path energy)"
echo "GPU: MAPS_EXTRA=rtest_mixed_dlights GAME_BASE=... $MAPS"

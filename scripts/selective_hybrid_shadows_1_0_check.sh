#!/usr/bin/env bash
# Selective Hybrid Shadows 1.0 static contracts.
# Does not change boot defaults; forbids FG / RT reflections in the SHS profile.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

echo "=== Selective Hybrid Shadows 1.0 check ==="

STABLE="$ROOT/config/modern_vulkan_stable.cfg"
MODERN="$ROOT/config/modern_vulkan.cfg"
PROFILE="$ROOT/config/vulkan_overlay_selective_hybrid_shadows.cfg"
DOC="$ROOT/docs/SELECTIVE_HYBRID_SHADOWS_1.0.md"
SHS_C="$ROOT/renderers/vulkan/vk_selective_sun_shadow.c"
SHS_H="$ROOT/renderers/vulkan/vk_selective_sun_shadow.h"
BACKEND="$ROOT/renderers/vulkan/tr_backend.c"
SHADE="$ROOT/renderers/vulkan/tr_shade.c"
HYB="$ROOT/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
RQ="$ROOT/renderers/vulkan/shaders/glsl/selective_hybrid/shs_sun_shadow.comp"
COMP="$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_composite.comp"

[[ -f "$STABLE" ]] || fail "missing modern_vulkan_stable.cfg"
grep -qE 'seta r_renderMode 2' "$STABLE" || fail "stable must remain mode 2"
grep -qE 'seta r_hybrid1 0' "$STABLE" || fail "stable must keep Hybrid1 off"
! grep -q 'r_selectiveHybridSunShadow 1' "$MODERN" "$STABLE" || fail "boot configs must not enable SHS"
pass "boot spine untouched"

[[ -f "$PROFILE" && -f "$DOC" && -f "$SHS_C" && -f "$SHS_H" ]] || fail "missing SHS sources/docs/profile"
grep -qE 'seta r_selectiveHybridSunShadow 1' "$PROFILE" || fail "profile must enable SHS"
grep -qE 'seta r_hybrid1_spec 0' "$PROFILE" || fail "profile must keep RT reflections off"
grep -qE 'seta r_hybrid1_diffuse 0' "$PROFILE" || fail "profile must keep RT GI/diffuse off"
grep -qE 'seta r_pathtrace 0' "$PROFILE" || fail "profile must keep pathtrace off"
grep -qE 'seta r_hybrid1_dlightShadows 0' "$PROFILE" || fail "profile must keep local RT shadows off"
! grep -qiE 'seta r_frameGen |seta r_dlssg |frame.?gen' "$PROFILE" || fail "profile must not enable frame generation"
pass "SHS profile: sun-only RT, no reflections/GI/PT/FG"

grep -q 'vk_shs_rt_owns_sun' "$BACKEND" "$SHADE" || fail "raster sun gate missing"
grep -q 'vk_shs_sun_only_rt' "$HYB" || fail "Hybrid1 sun-only dlight gate missing"
grep -q 'vk_shs_record_raw_ray_query' "$HYB" "$SHS_C" || fail "ray-query raw path missing"
grep -q 'rayQueryEXT' "$RQ" || fail "ray-query shader missing"
grep -q 'ignoreIntersectionEXT' "$ROOT/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_shadow.rahit" || fail "shadow any-hit missing"
grep -q 'RTX_PRIM_MATERIAL_FLAG_ALPHA_TEST' "$ROOT/renderers/vulkan/extensions/rtx/vk_rtx_bindless.h" || fail "alpha-test prim flag missing"
grep -q 'shsMode' "$COMP" || fail "composite SHS sun-term mode missing"
grep -q 'r_shsFailInject' "$SHS_C" || fail "fail inject missing"
grep -q 'sunShadow=' "$ROOT/renderers/vulkan/tr_init_diagnostics.inc" || fail "havenrp status sun owner missing"
pass "ownership router + raw RQ + composite + fail inject + status"

grep -q 'hist_shadow' "$HYB" || fail "dedicated shadow history missing"
grep -q 'taa_history' "$HYB" && fail "Hybrid1 must not reference taa_history" || true
pass "dedicated shadow history (not color TAA)"

echo "=== Selective Hybrid Shadows 1.0 check PASSED ==="
echo "GPU: exec vulkan_overlay_selective_hybrid_shadows.cfg; vid_restart"
echo "Recovery: exec modern_vulkan.cfg"

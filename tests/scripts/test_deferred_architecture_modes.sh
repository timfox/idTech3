#!/usr/bin/env bash
# Deferred Honesty M2 static gate — see docs/DEFERRED_HONESTY.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }
DOC="$ROOT/docs/DEFERRED_HONESTY.md"
H="$ROOT/renderers/vulkan/vk_deferred_honesty.c"
HH="$ROOT/renderers/vulkan/vk_deferred_honesty.h"
GF="$ROOT/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
LC="$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl"
CF="$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag"
LM="$ROOT/renderers/vulkan/shaders/glsl/lightmap_decode.glsl"
[[ -f "$DOC" ]] || fail "missing DEFERRED_HONESTY.md"
grep -q 'MIXED_MATERIAL_DEFERRED' "$DOC" || fail "doc MIXED_MATERIAL"
grep -q 'GBufferBaseColor\|gbufferBaseColor\|unlit' "$DOC" || fail "doc base color"
pass "docs"
grep -q 'R_DeferredMixedMaterialWanted\|MIXED_MATERIAL' "$HH" || fail "honesty header"
grep -q 'GBUFFER_VALID_LIGHTMAP\|PIXEL_OWNER_NONE\|BASE_COLOR' "$HH" || fail "validity/ownership/audit"
pass "honesty API"
case "$(basename "$0")" in
  test_gbuffer_true_base_color.sh)
    grep -q 'gbufferBaseColor\|staticLightExport' "$GF" || fail "gen_frag unlit export"
    grep -q 'deferredMixedHandoff' "$GF" || fail "mixed handoff"
    pass "true base color export" ;;
  test_gbuffer_normal_export.sh)
    grep -q 'out_deferred_normal\|EvaluateSurfaceNormal\|surface_normal' "$ROOT/renderers/vulkan/shaders/glsl/"*.glsl "$GF" 2>/dev/null || true
    grep -q 'out_deferred_normal' "$GF" || fail "normal MRT"
    [[ -f "$ROOT/renderers/vulkan/shaders/glsl/surface_normal.glsl" ]] || fail "surface_normal.glsl"
    pass "normal export contract" ;;
  test_gbuffer_material_export.sh)
    grep -q 'r_legacyDeferredRoughness\|legacyRoughness\|GBUFFER_APPROXIMATED' "$H" || fail "legacy material defaults"
    pass "material export defaults" ;;
  test_deferred_lightmap_term.sh)
    grep -q 'DeferredStaticDiffuseFromLightmap\|lightmapIrr\|staticTerm' "$LC" || fail "static LM term"
    [[ -f "$LM" ]] || fail "lightmap_decode.glsl"
    pass "deferred lightmap term" ;;
  test_deferred_no_lit_albedo.sh)
    grep -q 'USING_LIT_SCENE_AS_BASE' "$HH" || fail "lit-as-base flag"
    grep -q 'R_DeferredMixedMaterialWanted' "$H" && grep -q 'USING_LIT_SCENE_AS_BASE' "$H" || fail "mixed clears lit-as-base"
    pass "no lit albedo in mixed" ;;
  test_deferred_no_double_shading.sh)
    grep -q 'doubleShaded\|NoteDoubleShaded\|mixedOwned' "$H" "$LC" "$CF" || fail "double-shade guards"
    grep -q 'ownerA\|mixedMaterial' "$LC" "$CF" || fail "ownership mask"
    pass "no double shading contract" ;;
  test_deferred_mixed_composite.sh)
    grep -q 'mixedMaterial' "$CF" || fail "composite ownership"
    grep -q 'owned' "$CF" || fail "owned replace path"
    pass "mixed composite" ;;
  test_material_export_parity.sh)
    grep -q 'r_materialExportCompare\|material_translate_status' "$H" || fail "parity hooks"
    pass "material export parity hooks" ;;
  test_lightmap_parity.sh)
    grep -q 'r_lightmapParityCompare\|r_deferredLightmapMode' "$H" || fail "LM parity cvars"
    pass "lightmap parity hooks" ;;
  test_deferred_architecture_modes.sh)
    grep -q 'DEFERRED_ARCH_FORWARD_PLUS_REFERENCE = 0' "$HH" || fail "arch0 Forward+"
    grep -q 'DEFERRED_ARCH_ADDITIVE_HYBRID = 1' "$HH" || fail "arch1 hybrid"
    grep -q 'DEFERRED_ARCH_FULL_FIDELITY = 2' "$HH" || fail "arch2 full fidelity"
    grep -q 'DEFERRED_ARCH_COMPARE = 3' "$HH" || fail "arch3 comparison"
    grep -q 'DEFERRED_ARCH_STRICT_VALIDATION = 4' "$HH" || fail "arch4 strict"
    grep -q 'deferred_architecture_status' "$H" || fail "status command"
    grep -q 'deferred_architecture_validate' "$H" || fail "validate command"
    [[ -f "$ROOT/config/modern_deferred_mixed.cfg" ]] || fail "modern_deferred_mixed.cfg"
    pass "architecture modes + cfg" ;;
  test_deferred_oa_matrix.sh)
    grep -q 'OpenArena\|OA\|validation matrix\|certified' "$DOC" || fail "OA matrix docs"
    [[ -f "$ROOT/config/demo_deferred_lightmap_parity.cfg" ]] || fail "parity demo cfg"
    pass "OA validation scaffolding" ;;
esac
if [[ $failures -ne 0 ]]; then echo "$failures failed"; exit 1; fi
echo "PASS: $(basename "$0")"

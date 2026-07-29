#!/usr/bin/env bash
# Deferred Honesty M3 — sun BRDF + sky IBL + CSM primary-only + lobe routing
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

LC="$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl"
COMP="$ROOT/renderers/vulkan/shaders/glsl/deferred_lighting.comp"
GBUF="$ROOT/renderers/vulkan/vk_deferred_gbuffer.c"
HON="$ROOT/renderers/vulkan/vk_deferred_honesty.c"
DOC="$ROOT/docs/DEFERRED_HONESTY.md"
CORE="$ROOT/renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl"

grep -q 'sunDir\|sunRadiance' "$COMP" || fail "sun push fields in deferred_lighting.comp"
grep -q 'sunTerm\|sunDiffuse\|sunSpecular' "$LC" || fail "sun BRDF eval in common"
grep -q 'PbrEnergyCompensation\|clearcoat_lobe' "$LC" || fail "shared BRDF symbols in sun path"
grep -q 'primary = ( staticTerm + sunTerm ) \* sunVis\|staticTerm + sunTerm' "$LC" || fail "CSM on primary only"
grep -q 'sunDir\|sunRadiance' "$GBUF" || fail "CPU fills sun push"
grep -q 'r_deferredSunBrdf\|Milestone 3' "$DOC" || fail "M3 docs"
grep -q 'clearcoat_lobe' "$CORE" || fail "clearcoat_lobe in BRDF core"
pass "M3 sun BRDF + ownership wiring"

grep -q 'DEFERRED_HAS_IBL\|DeferredEvalSkyIBL\|brdfLutTex\|prefilterCube\|irradianceCube' "$LC" "$COMP" || fail "sky IBL in deferred shaders"
grep -q 'binding = 12\|binding = 13\|binding = 14\|iblFlags\|r_deferredIbl' "$GBUF" || fail "IBL descriptors / push"
grep -q 'r_deferredIbl' "$HON" "$DOC" || fail "IBL cvar / docs"
pass "M3 sky IBL wiring"

grep -q 'PBR_HAS_SHEEN' "$HON" || fail "sheen Forward+ gate"
grep -q 'PBR_HAS_CLEARCOAT' "$HON" || fail "clearcoat Forward+ when compact"
grep -q 'r_gbufferCompact' "$HON" || fail "clearcoat gate must key off compact (not mixed)"
pass "M3 lobe Forward+ routing"

grep -q 'binding = 15\|surfaceTex\|DEFERRED_HAS_SURFACE\|GBufferSurfaceData' "$COMP" "$LC" "$GBUF" "$DOC" || \
	fail "SurfaceData binding / docs"
grep -q 'lightmapMode\|lightmapDeluxeStrength' "$COMP" "$LC" "$GBUF" || fail "deferred lightmap mode push"
grep -q 'DeferredStaticDiffuseFromDeluxeApprox' "$LC" "$ROOT/renderers/vulkan/shaders/glsl/lightmap_decode.glsl" || fail "deferred deluxe approximation helper"
if grep -qE 'ownerBias|1024\.0' "$LC"; then fail "retired LM pack still in lighting"; fi
pass "M3 SurfaceData path"

if [[ $failures -ne 0 ]]; then
	echo "$failures failed"
	exit 1
fi
echo "PASS: test_deferred_lighting_parity.sh"
